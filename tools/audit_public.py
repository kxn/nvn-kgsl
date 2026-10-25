#!/usr/bin/env python3
"""Audit the exact Git candidate tree before publication.

This checks file boundaries and pinned, reviewed archives. It complements the
human provenance review documented in docs/SOURCE_AND_PUBLICATION_SCOPE.md.
It does not determine ownership or grant redistribution rights.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DENIED_SUFFIXES = {'.nso', '.nsp', '.nca', '.xci', '.apk', '.keystore', '.jks',
                   '.so', '.a', '.o', '.spv', '.bntx', '.bnsh', '.dds', '.ktx'}
SECRET = re.compile(rb'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----|'
                    rb'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}')
HEX_DUMP = re.compile(rb'hex=[0-9a-fA-F]{128,}|[0-9a-fA-F]{256,}')
PRIVATE_DEVICE = re.compile(rb'\b10\.(?:[0-9]{1,3}\.){2}[0-9]{1,3}(?::[0-9]+)?\b')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def paths():
    result = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others',
                             '--exclude-standard'], cwd=ROOT, check=True,
                            capture_output=True)
    return sorted(set(result.stdout.decode().rstrip('\0').split('\0')) - {''})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    policy_path = ROOT / 'publication-policy.json'
    policy = json.loads(policy_path.read_text())
    approved = policy['reviewed_archives']
    reviewed_binary = policy.get('reviewed_binary_files', {})
    failures, inventory, archives = [], {}, []

    def check_text(name, data):
        if SECRET.search(data):
            failures.append(name + ': credential marker')
        if HEX_DUMP.search(data):
            failures.append(name + ': long byte dump requires review')
        if PRIVATE_DEVICE.search(data):
            failures.append(name + ': private device identifier')

    for name in paths():
        p = ROOT / name
        if p.is_symlink():
            failures.append(name + ': symlinks are not published')
            continue
        if not p.is_file():
            failures.append(name + ': absent/nonregular candidate')
            continue
        data = p.read_bytes()
        inventory[name] = dict(bytes=len(data), sha256=digest(data))
        components = set(p.relative_to(ROOT).parts)
        if components & {'ExeFS', 'RomFS', '.local', '.local-state'}:
            failures.append(name + ': private directory')
        if p.suffix.lower() in DENIED_SUFFIXES:
            failures.append(name + ': private/generated artifact suffix')
        if data.startswith((b'NSO0', b'\x7fELF')) or data[512:516] in (b'NCA2', b'NCA3'):
            failures.append(name + ': executable/game container signature')
        if zipfile.is_zipfile(io.BytesIO(data)):
            entry = approved.get(name)
            if not entry or entry.get('sha256') != digest(data):
                failures.append(name + ': unreviewed or changed archive')
                continue
            members = []
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for info in z.infolist():
                    q = Path(info.filename)
                    if q.is_absolute() or '..' in q.parts or info.flag_bits & 1:
                        failures.append(name + ': unsafe/encrypted archive member')
                        continue
                    raw = z.read(info)
                    member = name + '!' + info.filename
                    if raw.startswith((b'NSO0', b'\x7fELF')):
                        failures.append(member + ': executable/game container')
                    if zipfile.is_zipfile(io.BytesIO(raw)):
                        failures.append(member + ': nested archive requires separate review')
                    if entry['kind'] == 'user-supplied-design-reference':
                        try:
                            raw.decode('utf-8')
                        except UnicodeDecodeError:
                            failures.append(member + ': nontext design member')
                        check_text(member, raw)
                    elif entry['kind'] != 'authored-gpu-readbacks':
                        failures.append(member + ': unsupported archive classification')
                    members.append(dict(path=info.filename, bytes=len(raw), sha256=digest(raw)))
            archives.append(dict(path=name, kind=entry['kind'], members=members))
        else:
            try:
                data.decode('utf-8')
            except UnicodeDecodeError:
                entry = reviewed_binary.get(name)
                if (not entry or entry.get('sha256') != digest(data) or
                        entry.get('kind') != 'authored-gpu-readback'):
                    failures.append(name + ': unreviewed binary file')
                continue
            check_text(name, data)
    missing = sorted(set(approved) - set(inventory))
    failures += [name + ': reviewed archive is missing' for name in missing]
    failures += [name + ': reviewed binary is missing'
                 for name in sorted(set(reviewed_binary) - set(inventory))]
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / '.local'):
        parser.error('write the audit output below ignored .local/')
    out.parent.mkdir(parents=True, exist_ok=True)
    result = dict(schema=1, passed=not failures, files=inventory, archives=archives,
                  failures=failures, scope='Exact Git candidate plus every reviewed ZIP member; human provenance review remains required')
    out.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(passed=result['passed'], files=len(inventory),
                          archives=len(archives), failures=failures)))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
