#!/usr/bin/env python3
"""Build a verified native D2R resolver OBJECT target with original HLE_FORCE.

Requires explicit --nso/--ndk/--out. Only the bridge object and independent driver
archive are built: no game host, rewritten game binary, probe APK or deployment.
The original host has not been connected by this tool. A later host integration
can link CMake's nvn_d2r_host_bridge OBJECT target directly, retaining swforce.
Use --verify to recheck original-name provenance and every cached build artifact
without rebuilding or repairing it. Normal rebuilds also check previous cached
artifact integrity before invoking CMake.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from workspace_paths import d2r_root, checkout_identity


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def file_map(paths):
    return {str(path): sha(path) for path in sorted(set(paths))}


def check_files(files, label):
    for filename, expected in files.items():
        path = Path(filename)
        if not path.is_file() or sha(path) != expected:
            raise ValueError(f'{label} integrity mismatch: {path}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--d2r-root', type=Path, help='External D2R validation checkout; or D2R_ROOT')
    parser.add_argument('--nso', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--python', type=Path, default=Path(sys.executable),
                        help='Python with lz4 for the original NSO extractor')
    parser.add_argument('--verify', action='store_true', help='Verify existing output; do not rebuild')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    repo = root.parent
    try:
        d2r = d2r_root(args.d2r_root)
    except ValueError as exc:
        parser.error(str(exc))
    out, nso, ndk = args.out.resolve(), args.nso.resolve(), args.ndk.resolve()
    # The name extractor enforces this boundary too; no generated game metadata
    # or transient original raw image belongs in tracked source directories.
    if not out.is_relative_to(repo / '.local'):
        parser.error('output must stay under this worktree .local directory')
    build, names = out / 'cmake', out / 'names'
    record_path = out / 'build-record.json'
    commands = []

    def run(*command, quiet=False):
        argv = list(map(str, command))
        commands.append(argv)
        result = subprocess.run(argv, check=True, text=True, capture_output=True)
        if result.stdout and not quiet:
            print(result.stdout, end='')
        return result.stdout

    try:
        python = args.python.absolute()  # Preserve the venv path; do not resolve its interpreter symlink.
        prebuilt = ndk / 'toolchains/llvm/prebuilt/linux-x86_64'
        toolchain = ndk / 'build/cmake/android.toolchain.cmake'
        readelf = prebuilt / 'bin/llvm-readelf'
        tools = [python.resolve(), ndk / 'source.properties', toolchain,
                 prebuilt / 'bin/clang', prebuilt / 'bin/llvm-ar', readelf]
        for name in ('cmake', 'ninja'):
            found = shutil.which(name)
            if not found:
                raise ValueError(f'missing build tool: {name}')
            tools.append(Path(found).resolve())
        tool_hashes = file_map(tools)
        sources = [root / 'CMakeLists.txt', root / 'tools/build_d2r_bridge.py', root / 'tools/workspace_paths.py',
                   root / 'adapters/d2r/host_bridge.c', root / 'adapters/d2r/host_bridge.h',
                   root / 'adapters/d2r/extract_nvn_names.py',
                   d2r / 'dm/tools/nso.py', d2r / 'dm/gfx/tools/nvn_slots.py']
        sources += [d2r / 'dm/host' / name for name in ('hle.h', 'dm.h', 'platform.h')]
        for folder in ('src', 'include', 'third_party/nvn', 'third_party/kgsl'):
            sources.extend(p for p in (root / folder).rglob('*') if p.is_file())
        source_hashes = file_map(sources)
        options = {'d2r_root': str(d2r), 'source_checkouts': {'nvn': checkout_identity(repo), 'd2r': checkout_identity(d2r)}, 'nso': str(nso), 'ndk': str(ndk), 'python': str(python),
                   'abi': 'arm64-v8a', 'api': 29, 'build_type': 'RelWithDebInfo',
                   'targets': ['nvn_d2r_host_bridge', 'nvn_kgsl'], 'host_hle': True,
                   'nvn_cs_header': '', 'nvn_graphics_header': '', 'nvn_maxwell_header': '',
                   'nvn_graphics_api_header': '', 'nvn_texture_header': '', 'nvn_texture_api_header': '',
                   'nvn_api_probe': False, 'nvn_device_inspect': False, 'nvn_ahb_probe': False, 'nvn_ahb_graphics_header': '',
                   'bridge_policy': 'typed forwarding; sticky errors fatal; return values unchanged'}
        prior = json.loads(record_path.read_text()) if record_path.exists() else None
        if prior:
            check_files(prior['artifacts'], 'cached build artifact')
            check_files(prior['generated_names'], 'cached generated name')
        if args.verify:
            if prior is None:
                raise ValueError('missing build-record.json')
            if prior['source'] != source_hashes or prior['tools'] != tool_hashes:
                raise ValueError('source/tool identity changed; build into a fresh output or rebuild')
            if prior['options'] != options or prior['input_sha256'] != sha(nso):
                raise ValueError('original input or effective build options changed')
        else:
            out.mkdir(parents=True, exist_ok=True)
            run(python, root / 'adapters/d2r/extract_nvn_names.py', '--nso', nso, '--out', names, '--d2r-root', d2r)
        run(python, root / 'adapters/d2r/extract_nvn_names.py', '--nso', nso, '--out', names, '--d2r-root', d2r, '--verify')
        name_record = json.loads((names / 'names-record.json').read_text())
        if (name_record.get('generated_header_schema') != 2 or
                name_record.get('adapter_resolver_names') != 1 or
                name_record.get('typed_forward_names', 0) + name_record.get('adapter_resolver_names', 0)
                != name_record['public_prototype_names']):
            raise ValueError('expected verified typed-forwarding name metadata schema2')
        native_names = set(re.findall(r'^ENTRY\((\w+),', (root / 'src/nvn_entries.inc').read_text(), re.M))
        declared_names = set((names / 'names.txt').read_text().splitlines()) - set(name_record['unprototyped_fatal_boundaries'])
        if not native_names <= declared_names:
            raise ValueError('native resolver entry is missing a declared D2R typed wrapper')
        bridge_policy = {'metadata_schema': 2, 'typed_forward_names': name_record['typed_forward_names'],
                         'adapter_resolver_names': 1,
                         'unknown_abi_fatal_names': len(name_record['unprototyped_fatal_boundaries']),
                         'native_names_with_typed_forwarding': len(native_names),
                         'diagnostics': name_record['diagnostic_policy']}
        if args.verify and prior.get('bridge_policy') != bridge_policy:
            raise ValueError('cached bridge forwarding policy differs')
        if not args.verify:
            run('cmake', '-S', root, '-B', build, '-G', 'Ninja',
                f'-DCMAKE_TOOLCHAIN_FILE={toolchain}', f'-DD2R_ROOT={d2r}', '-DANDROID_ABI=arm64-v8a',
                '-DANDROID_PLATFORM=android-29', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DNVN_D2R_HOST_BRIDGE=ON',
                f'-DNVN_D2R_NAMES_HEADER={names / "names.inc.h"}',
                '-DNVN_CS_HEADER=', '-DNVN_GRAPHICS_HEADER=', '-DNVN_MAXWELL_HEADER=', '-DNVN_API_PROBE=OFF',
                '-DNVN_GRAPHICS_API_HEADER=', '-DNVN_TEXTURE_HEADER=', '-DNVN_TEXTURE_API_HEADER=',
                '-DNVN_DEVICE_INSPECT=OFF', '-DNVN_AHB_PROBE=OFF', '-DNVN_AHB_GRAPHICS_HEADER=')
            run('cmake', '--build', build, '--target', 'nvn_d2r_host_bridge', 'nvn_kgsl')
        objects = sorted((build / 'CMakeFiles/nvn_d2r_host_bridge.dir').rglob('*.o'))
        if len(objects) != 1 or objects[0].name != 'host_bridge.c.o':
            raise ValueError('expected exactly the original-host bridge object')
        elf = run(readelf, '--file-header', '--sections', '--symbols', '--wide', objects[0], quiet=True)
        registration = re.search(r'\bswforce\s+PROGBITS\s+\S+\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+([A-Z]+)', elf)
        if (not re.search(r'Machine:\s+AArch64\b', elf) or not re.search(r'Class:\s+ELF64\b', elf)
                or not re.search(r'Type:\s+REL\b', elf) or not registration
                or int(registration[1], 16) != 24 or 'R' not in registration[2]):
            raise ValueError('bridge must be an AArch64 object with the original swforce registration')
        symbols = set(re.findall(r'\bforward_nvn\w+\b', elf))
        unknown_symbols = set(re.findall(r'\bunknown_abi_nvn\w+\b', elf))
        if (len(symbols) != bridge_policy['typed_forward_names'] or
                len(unknown_symbols) != bridge_policy['unknown_abi_fatal_names']):
            raise ValueError('bridge object does not retain every typed/unknown-ABI boundary')
        elf_summary = '\n'.join(line.strip() for line in elf.splitlines()
                                if any(label in line for label in ('Class:', 'Machine:', 'Type:'))
                                or re.search(r'\bswforce\s+PROGBITS\b', line))
        print(elf_summary)
        if args.verify:
            print(f'verified {record_path}')
            return
        compiled = json.loads((build / 'compile_commands.json').read_text())
        bridge_compile = [item for item in compiled if Path(item['file']) == root / 'adapters/d2r/host_bridge.c']
        if len(bridge_compile) != 1 or 'NN_D2R_HOST_HLE=1' not in bridge_compile[0]['command']:
            raise ValueError('missing original HLE_FORCE compile definition')
        generated = file_map(names / name for name in ('names.inc.h', 'names.txt', 'slots.tsv', 'names-record.json'))
        artifacts = file_map(objects + [build / 'libnvn_kgsl.a', build / 'compile_commands.json'])
        name_record = json.loads((names / 'names-record.json').read_text())
        if (sha(nso) != name_record['input_sha256'] or source_hashes != file_map(sources)
                or tool_hashes != file_map(tools)):
            raise ValueError('input/source/tool identity changed during the build')
        record = {'schema': 1, 'input_sha256': sha(nso), 'build_id': name_record['build_id'],
                  'original_segments': name_record['segments'], 'generated_names': generated,
                  'source': source_hashes, 'tools': tool_hashes, 'commands': commands,
                  'options': options, 'bridge_compile': bridge_compile, 'bridge_policy': bridge_policy,
                  'nvn_resolver_entries': len(re.findall(r'^ENTRY\(', (root / 'src/nvn_entries.inc').read_text(), re.M)),
                  'bridge_elf': elf_summary, 'artifacts': artifacts,
                  'scope': 'Android ARM64 compile/archive only; original HLE_FORCE bridge object. '
                           'Typed ABI wrappers stop on sticky driver diagnostics, including unsupported arguments. '
                           'Not connected to a real game host; no game module, GPU, APK or device execution.'}
        record_path.write_text(json.dumps(record, indent=2) + '\n')
        print(record_path)
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stdout or '', end='')
            print(error.stderr or '', end='')
        parser.error(str(error))


if __name__ == '__main__':
    main()
