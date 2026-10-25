#!/usr/bin/env python3
"""HOST resource/copy/clear semantics and actual Android backend link; no GPU."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ndk-clang', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    native, out = root / 'nvn-native', args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('choose a new output directory below ROOT/.local')
    out.mkdir(parents=True)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    proof = {'schema': 1, 'scope': 'HOST state/packet/owner checks; NDK real backend link only; no GPU/game',
             'passed': False, 'runs': []}
    def run(label, argv):
        result = subprocess.run([str(x) for x in argv], cwd=root, text=True, capture_output=True,
                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        proof['runs'].append({'label': label, 'argv': [str(x) for x in argv], 'returncode': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise RuntimeError(label+' failed')
        return result.stdout
    try:
        files = [p for folder in ['src', 'include', 'third_party/nvn', 'third_party/kgsl', 'android']
                 for p in (native / folder).rglob('*') if p.is_file()]
        files += [native / 'tests/d2r_bridge' / f for f in ['resource_copy.c', 'target_clear.c', 'resource_packet_boundary.c', 'memory_backend.c', 'window_session_backend.inc']]
        files.append(Path(__file__))
        proof['sources'] = {str(p): sha(p) for p in files}
        common = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-I'+str(native / 'include'),
                  '-I'+str(native / 'third_party/nvn'), '-I'+str(native / 'third_party/kgsl'),
                  '-DNN_RESOURCE_CORE="'+str(native / 'src/nvn.c')+'"',
                  '-DNN_RESOURCE_MEMORY_BACKEND="'+str(native / 'tests/d2r_bridge/memory_backend.c')+'"']
        modules = [native / 'src' / (name+'.c') for name in ['compute', 'graphics', 'external_image', 'order', 'render_state', 'sampler_state', 'texture_source', 'texture_bridge', 'counter']]
        for compiler, sanitizer in [('clang', ['-fsanitize=function', '-fsanitize-trap=function']),
                                     ('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'])]:
            for name, fixture, rows in [('resources', 'target_clear.c', 2), ('boundaries', 'resource_packet_boundary.c', 1)]:
                binary = out / (name+'-'+compiler)
                run(name+' '+compiler+' compile', [compiler, *common, *sanitizer, native / 'tests/d2r_bridge' / fixture, *modules, '-o', binary])
                results = [json.loads(line) for line in run(name+' '+compiler+' execute', [binary]).splitlines() if line.startswith('{')]
                if len(results) != rows or any(not item['passed'] for item in results):
                    raise ValueError(name+' result contract changed')
                proof['runs'][-1]['results'] = results
        sources = [native / 'src/nvn.c', native / 'src/kgsl.c', *modules,
                   *[native / 'android' / (name+'.c') for name in ['window_session', 'window_android', 'present', 'present_pacer', 'window_display', 'ahb']]]
        binary = out / 'libnative-resources-api29.so'
        proof['ndk_clang_sha256'] = sha(args.ndk_clang.resolve())
        run('NDK actual backend strict full link', [args.ndk_clang.resolve(), *common, '-fPIC', '-shared', '-Wl,--no-undefined',
                                                   *sources, '-landroid', '-ldl', '-o', binary])
        proof['binary_sha256'] = sha(binary)
        if any(sha(p) != expected for p, expected in proof['sources'].items()):
            raise ValueError('source changed during checks')
        proof['passed'] = True
    except Exception as error:
        proof['error'] = str(error)
    finally:
        (out / 'proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    print(json.dumps({'passed': proof['passed'], 'proof': str(out / 'proof.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
