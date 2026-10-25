#!/usr/bin/env python3
"""Generate/verify D2R NVN lookup names from the pinned unmodified main NSO.

Reuses the original NSO and NVN-slot tools; no binary rewrite or CPU execution.
Only name/slot metadata and provenance remain in the ignored output directory.
The generated X-macro header supplies typed forwarding signatures and arguments
to host_bridge.c. Use --verify
before consuming cached output; it re-extracts from the original input and
checks tool/source fingerprints and all artifact bytes, not just the record.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from workspace_paths import d2r_root, checkout_identity

INPUT_SHA = '55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e'
PROFILE = 'd2r-3.3.94012-main-nvn-loader-0x1739290'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def command_identity(commands):
    """Preserve tools/options and input paths, normalize only temporary paths."""
    if len(commands) != 2 or len(commands[0]) != 4 or len(commands[1]) != 5:
        raise ValueError('unexpected extraction command shape')
    normalized = [list(command) for command in commands]
    normalized[0][3] = '<temporary-main.raw>'
    normalized[1][2:] = ['<temporary-main.raw>', '<temporary-slots.tsv>', '<temporary-names.txt>']
    return normalized


def extract(nso, out, root, d2r=None):
    d2r = d2r_root(d2r)
    data = nso.read_bytes()
    if data[:4] != b'NSO0' or sha(data) != INPUT_SHA:
        raise ValueError('expected the pinned unmodified original main NSO')
    commands = []
    # The original tools require filenames. Temporary raw game bytes are
    # removed before returning, including on an error; none are delivered.
    with tempfile.TemporaryDirectory(prefix='nvn-names-', dir=out.parent) as tmp:
        tmp = Path(tmp)
        raw_path, slots_path, names_path = (tmp / n for n in ('main.raw', 'slots.tsv', 'names.txt'))
        command = [sys.executable, str(d2r / 'dm/tools/nso.py'), str(nso), str(raw_path)]
        subprocess.run(command, check=True, capture_output=True, text=True)
        commands.append(command)
        raw = raw_path.read_bytes()
        segments = []
        for i in range(3):
            _, offset, size = struct.unpack_from('<III', data, 0x10 + i * 16)
            actual = sha(raw[offset:offset + size])
            expected = data[0xa0 + i * 32:0xc0 + i * 32].hex()
            if size == 0 or offset + size > len(raw) or actual != expected:
                raise ValueError(f'original NSO segment {i} hash mismatch')
            segments.append({'offset': offset, 'size': size, 'sha256': actual})
        command = [sys.executable, str(d2r / 'dm/gfx/tools/nvn_slots.py'),
                   str(raw_path), str(slots_path), str(names_path)]
        subprocess.run(command, check=True, capture_output=True, text=True)
        commands.append(command)
        names_bytes, slots_bytes = names_path.read_bytes(), slots_path.read_bytes()
        names = names_bytes.decode('ascii').splitlines()
        if len(names) != 523 or len(set(names)) != len(names):
            raise ValueError('unexpected NVN name count or duplicate names')
        if any(not re.fullmatch(r'nvn[A-Za-z0-9_]+', n) for n in names):
            raise ValueError('unsafe or invalid NVN name')
        rows = [line.split('\t') for line in slots_bytes.decode().splitlines()
                if line and not line.startswith('#')][1:]
        if [row[0] for row in rows] != names:
            raise ValueError('name and slot output order differ')
        if [int(row[1], 16) for row in rows] != list(range(0x320e910, 0x320e910 + 523 * 8, 8)):
            raise ValueError('unexpected NVN slot addresses')
        if sum(int(row[2]) != 0 for row in rows) != 216:
            raise ValueError('unexpected static NVN call-name count')
        # Same public PFN declaration format used by the original gen_protos.py.
        # Preserve Android's exact declared types; no Apple boolean widening.
        public = (root / 'nvn-native/third_party/nvn/nvn.h').read_text()
        prototypes = {key: (' '.join(result.split()), ' '.join(parameters.split()))
                      for result, key, parameters in re.findall(
                          r'typedef\s+([\w\s\*]+?)\s*\(\*PFNNVN(\w+)PROC\)\s*\(([^;]*?)\);',
                          public, re.S)}
        # Opaque NVN objects are caller-owned storage and getters return
        # object pointers, never by-value copies. A historical reconstructed
        # declaration omitted '*' for several GetMemoryPool APIs; silently
        # generating those wrappers changes AAPCS64 to hidden-X8 sret.
        # Real value aggregates (e.g. separate handles) are not opaque objects.
        opaque_objects = set(re.findall(
            r'typedef\s+struct\s*\{[^{}]*?\breserved\s*\[[^]]+\][^{}]*?\}\s*(NVN\w+)\s*;',
            public, re.S))
        entries, unprototyped = [], []
        forms = {'VOID': 0, 'VALUE': 0, 'RESOLVER': 0, 'UNKNOWN': 0}
        for name in names:
            key = name[3:].upper()
            if key in prototypes:
                result, parameters = prototypes[key]
                if result in opaque_objects:
                    raise ValueError(f'opaque NVN object returned by value: {name}: {result}')
                args = [a.strip() for a in parameters.split(',')] if parameters not in ('', 'void') else []
                parameters = ', '.join(f'{arg} a{i} __attribute__((unused))' for i, arg in enumerate(args)) or 'void'
                abi = f'PFNNVN{key}PROC'
                arguments = ', '.join(f'a{i}' for i in range(len(args)))
                form = 'VOID' if result == 'void' else 'VALUE'
                if name == 'nvnDeviceGetProcAddress':
                    # Never forward this raw resolver: subsequent lookups must
                    # return adapter wrappers, including the NULL-device pass.
                    form = 'RESOLVER'
            else:
                unprototyped.append(name)
                abi, result, parameters = 'PFNNVNGENERICFUNCPTRPROC', 'void', 'void'
                arguments, form = '', 'UNKNOWN'
            forms[form] += 1
            entries.append(f'NN_D2R_NVN_ENTRY({name}, {abi}, {result}, ({parameters}), ({arguments}), {form})\n')
        header = ('/* Generated verified name metadata; no guest code. Deliberately no include guard. */\n'
                  '#ifndef NN_D2R_NVN_NAMESET_META\n#define NN_D2R_NVN_NAMESET_META\n'
                  '#define NN_D2R_NVN_NAMES_SCHEMA 2\n'
                  '#define NN_D2R_NVN_NAMES_COUNT 523\n'
                  f'#define NN_D2R_NVN_TYPED_FORWARD_COUNT {forms["VOID"] + forms["VALUE"]}\n'
                  f'#define NN_D2R_NVN_ADAPTER_RESOLVER_COUNT {forms["RESOLVER"]}\n'
                  f'#define NN_D2R_NVN_UNKNOWN_ABI_COUNT {forms["UNKNOWN"]}\n'
                  f'#define NN_D2R_NVN_INPUT_SHA256 "{INPUT_SHA}"\n'
                  f'#define NN_D2R_NVN_NAMES_SHA256 "{sha(names_bytes)}"\n'
                  '#endif\n#ifndef NN_D2R_NVN_ENTRY\n#error "Define NN_D2R_NVN_ENTRY"\n#endif\n'
                  + ''.join(entries)).encode()
        if forms['RESOLVER'] != 1:
            raise ValueError('expected the one original DeviceGetProcAddress resolver')
        artifacts = {'names.txt': names_bytes, 'slots.tsv': slots_bytes, 'names.inc.h': header}
        sources = ['nvn-native/adapters/d2r/extract_nvn_names.py',
                   'nvn-native/adapters/d2r/host_bridge.c', 'nvn-native/adapters/d2r/host_bridge.h',
                   'nvn-native/third_party/nvn/nvn.h', 'dm/tools/nso.py', 'dm/gfx/tools/nvn_slots.py']
        record = {'schema': 1, 'profile': PROFILE, 'input_sha256': INPUT_SHA,
                  'build_id': data[0x40:0x60].hex(), 'segments': segments,
                  'raw_sha256': sha(raw), 'names': 523, 'static_call_names': 216,
                  'public_prototype_names': len(names) - len(unprototyped),
                  'typed_forward_names': forms['VOID'] + forms['VALUE'],
                  'adapter_resolver_names': forms['RESOLVER'],
                  'generated_header_schema': 2,
                  'diagnostic_policy': 'pre/post-call sticky error is fatal; return values unchanged',
                  'unprototyped_fatal_boundaries': unprototyped,
                  'sources': {str((d2r if name.startswith('dm/') else root) / name): sha(((d2r if name.startswith('dm/') else root) / name).read_bytes()) for name in sources},
                  'source_checkouts': {'nvn': checkout_identity(root), 'd2r': checkout_identity(d2r)},
                  'path_policy_sha256': sha((root / 'nvn-native/tools/workspace_paths.py').read_bytes()),
                  'python_sha256': sha(Path(sys.executable).resolve().read_bytes()),
                  'lz4_version': importlib.metadata.version('lz4'),
                  'options': {'input': 'unmodified-main', 'loader': '0x1739290',
                              'name_order': 'original-slot-address', 'raw_output_retained': False,
                              'opaque_return_validation': 'reject-opaque-NVN-object-values'},
                  'commands': commands,
                  'artifacts': {name: {'sha256': sha(blob), 'size': len(blob)}
                                for name, blob in artifacts.items()}}
        return artifacts, record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--d2r-root', type=Path, help='External D2R validation checkout; or D2R_ROOT')
    parser.add_argument('--nso', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--verify', action='store_true', help='Verify cached output without replacing it')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    out = args.out.resolve()
    if not out.is_relative_to(root / '.local'):
        parser.error('output must stay under this worktree .local directory')
    try:
        if args.verify and not out.is_dir():
            raise ValueError('missing generated output directory')
        out.parent.mkdir(parents=True, exist_ok=True)
        artifacts, record = extract(args.nso.resolve(), out, root, args.d2r_root)
        if args.verify:
            saved = json.loads((out / 'names-record.json').read_text())
            # Temporary filenames differ on each run; all source/input/options
            # and artifact identities must still match the fresh extraction.
            for key in record:
                if key != 'commands' and saved.get(key) != record[key]:
                    raise ValueError(f'cached metadata differs: {key}')
            if command_identity(saved['commands']) != command_identity(record['commands']):
                raise ValueError('cached extraction commands differ')
            for name, blob in artifacts.items():
                if (out / name).read_bytes() != blob:
                    raise ValueError(f'cached artifact differs: {name}')
        else:
            out.mkdir(parents=True, exist_ok=True)
            for name, blob in artifacts.items():
                (out / name).write_bytes(blob)
            (out / 'names-record.json').write_text(json.dumps(record, indent=2) + '\n')
    except (OSError, ValueError, KeyError, IndexError, TypeError,
            subprocess.CalledProcessError) as error:
        parser.error(str(error))
    print(f'{"verified" if args.verify else "generated"} {out / "names-record.json"}')


if __name__ == '__main__':
    main()
