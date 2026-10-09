"""Reviewed source and full-history gate; binary publication remains blocked."""
import argparse, hashlib, json, re, subprocess, sys
from pathlib import Path, PurePosixPath

ROOT=Path(__file__).resolve().parents[1]
MANIFEST='docs/review-manifest.json'
LIMIT=256*1024
MANIFEST_LIMIT=512*1024
REFERENCE='96f95e015af9f5f38e4a8aefca7cab29a78b030d'
EXTENSIONS={'.c','.h','.py','.md','.txt','.json','.toml','.lock','.yml'}
NAMES={'Makefile','.gitignore','.gitattributes'}
FORBIDDEN={'assets','intro_native','graphics','sound','romfs','upstream','build','work','dist','.git','__pycache__','node_modules','.codex','.agents'}

def git(root,*args):
    return subprocess.check_output(['git','-C',str(root),*args])

def validate_name(name):
    p=PurePosixPath(name)
    if p.is_absolute() or '..' in p.parts or '\\' in name or ':' in name or any(x.lower() in FORBIDDEN for x in p.parts):
        raise ValueError(f'Forbidden source path: {name}')
    if p.name not in NAMES and p.suffix not in EXTENSIONS:
        raise ValueError(f'Unapproved source file type: {name}')

def check(files,modes=None,windows=None,supplements=()):
    doc=json.loads(files[MANIFEST])
    if doc.get('schema')!=1 or doc.get('technical_reference')!=REFERENCE or doc.get('binary_release_approved') is not False:
        raise ValueError('Unapproved reference or release policy')
    entries={}
    for row in doc['files']:
        name=row['path']
        if name in entries or name==MANIFEST or row.get('review_status')!='reviewed':
            raise ValueError('Duplicate or unreviewed manifest entry')
        if not all(row.get(k) for k in ('credit','license','basis')) or not re.fullmatch('[0-9a-f]{64}',row.get('sha256','')):
            raise ValueError('Incomplete provenance record')
        entries[name]=row
    # Retrospective review only supplies a missing record, never replaces a
    # historical review or changes the snapshot's bytes/policy. All ordinary
    # content, path, mode, size and optional ROM checks below still apply.
    # A superseding review names the exact stale hash the snapshot's own record
    # holds; it repairs only a record that does not prove that snapshot's bytes.
    for row in supplements:
        name=row['path']
        if name==MANIFEST or name not in files:
            raise ValueError('Historical review must supply an absent record')
        if 'supersedes' in row:
            raw=files[name]; canonical=raw.replace(b'\r\n',b'\n')
            proven={hashlib.sha256(x).hexdigest() for x in (raw,canonical,canonical.replace(b'\n',b'\r\n'))}
            if name not in entries or entries[name]['sha256']!=row['supersedes'] or row['supersedes'] in proven:
                raise ValueError('Superseding review must name a stale record')
        elif name in entries:
            raise ValueError('Historical review must supply an absent record')
        if row.get('review_status')!='reviewed' or not all(row.get(k) for k in ('credit','license','basis')):
            raise ValueError('Incomplete historical provenance')
        if not re.fullmatch('[0-9a-f]{64}',row.get('sha256','')):
            raise ValueError('Invalid historical review hash')
        if hashlib.sha256(files[name]).hexdigest()!=row['sha256']:
            raise ValueError('Historical review requires exact blob bytes')
        entries[name]=row
    if set(files)!=set(entries)|{MANIFEST}:
        raise ValueError('Unreviewed or missing files: '+str(sorted(set(files)^(set(entries)|{MANIFEST}))))
    for name,raw in files.items():
        validate_name(name)
        # Governance metadata grows with the reviewed file inventory. Its
        # bound is separate; every source file keeps the original 256 KiB cap.
        limit=MANIFEST_LIMIT if name==MANIFEST else LIMIT
        if len(raw)>limit or b'\0' in raw: raise ValueError(f'Binary or oversized file: {name}')
        text=raw.decode('utf-8')
        if re.search(r'[0-9a-fA-F]{512,}|data:(?:image|audio)/[^;]+;base64,|["\'][A-Za-z0-9+/]{512,}={0,2}["\']',text):
            raise ValueError(f'Encoded payload: {name}')
        if re.search(r'^\s*\.incbin\s+["\']|^\s*(?:static\s+)?(?:const\s+)?(?:u8|u16|u32|uint8_t|uint16_t|uint32_t)\b[^;\n]*=\s*INCBIN_',text,re.M):
            raise ValueError(f'Embedded payload directive: {name}')
        if modes and modes.get(name)!='100644': raise ValueError(f'Non-source Git mode: {name}')
        if name!=MANIFEST:
            # Git's text/eol=lf rule normalizes reviewed Windows files on add.
            # Canonical LF is the current manifest format. Older reviewed
            # snapshots may record CRLF bytes; prove that exact equivalence,
            # never ignore a content difference or permit arbitrary whitespace.
            canonical=raw.replace(b'\r\n',b'\n')
            expected=entries[name]['sha256']
            valid=hashlib.sha256(canonical).hexdigest()==expected
            if doc.get('hash_policy') is None:
                valid=valid or hashlib.sha256(canonical.replace(b'\n',b'\r\n')).hexdigest()==expected
            if not valid: raise ValueError(f'Changed without review: {name}')
        if windows and any(raw[n:n+64] in windows for n in range(len(raw)-63)):
            raise ValueError(f'Exact ROM fragment: {name}')
    return len(files)

def snapshot(root,commit=None):
    files,modes={},{}
    if commit:
        for record in git(root,'ls-tree','-rz',commit).split(b'\0'):
            if not record: continue
            metadata,path=record.split(b'\t',1)
            mode,kind,blob=metadata.decode().split()
            if kind!='blob': raise ValueError('Non-source Git object')
            name=path.decode(); validate_name(name)
            limit=MANIFEST_LIMIT if name==MANIFEST else LIMIT
            if int(git(root,'cat-file','-s',blob))>limit: raise ValueError('Oversized historical blob')
            files[name]=git(root,'cat-file','blob',blob); modes[name]=mode
    else:
        for record in git(root,'ls-files','--stage','-z').split(b'\0'):
            if not record: continue
            metadata,path=record.split(b'\t',1)
            mode,blob,stage=metadata.decode().split()
            if stage!='0': raise ValueError('Unresolved index conflict')
            modes[path.decode()]=mode
        names=set(modes)|{x.decode() for x in git(root,'ls-files','--others','--exclude-standard','-z').split(b'\0') if x}
        for name in names:
            validate_name(name); path=root/name
            if path.is_symlink() or root.resolve() not in path.resolve().parents: raise ValueError('Source path escapes checkout')
            files[name]=path.read_bytes(); modes.setdefault(name,'100644')
    return files,modes

def history(root,windows=None):
    if git(root,'rev-parse','--is-shallow-repository').strip()!=b'false': raise ValueError('Full history required')
    commits=git(root,'rev-list','--all').decode().splitlines()
    if not commits: raise ValueError('No committed history')
    current,modes=snapshot(root)
    check(current,modes,windows)
    reviews=json.loads(current[MANIFEST]).get('historical_reviews',[])
    by_commit={}; seen=set()
    for row in reviews:
        commit=row.get('commit',''); name=row.get('path','')
        if not re.fullmatch('[0-9a-f]{40}',commit) or commit not in commits:
            raise ValueError('Historical review must name a reachable full commit')
        if (commit,name) in seen: raise ValueError('Duplicate historical review')
        seen.add((commit,name)); by_commit.setdefault(commit,[]).append(row)
    for commit in commits:
        check(*snapshot(root,commit),windows,supplements=by_commit.get(commit,()))
    return len(commits)

DECISION='docs/release-decision.json'
DECISION_KEYS={'schema','kind','decided_by','date','release_tag','rights_verified','third_party_authorization','statement','assets'}

def release_gate(root,assets=()):
    """Binary publication gate. Returns (exit status, message).

    Open only when the owner's recorded decision names the exact binaries by SHA-256. The decision is an
    owner risk acceptance: it never states or implies that third-party rights are verified, and
    `binary_release_approved` stays false because that flag is reserved for verified clearance."""
    path=root/DECISION
    if not path.is_file():
        return 2,'BINARY RELEASE BLOCKED: no owner release decision (docs/release-decision.json); binary audit, ROM scan, linked licenses and hardware acceptance not approved'
    try:
        doc=json.loads(path.read_text(encoding='utf-8'))
        if set(doc)!=DECISION_KEYS or doc['schema']!=1 or doc['kind']!='owner-risk-acceptance':
            raise ValueError('unexpected fields or kind')
        if doc['rights_verified'] is not False:
            raise ValueError('an owner decision may not claim verified rights')
        if not all(isinstance(doc[k],str) and doc[k] for k in ('decided_by','date','release_tag','third_party_authorization','statement')):
            raise ValueError('incomplete decision')
        if not re.fullmatch(r'v\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?',doc['release_tag']):
            raise ValueError('invalid release tag')
        listed=doc['assets']
        if not isinstance(listed,dict) or not listed or not all(re.fullmatch('[0-9a-f]{64}',h) for h in listed.values()):
            raise ValueError('assets must map file names to SHA-256 hashes')
        for asset in assets:
            digest=hashlib.sha256(Path(asset).read_bytes()).hexdigest()
            if listed.get(Path(asset).name)!=digest:
                raise ValueError(f'{Path(asset).name} is not the binary named in the decision')
    except (ValueError,KeyError,OSError,UnicodeError) as exc:
        return 2,f'BINARY RELEASE BLOCKED: invalid owner release decision: {exc}'
    return 0,(f"RELEASE ALLOWED BY OWNER DECISION ({doc['kind']}, {doc['decided_by']}, {doc['date']}, {doc['release_tag']}): "
              'third-party rights are NOT independently verified; this is not a clearance')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,default=ROOT)
    ap.add_argument('--history',action='store_true')
    ap.add_argument('--release',action='store_true')
    ap.add_argument('--asset',type=Path,action='append',default=[],help='with --release: a binary that must match the owner decision')
    ap.add_argument('--rom',type=Path,help='optional own-ROM scan; never uploaded')
    args=ap.parse_args()
    if args.release:
        status,message=release_gate(args.root,args.asset)
        print(message,file=sys.stderr if status else sys.stdout)
        return status
    windows=None
    if args.rom:
        sys.path.insert(0,str(ROOT/'builder'))
        from firered3ds_builder.rom import load_rom
        raw=load_rom(args.rom).data
        windows={raw[n:n+64] for n in range(0,len(raw)-63,64) if len(set(raw[n:n+64]))>4}
    files,modes=snapshot(args.root)
    result={'reviewed_files':check(files,modes,windows),'binary_release_approved':False,'rom_scan':windows is not None}
    if args.history: result['audited_commits']=history(args.root,windows)
    print(json.dumps(result,indent=2))
    return 0

if __name__=='__main__':
    try: sys.exit(main())
    except (ValueError,KeyError,OSError,UnicodeError,subprocess.CalledProcessError) as exc: sys.exit(f'source audit: {exc}')
