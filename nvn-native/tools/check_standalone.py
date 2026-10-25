#!/usr/bin/env python3
"""Public-source smoke checks; no game data, external D2R checkout or ADB required.

Run four synthetic HOST suites and optionally build the Android runtime/probe.
Hardware execution and private original-game integration remain separate gates.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, help='Optional Android NDK; builds ARM64/API29 without running it')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    native = root / 'nvn-native'
    out = args.out.resolve()
    if not out.is_relative_to(root / '.local'):
        parser.error('smoke outputs must stay under this repository .local')
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env.pop('D2R_ROOT', None)
    env.pop('NVN_SHADER_FRONTEND_ROOT', None)
    env['PYTHONPATH'] = str(native / 'tools')
    commands = []

    def run(argv):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=root, env=env, capture_output=True, text=True)
        commands.append({'argv': argv, 'returncode': result.returncode,
                         'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise ValueError(result.stderr or result.stdout)

    record = {'schema': 1, 'kind': 'standalone-source-smoke-v1', 'passed': False,
              'game_inputs': False, 'game_execution': False, 'gpu_execution': False,
              'adb_used': False, 'commands': commands}
    try:
        for name in ('shader_fixture', 'copy_tools', 'probe_provenance', 'pool_policy_diagnostic'):
            run([sys.executable, '-m', 'unittest', 'discover', '-s', native / 'tests', '-p', 'test_' + name + '.py'])
        if args.ndk:
            ndk = args.ndk.resolve()
            build = out / 'android'
            run(['cmake', '-S', native, '-B', build, '-G', 'Ninja',
                 '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
                 '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-29',
                 '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DNVN_D2R_HOST_BRIDGE=OFF', '-DD2R_ROOT='])
            run(['cmake', '--build', build, '--target', 'nvn_kgsl_probe', '-j', '4'])
            deps = subprocess.check_output(['ninja', '-C', str(build), '-t', 'deps'], text=True)
            outside = []
            source_paths = set()
            for line in deps.splitlines():
                if not line.startswith('    '):
                    continue
                path = Path(line.strip())
                path = (path if path.is_absolute() else build / path).resolve()
                if path.is_relative_to(build):
                    continue
                if path.is_relative_to(native):
                    source_paths.add(path)
                if path.is_relative_to(root) and not path.is_relative_to(native):
                    outside.append(str(path))
            if outside:
                raise ValueError('standalone core depends on non-native repository files: ' + repr(outside))
            record['source_hashes'] = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source_paths)}
            record['android_translation_units'] = len(json.loads((build / 'compile_commands.json').read_text()))
        record['passed'] = True
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        record['error'] = str(exc)
    path = out / 'check.json'
    path.write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({'passed': record['passed'], 'check': str(path)}))
    return 0 if record['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
