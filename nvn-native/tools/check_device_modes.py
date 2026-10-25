#!/usr/bin/env python3
"""HOST ONLY: verify schema3 artifacts, then test NVN Draw mode snapshots in RAM."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True, help='verified source checkout')
    parser.add_argument('--source', type=Path, help='default: nvn-native/tests/d2r_bridge/device_modes.c')
    parser.add_argument('--color-check', type=Path, required=True)
    parser.add_argument('--texture-check', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='new directory below ROOT/.local')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    if not out.is_relative_to(root / '.local'):
        parser.error('generated shader headers and binaries must stay below ROOT/.local')
    if out.exists():
        parser.error('choose a new --out directory to preserve earlier evidence')
    source = (args.source or root / 'nvn-native/tests/d2r_bridge/device_modes.c').resolve()
    checks = {'color': args.color_check.resolve(), 'texture': args.texture_check.resolve()}
    out.mkdir(parents=True)
    proof = {'scope': 'HOST ONLY: memory backend, no GPU or original module execution',
             'passed': False, 'headers_are_generated_ignored_artifacts': True,
             'source': str(source), 'runs': [], 'checked_inputs': {}}
    files = [source, Path(__file__).resolve(), root / 'nvn-native/src/nvn.c',
             root / 'nvn-native/src/nvn_entries.inc', root / 'nvn-native/src/graphics.c',
             root / 'nvn-native/src/compute.c', root / 'nvn-native/tests/d2r_bridge/memory_backend.c',
             root / 'nvn-native/src/external_image.c',
             root / 'nvn-native/third_party/nvn/nvn.h',
             root / 'nvn-native/adapters/d2r/graphics_bundle.py',
             root / 'nvn-native/adapters/d2r/texture_bundle.py']
    files += sorted((root / 'nvn-native/include/nvn_native').glob('*.h'))
    files += sorted((root / 'nvn-native/src').glob('*.c')) + sorted((root / 'nvn-native/src').glob('*.inc'))
    files.append(root / 'nvn-native/tests/d2r_bridge/window_session_backend.inc')
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def run(label, argv):
        result = subprocess.run(argv, cwd=out, env=env, capture_output=True, text=True)
        proof['runs'].append({'label': label, 'argv': argv, 'returncode': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise RuntimeError(label + ' failed; see proof.json')
        return result.stdout

    try:
        proof['source_sha256'] = {str(path): sha(path) for path in files}
        adapters = root / 'nvn-native/adapters/d2r'
        modules = {name: module(adapters / ('graphics_bundle.py' if name == 'color' else 'texture_bundle.py'),
                                'host_mode_' + name) for name in checks}
        for name, check_path in checks.items():
            # Reuse the normal check consumers: source/compiler/artifact hashes,
            # original input identities, device and scalar patch metadata.
            header, check, hashes = modules[name].build(check_path, out / name)
            if check.get('modes') is not True or check['program']['chip_id'] != 0x06050002:
                raise ValueError('this memory backend requires explicit schema3 A650 check: ' + name)
            proof['checked_inputs'][name] = {'check': str(check_path), 'check_sha256': sha(check_path),
                                             'generated_header': str(header), 'artifacts': hashes}
            for compiler, sanitizer in [('clang', 'function'), ('gcc', 'address-undefined')]:
                flags = (['-fsanitize=function', '-fsanitize-trap=function'] if compiler == 'clang' else
                         ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'])
                binary = out / (name + '-' + compiler)
                command = [compiler, '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror', *flags,
                           '-I' + str(root / 'nvn-native/include'),
                           '-I' + str(root / 'nvn-native/third_party/nvn'),
                           # The same relative include works in its intended tracked
                           # location and in an ignored draft outside that directory.
                           '-iquote', str(root / 'nvn-native/tests/d2r_bridge'),
                           '-DNN_MODE_IMAGE_HEADER="' + str(header) + '"', str(source),
                           str(root / 'nvn-native/tests/d2r_bridge/memory_backend.c'),
                           str(root / 'nvn-native/src/compute.c'), str(root / 'nvn-native/src/graphics.c'),
                           str(root / 'nvn-native/src/external_image.c'),str(root / 'nvn-native/src/order.c'),str(root / 'nvn-native/src/render_state.c'),str(root / 'nvn-native/src/sampler_state.c'),str(root / 'nvn-native/src/texture_source.c', root / 'nvn-native/src/texture_bridge.c'),str(root / 'nvn-native/src/counter.c'),
                           '-o', str(binary)]
                run(name + ' ' + sanitizer + ' compile', command)
                result = json.loads(run(name + ' ' + sanitizer + ' run', [str(binary)]))
                expected = {'scope': 'host_only_mode_frontend', 'profile': 0 if name == 'color' else 1,
                            'checks': 90 if name == 'color' else 116,
                            'same_record_two_mode_snapshots': True, 'gpu_submits': 0}
                if result != expected:
                    raise ValueError('unexpected host test result: ' + name)
                proof['runs'][-1]['result'] = result
                proof['runs'][-1]['binary_sha256'] = sha(binary)
        proof['source_unchanged'] = all(sha(Path(path)) == value
                                        for path, value in proof['source_sha256'].items())
        if not proof['source_unchanged']:
            raise RuntimeError('test source changed during verification')
        proof['tool_versions'] = {cc: subprocess.run([cc, '--version'], text=True,
                                     capture_output=True, check=True).stdout for cc in ('clang', 'gcc')}
        proof['passed'] = True
    except Exception as error:
        proof['error'] = str(error)
    finally:
        (out / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'host_only': True, 'passed': proof['passed'], 'proof': str(out / 'proof.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
