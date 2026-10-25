#!/usr/bin/env python3
"""Check actual Copy/Counter/Queue sources on HOST and link Android; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--ndk-clang', type=Path, required=True)
    parser.add_argument('--image-header', type=Path, required=True)
    parser.add_argument('--counter-image', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    native, out = root / 'nvn-native', args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('choose a new output directory below ROOT/.local')
    out.mkdir(parents=True)
    tests = native / 'tests/d2r_bridge'
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files = [p for folder in ('src', 'include', 'third_party/nvn', 'third_party/kgsl', 'android', 'tests/d2r_bridge')
             for p in (native / folder).rglob('*') if p.is_file()]
    files += [Path(__file__).resolve(), args.image_header.resolve(), args.counter_image.resolve()]
    proof = dict(passed=False, scope='HOST state/packet/ownership only; actual Android link, no GPU/game',
                 sources={str(p): sha(p) for p in files}, runs=[])
    baseline = out / 'copy-baseline.c'
    original = (tests / 'copy_commands.c').read_text()
    if original.count('int main(void)') != 1:
        raise ValueError('Copy fixture main contract changed')
    baseline.write_text(original.replace('int main(void)', 'int copy_baseline_main(void)'))
    define = lambda key, value: '-D' + key + '="' + str(value) + '"'
    common = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
              '-I'+str(native/'include'), '-I'+str(native/'third_party/nvn'), '-I'+str(tests), '-pthread']
    modules = [native/'src'/(name+'.c') for name in ('order', 'compute', 'graphics', 'render_state', 'sampler_state', 'texture_source', 'texture_bridge', 'external_image', 'counter')]
    backend = tests/'memory_backend.c'
    timeline = tests/'timeline_backend.c'
    mem = define('NN_MEMORY_BACKEND_SOURCE', backend)
    copy_defs = [define('NN_COPY_BASELINE', baseline), define('NN_TEST_NVN_SOURCE', native/'src/nvn.c'),
                 define('NN_DEVICE_MODES_FIXTURE', tests/'device_modes.c'),
                 define('NN_MODE_IMAGE_HEADER', args.image_header.resolve()), mem, '-Wl,--wrap=malloc']
    counter_defs = [define('NN_RESOURCE_CORE', native/'src/nvn.c'), define('NN_RESOURCE_MEMORY_BACKEND', backend),
                    define('NN_COUNTER_RESOURCE_FIXTURE', tests/'resource_copy.c'),
                    define('NN_COUNTER_IMAGE', args.counter_image.resolve())]
    fixtures = [
        ('copy-counter', [tests/'copy_counter.c', timeline, *modules], copy_defs, 513),
        ('counter', [tests/'counter_core.c', *modules], counter_defs, 104),
        ('counter-words', [tests/'counter_words.c', native/'src/counter.c', native/'src/order.c'], [], 137),
        ('sdk-defaults', [tests/'defaults_recording.c', backend, *modules],
         [define('NN_CORE', native/'src/nvn.c')], 103),
        ('queue', [tests/'queue_family.c', timeline, *modules], [define('NN_CORE', native/'src/nvn.c'), mem], 4252),
        ('defaults', [tests/'queue_defaults.c', timeline, *modules],
         [define('NN_QUEUE_CORE', native/'src/nvn.c'), mem, '-Wl,--wrap=nn_queue_create', '-Wl,--wrap=nn_queue_destroy'], 63)]
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    def run(label, argv):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=root, text=True, capture_output=True, env=env)
        proof['runs'].append(dict(label=label, argv=argv, returncode=result.returncode,
                                  stdout=result.stdout, stderr=result.stderr))
        if result.returncode:
            raise RuntimeError(label+' failed')
        return result.stdout
    try:
        for compiler, sanitizer in [('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                             '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']),
                                     ('clang', ['-fsanitize=function', '-fsanitize-trap=function'])]:
            for label, sources, definitions, expected in fixtures:
                binary = out/(label+'-'+compiler)
                run(label+' '+compiler+' compile', [compiler, *common, *sanitizer, *definitions, *sources, '-lm', '-o', binary])
                result = json.loads(run(label+' '+compiler+' execute', [binary]).strip().splitlines()[-1])
                valid_count = (result['checks'] >= expected and result.get('retired_abort_cases') == 1
                               if label == 'copy-counter' else result['checks'] == expected)
                if result.get('passed') is not True or result.get('gpu_submits', 0) or not valid_count:
                    raise ValueError('fixture scope/count changed: '+label)
                proof['runs'][-1]['result'] = result
        # Compile the actual KGSL/Android implementation, with no RAM symbols.
        android = [native/'src/nvn.c', native/'src/kgsl.c', *modules,
                   *[native/'android'/(name+'.c') for name in ('window_session', 'window_android', 'present', 'present_pacer', 'window_display', 'ahb')]]
        binary = out/'libnative-command-families-api29.so'
        proof['ndk_clang_sha256'] = sha(args.ndk_clang.resolve())
        run('NDK actual backend strict full link', [args.ndk_clang.resolve(), *common, '-O2',
            '-I'+str(native/'third_party/kgsl'), '-fPIC', '-shared', '-Wl,--no-undefined', *android, '-landroid', '-ldl', '-o', binary])
        proof['binary_sha256'] = sha(binary)
        if any(sha(path) != expected for path, expected in proof['sources'].items()):
            raise ValueError('sources changed during checks')
        proof['passed'] = True
    except Exception as error:
        proof['error'] = str(error)
    (out/'proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    print(json.dumps(dict(passed=proof['passed'], proof=str(out/'proof.json'))))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
