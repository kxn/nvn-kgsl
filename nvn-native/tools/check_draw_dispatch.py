#!/usr/bin/env python3
"""HOST ONLY: typed draw/dispatch recorders and GPU argument packet encoding."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--color-check', type=Path, required=True)
    parser.add_argument('--compute-check', type=Path, required=True)
    parser.add_argument('--ndk-clang', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('choose a new output directory under ROOT/.local')
    out.mkdir(parents=True)
    native = root / 'nvn-native'
    proof = {'schema': 1, 'scope': 'HOST RAM ONLY; NDK link only; no GPU/game execution',
             'passed': False, 'runs': []}
    def run(label, command):
        result = subprocess.run(command, cwd=root, text=True, capture_output=True,
                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'})
        proof['runs'].append({'label': label, 'argv': [str(x) for x in command],
                              'returncode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise RuntimeError(label + ' failed')
        return result.stdout
    try:
        files = [Path(__file__), native / 'tests/d2r_bridge/draw_dispatch.c',
                 native / 'tests/d2r_bridge/memory_backend.c', native / 'tests/d2r_bridge/window_session_backend.inc']
        files += [p for folder in ['src', 'include'] for p in (native / folder).rglob('*') if p.is_file()]
        proof['sources'] = {str(p): sha(p) for p in files}
        graphics = module(native / 'adapters/d2r/graphics_bundle.py', 'nn_draw_graphics')
        color, check, hashes = graphics.build(args.color_check.resolve(), out / 'color')
        if check.get('modes') is not True or check['program']['chip_id'] != 0x06050002:
            raise ValueError('A650 schema3 color image required')
        offline = module(native / 'tools/offline_compute_cache.py', 'nn_draw_compute')
        _, manifest, outputs = offline.build(args.compute_check, out / 'compute', root=root)
        compute = out / 'compute/image_1.h'
        if str(compute) not in outputs or manifest['chip_id'] != 0x06050002:
            raise ValueError('verified A650 compute image required')
        proof['inputs'] = {'color_check': str(args.color_check.resolve()), 'color_sha256': sha(args.color_check),
                           'color_artifacts': hashes, 'compute_check': str(args.compute_check.resolve()),
                           'compute_sha256': sha(args.compute_check), 'cache_manifest_sha256': sha(out / 'compute/cache-manifest.json')}
        flags = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-I'+str(native / 'include'),
                 '-I'+str(native / 'third_party/nvn'), '-DNN_DRAW_IMAGE_HEADER="'+str(color)+'"',
                 '-DNN_COMPUTE_IMAGE_HEADER="'+str(compute)+'"']
        sources = [native / 'tests/d2r_bridge/draw_dispatch.c', native / 'tests/d2r_bridge/memory_backend.c',
                   native / 'src/compute.c', native / 'src/graphics.c', native / 'src/external_image.c',native / 'src/order.c',native / 'src/render_state.c',native / 'src/sampler_state.c',native / 'src/texture_source.c', native / 'src/texture_bridge.c', native / 'src/counter.c']
        for compiler, sanitizer in [('clang', ['-fsanitize=function', '-fsanitize-trap=function']),
                                     ('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'])]:
            binary = out / ('host-'+compiler)
            run(compiler+' compile', [compiler, *flags, *sanitizer, *sources, '-o', binary])
            stdout = run(compiler+' execute', [binary])
            result = json.loads(stdout.splitlines()[-1])
            if result['gpu_submits'] != 0 or result['draw_modes'] != 6 or result['compute_indirect'] != 1:
                raise ValueError('HOST result contract changed')
            proof['runs'][-1]['result'] = result
            proof['runs'][-1]['binary_sha256'] = sha(binary)
        ndk = args.ndk_clang.resolve()
        binary = out / 'android-arm64-link-only'
        proof['ndk_clang_sha256'] = sha(ndk)
        run('NDK strict link only', [ndk, *flags, '-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',
                                    *sources, '-o', binary])
        proof['runs'][-1]['binary_sha256'] = sha(binary)
        if any(sha(p) != expected for p, expected in proof['sources'].items()):
            raise ValueError('source changed during verification')
        proof['passed'] = True
    except Exception as error:
        proof['error'] = str(error)
    finally:
        (out / 'proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    print(json.dumps({'passed': proof['passed'], 'proof': str(out / 'proof.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
