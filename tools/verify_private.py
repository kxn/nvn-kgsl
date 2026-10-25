#!/usr/bin/env python3
"""Verify a separately supplied private companion without publishing its data."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = root / 'MANIFEST.jsonl'
    meta = json.loads((root / 'MANIFEST.meta.json').read_text())
    with manifest.open('rb') as stream:
        actual = hashlib.file_digest(stream, 'sha256').hexdigest()
    if actual != meta['manifest_sha256']:
        raise SystemExit('private manifest hash mismatch')
    count = total = links = 0
    with manifest.open() as stream:
        for line in stream:
            entry = json.loads(line)
            p = root / entry['path']
            if Path(entry['path']).is_absolute() or '..' in Path(entry['path']).parts:
                raise SystemExit('invalid relative manifest path')
            if entry['kind'] == 'symlink':
                if not p.is_symlink() or not p.resolve(strict=False).is_relative_to(root):
                    raise SystemExit('private link absent or escapes companion: ' + entry['path'])
                if p.readlink().as_posix() != entry['target']:
                    raise SystemExit('private link changed: ' + entry['path'])
                links += 1
                continue
            if not p.is_file() or p.is_symlink() or p.stat().st_size != entry['bytes']:
                raise SystemExit('private file absent or size changed: ' + entry['path'])
            with p.open('rb') as source:
                if hashlib.file_digest(source, 'sha256').hexdigest() != entry['sha256']:
                    raise SystemExit('private file hash changed: ' + entry['path'])
            count += 1
            total += entry['bytes']
    if count != meta['regular_files'] or total != meta['regular_bytes'] or links != meta['symlinks']:
        raise SystemExit('private manifest count mismatch')
    print(json.dumps(dict(passed=True, files=count, bytes=total, symlinks=links,
                          scope='private copied content only; old deployment records remain historical')))


if __name__ == '__main__':
    main()
