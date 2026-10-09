"""Rejection tests use newly generated synthetic files and new temporary Git history."""
import hashlib, json, subprocess, tempfile, unittest
from pathlib import Path
import source_audit as audit

def reviewed(files):
    doc={'schema':1,'technical_reference':audit.REFERENCE,'binary_release_approved':False,'files':[]}
    for name,raw in files.items():
        doc['files'].append({'path':name,'sha256':hashlib.sha256(raw).hexdigest(),
            'credit':'Synthetic test author','license':'MIT','basis':'Generated synthetic fixture','review_status':'reviewed'})
    return dict(files,**{audit.MANIFEST:json.dumps(doc).encode()})

class Rejections(unittest.TestCase):
    def test_inventory_manifest_has_a_separate_bounded_size(self):
        files=reviewed({'README.md':b'a\n'})
        doc=json.loads(files[audit.MANIFEST])
        doc['review_note']='Reviewed inventory metadata. ' * 10000
        files[audit.MANIFEST]=json.dumps(doc).encode()
        self.assertGreater(len(files[audit.MANIFEST]),audit.LIMIT)
        self.assertLess(len(files[audit.MANIFEST]),audit.MANIFEST_LIMIT)
        self.assertEqual(audit.check(files),2)
        doc['review_note']='Reviewed inventory metadata. ' * 20000
        files[audit.MANIFEST]=json.dumps(doc).encode()
        with self.assertRaisesRegex(ValueError,'oversized'):
            audit.check(files)

    def test_manifest_size_allowance_does_not_apply_to_source(self):
        files=reviewed({'README.md':b'Reviewed source text.\n' * 14000})
        self.assertGreater(len(files['README.md']),audit.LIMIT)
        self.assertLess(len(files['README.md']),audit.MANIFEST_LIMIT)
        with self.assertRaisesRegex(ValueError,'oversized'):
            audit.check(files)

    def test_exact_missing_historical_review_only(self):
        complete=reviewed({'README.md':b'a\n','fix.py':b'print(1)\n'})
        doc=json.loads(complete[audit.MANIFEST])
        row=doc['files'].pop()
        files=dict(complete); files[audit.MANIFEST]=json.dumps(doc).encode()
        with self.assertRaises(ValueError): audit.check(files)
        self.assertEqual(audit.check(files,supplements=[row]),3)
        with self.assertRaises(ValueError): audit.check(complete,supplements=[row])
        for raw in (b'print(2)\n',b'print(1)\r\n'):
            changed=dict(files); changed['fix.py']=raw
            with self.assertRaises(ValueError): audit.check(changed,supplements=[row])
        with self.assertRaises(ValueError): audit.check(files,supplements=[row,row])
        with self.assertRaises(ValueError): audit.check(files,windows={b'print(1)\n'.ljust(64,b'x')},
            supplements=[dict(row,sha256='0'*64)])
        for name,raw in [('rom.gba',b'abc'),('bad.py',b'\0')]:
            valid=reviewed({name:raw}); manifest=json.loads(valid[audit.MANIFEST])
            bad=manifest['files'].pop(); valid[audit.MANIFEST]=json.dumps(manifest).encode()
            with self.assertRaises(ValueError): audit.check(valid,supplements=[bad])

    def test_retrospective_history_is_commit_scoped(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def git(*args): return audit.git(root,*args)
            git('init','-q'); git('config','user.name','Synthetic'); git('config','user.email','synthetic@example.invalid')
            files=reviewed({'README.md':b'a\n','fix.py':b'print(1)\n'})
            doc=json.loads(files[audit.MANIFEST]); row=doc['files'].pop()
            files[audit.MANIFEST]=json.dumps(doc).encode()
            for name,raw in files.items():
                p=root/name; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(raw)
            git('add','.'); git('commit','-qm','missing record')
            commit=git('rev-parse','HEAD').decode().strip()
            # A present-day review repairs that snapshot only.
            doc['files'].append(row); doc['historical_reviews']=[dict(row,commit=commit)]
            (root/audit.MANIFEST).write_text(json.dumps(doc))
            git('add','.'); git('commit','-qm','exact retrospective review')
            self.assertEqual(audit.history(root),2)
            for records in ([dict(row,commit='0'*40)], [dict(row,commit=commit,sha256='0'*64)],
                            [dict(row,commit=commit)]*2):
                doc['historical_reviews']=records
                (root/audit.MANIFEST).write_text(json.dumps(doc))
                with self.assertRaises(ValueError): audit.history(root)

    def test_exact_superseding_historical_review_only(self):
        # A snapshot whose file changed but whose record still names the old
        # bytes: a review may supersede that exact stale record, nothing else.
        old=reviewed({'README.md':b'a\n','fix.py':b'print(1)\n'})
        files=dict(old); files['fix.py']=b'print(2)\n'
        stale=json.loads(old[audit.MANIFEST])['files'][1]
        row=dict(stale,sha256=hashlib.sha256(b'print(2)\n').hexdigest(),supersedes=stale['sha256'])
        with self.assertRaisesRegex(ValueError,'Changed without review'): audit.check(files)
        self.assertEqual(audit.check(files,supplements=[row]),3)
        bad_rows=[dict(row,supersedes='0'*64),                       # not the snapshot's record
                  dict(row,sha256=stale['sha256']),                   # not the snapshot's bytes
                  {k:v for k,v in row.items() if k!='supersedes'},    # a plain record cannot replace
                  dict(row,review_status='pending'), dict(row,basis=''),
                  dict(row,path=audit.MANIFEST), dict(row,path='other.py')]
        for bad in bad_rows:
            with self.assertRaises(ValueError): audit.check(files,supplements=[bad])
        # Never overrides a record that already proves the bytes.
        current=dict(row,sha256=stale['sha256'],supersedes=stale['sha256'])
        with self.assertRaises(ValueError): audit.check(old,supplements=[current])
        with self.assertRaises(ValueError): audit.check(files,supplements=[row,row])
        # The superseded bytes still face every content check.
        payload=b'x=' + b'ab' * 300 + b'\n'
        files2=dict(old); files2['fix.py']=payload
        with self.assertRaisesRegex(ValueError,'Encoded payload'):
            audit.check(files2,supplements=[dict(row,sha256=hashlib.sha256(payload).hexdigest())])

    def test_superseding_review_is_commit_scoped(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def git(*args): return audit.git(root,*args)
            git('init','-q'); git('config','user.name','Synthetic'); git('config','user.email','synthetic@example.invalid')
            files=reviewed({'README.md':b'a\n','fix.py':b'print(1)\n'})
            for name,raw in files.items():
                p=root/name; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(raw)
            git('add','.'); git('commit','-qm','reviewed')
            (root/'fix.py').write_bytes(b'print(2)\n')
            git('add','.'); git('commit','-qm','changed, record forgotten')
            commit=git('rev-parse','HEAD').decode().strip()
            doc=json.loads(files[audit.MANIFEST]); stale=doc['files'][1]
            new=hashlib.sha256(b'print(2)\n').hexdigest()
            doc['files'][1]=dict(stale,sha256=new)
            doc['historical_reviews']=[dict(stale,sha256=new,supersedes=stale['sha256'],commit=commit)]
            (root/audit.MANIFEST).write_text(json.dumps(doc))
            git('add','.'); git('commit','-qm','record and superseding review')
            self.assertEqual(audit.history(root),3)
            first=git('rev-list','--max-parents=0','HEAD').decode().strip()
            for records in ([dict(doc['historical_reviews'][0],commit=first)], []):
                doc['historical_reviews']=records
                (root/audit.MANIFEST).write_text(json.dumps(doc))
                with self.assertRaises(ValueError): audit.history(root)

    def test_only_git_newline_normalization_is_equivalent(self):
        files=reviewed({'README.md':b'reviewed\r\ntext\r\n'})
        files['README.md']=b'reviewed\ntext\n'
        self.assertEqual(audit.check(files),2)
        files['README.md']=b'reviewed \ntext\n'
        with self.assertRaises(ValueError): audit.check(files)
    def test_valid_source(self):
        self.assertEqual(audit.check(reviewed({'README.md':b'Synthetic source\n'})),2)
    def test_missing_and_changed_review(self):
        files=reviewed({'README.md':b'a\n'})
        for name,raw in [('README.md',b'b\n'),('extra.py',b'print(1)\n')]:
            changed=files.copy(); changed[name]=raw
            with self.assertRaises(ValueError): audit.check(changed)
    def test_payload_paths_binary_and_modes(self):
        for name,raw in [('rom.gba',b'abc'),('romfs/x.txt',b'abc'),('data.py',b'\0'),('data.py',b'a'*(audit.LIMIT+1)),('data.py',b'"'+b'A'*600+b'"')]:
            with self.assertRaises(ValueError): audit.check(reviewed({name:raw}))
        files=reviewed({'data.py':b'print(1)\n'})
        with self.assertRaises(ValueError): audit.check(files,{name:'100755' for name in files})
    def test_source_directive_rejected(self):
        with self.assertRaises(ValueError): audit.check(reviewed({'data.c':b'const u8 game[] = INCBIN_U8("x.bin");\n'}))
    def test_local_rom_window(self):
        raw=bytes(range(32,96))
        with self.assertRaises(ValueError): audit.check(reviewed({'test.txt':raw}),windows={raw})
    def test_policy_cannot_be_enabled(self):
        files=reviewed({'README.md':b'a\n'})
        doc=json.loads(files[audit.MANIFEST]); doc['binary_release_approved']=True
        files[audit.MANIFEST]=json.dumps(doc).encode()
        with self.assertRaises(ValueError): audit.check(files)
    def test_deleted_payload_still_rejected_in_history(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def git(*args): return audit.git(root,*args)
            git('init','-q'); git('config','user.name','Synthetic'); git('config','user.email','synthetic@example.invalid')
            for name,raw in reviewed({'rom.gba':b'synthetic payload'}).items():
                path=root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(raw)
            git('add','.'); git('commit','-qm','synthetic bad fixture')
            (root/'rom.gba').unlink()
            for name,raw in reviewed({'README.md':b'clean synthetic\n'}).items():
                path=root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(raw)
            git('add','-A'); git('commit','-qm','synthetic deletion')
            with self.assertRaises(ValueError): audit.history(root)
    def test_binary_policy_exit_is_explicit(self):
        result=subprocess.run([__import__('sys').executable,str(Path(audit.__file__)),'--release'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertIn('BINARY RELEASE BLOCKED',result.stderr)

if __name__=='__main__': unittest.main()
