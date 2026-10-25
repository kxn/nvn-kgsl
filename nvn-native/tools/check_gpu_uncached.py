#!/usr/bin/env python3
"""HOST ONLY GPU_UNCACHED CPU-only regression and Android source/link checks; no GPU work."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--ndk-clang', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True, help='new ignored directory below ROOT/.local')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    if not out.is_relative_to(root / '.local') or out.exists():
        parser.error('--out must be a new directory below ROOT/.local')
    out.mkdir(parents=True)
    ndk = args.ndk_clang.resolve()
    sources = [root / 'nvn-native/tests/d2r_bridge/pool_gpu_uncached.c', root / 'nvn-native/src/compute.c',
               root / 'nvn-native/src/graphics.c', root / 'nvn-native/src/external_image.c',root / 'nvn-native/src/order.c',root / 'nvn-native/src/render_state.c',root / 'nvn-native/src/sampler_state.c', root / 'nvn-native/src/texture_source.c', root / 'nvn-native/src/texture_bridge.c', root / 'nvn-native/src/counter.c']
    common = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
              '-I' + str(root / 'nvn-native/include'),
              '-I' + str(root / 'nvn-native/third_party/nvn'),
              '-iquote', str(root / 'nvn-native/include/nvn_native'),
              '-DNN_MEMORY_BACKEND_SOURCE="' + str(root / 'nvn-native/tests/d2r_bridge/memory_backend.c') + '"']
    proof = {'scope': 'HOST ONLY actual tracked core and memory backend; Android compile/link only',
             'passed': False, 'gpu_submits': 0, 'runs': []}
    sources_inventory = list((root / 'nvn-native/src').glob('*.c')) + list((root / 'nvn-native/src').glob('*.inc'))
    files = sources + sources_inventory + [Path(__file__).resolve(), root / 'nvn-native/src/nvn.c', root / 'nvn-native/src/nvn_entries.inc',
                      root / 'nvn-native/include/nvn_native/nvn.h', root / 'nvn-native/tests/d2r_bridge/memory_backend.c']
    files += sorted((root / 'nvn-native/include/nvn_native').glob('*.h'))
    files.append(root / 'nvn-native/tests/d2r_bridge/window_session_backend.inc')
    proof['sha256'] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')

    def run(label, command):
        result = subprocess.run(command, cwd=out, env=env, text=True, capture_output=True)
        proof['runs'].append({'label': label, 'argv': command, 'exit': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise RuntimeError(label + ' failed')
        return result.stdout

    try:
        for flags in (0x12, 0x14):
            profile = ['-DNN_UNQUALIFIED_FLAGS=' + str(flags)]
            for cc, sanitizer in [('clang', ['-fsanitize=function', '-fsanitize-trap=function']),
                                  ('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'])]:
                binary = out / ('test-' + str(flags) + '-' + cc)
                run(str(flags) + ' ' + cc + ' compile', [cc, *common, *profile, *sanitizer, *map(str, sources), '-o', str(binary)])
                result = json.loads(run(str(flags) + ' ' + cc + ' run', [str(binary)]))
                if result != {'scope': 'HOST ONLY CPU-only pool and rejection tests', 'flags': flags,
                              'checks': 880 if flags == 0x12 else 879,
                              'policy_rejections': 15, 'gpu_submits': 0}:
                    raise ValueError('unexpected host test counters')
            run(str(flags) + ' Android29 fixture strict link (not executed)',
                [str(ndk), *common, *profile, '-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1', '-fPIC', '-shared',
                 '-Wl,--no-undefined', *map(str, sources), '-o', str(out / ('libhost-fixture-' + str(flags) + '.so'))])
        run('Android29 core strict compile', [str(ndk), *common, '-c', str(root / 'nvn-native/src/nvn.c'), '-o', str(out / 'nvn.o')])
        if any(hashlib.sha256(Path(p).read_bytes()).hexdigest() != sha for p, sha in proof['sha256'].items()):
            raise RuntimeError('input source changed during verification')
        proof['passed'] = True
    except Exception as error:
        proof['error'] = str(error)
    finally:
        (out / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': proof['passed'], 'scope': proof['scope'], 'proof': str(out / 'proof.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
