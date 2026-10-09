"""Apply context-free edits to hash-pinned external files, after full preflight."""
import hashlib
import json
from pathlib import Path, PurePosixPath


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def apply(tree, recipes):
    tree = Path(tree).resolve()
    pending = {}
    for recipe_path in recipes:
        doc = json.loads(Path(recipe_path).read_text(encoding='utf-8'))
        if doc.get('schema') != 1 or not isinstance(doc.get('files'), list) or not doc['files']:
            raise ValueError('Invalid edit recipe')
        seen = set()
        for row in doc['files']:
            name = row['path']
            parts = PurePosixPath(name)
            if parts.is_absolute() or '..' in parts.parts or ':' in name or '\\' in name or name in seen:
                raise ValueError('Unsafe or duplicate edit path')
            seen.add(name)
            target = tree / name
            if target.is_symlink() or tree not in target.resolve().parents:
                raise ValueError('Edit escapes build tree')
            raw = pending[target] if target in pending else (target.read_bytes() if target.exists() else None)
            if row['before_sha256'] != (sha(raw) if raw is not None else None):
                raise ValueError(f'External source differs from checkpoint: {name}')
            lines = [] if raw is None else raw.decode('utf-8').splitlines(keepends=True)
            result = []
            for instruction in row['lines']:
                if set(instruction) == {'line'}:
                    n = instruction['line']
                    if type(n) is not int or not 0 <= n < len(lines):
                        raise ValueError('Invalid local line reference')
                    result.append(lines[n])
                elif set(instruction) == {'span'}:
                    start, count = instruction['span']
                    if type(start) is not int or type(count) is not int or start < 0 or count < 1 or start + count > len(lines):
                        raise ValueError('Invalid local line span')
                    result.extend(lines[start:start+count])
                elif set(instruction) == {'port'}:
                    text = instruction['port']
                    if not isinstance(text, str) or not text.endswith('\n') or text.count('\n') != 1 or '\r' in text or '\0' in text:
                        raise ValueError('Invalid port addition')
                    result.append(text)
                else:
                    raise ValueError('Invalid edit instruction')
            changed = None if row.get('remove') else ''.join(result).encode('utf-8')
            if (sha(changed) if changed is not None else None) != row['after_sha256'] or (changed is None and result):
                raise ValueError(f'Edit result differs from checkpoint: {name}')
            pending[target] = changed
    for target, raw in pending.items():
        if target.with_name(target.name+'.twin-new').exists():
            raise ValueError('Existing staging file preserved')
    for target, raw in pending.items():
        if raw is None:
            target.unlink(missing_ok=True)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        stage = target.with_name(target.name+'.twin-new')
        with stage.open('xb') as stream:
            stream.write(raw)
        stage.replace(target)
    return len(pending)
