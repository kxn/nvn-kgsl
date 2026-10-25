#!/usr/bin/env python3
"""HOST canonical texture consumer/Window ownership and real Android strict link.

Uses the production implementation and existing RAM/failure fixtures; submits
no GPU commands. Header inputs must be explicit normal producer outputs. Their
complete sibling-header fingerprints are recorded; this checker does not prove
shader compilation or original SDK behavior.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--image-header', type=Path, required=True)
    p.add_argument('--bundle-header', type=Path, required=True)
    p.add_argument('--probe-header', type=Path, required=True)
    p.add_argument('--ndk-clang', type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    n, out = root/'nvn-native', a.out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):
        p.error('fresh ignored output required')
    out.mkdir(parents=True)
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    files = [f for folder in ('src', 'include', 'android', 'third_party', 'tests/d2r_bridge')
             for f in (n/folder).rglob('*') if f.is_file() and f.suffix in ('.c', '.h', '.inc', '.S')]
    files += [n/'tests/host_android/android/hardware_buffer.h', Path(__file__).resolve(), a.ndk_clang.resolve(), n/'tests/nvn_texture_bridge_probe.c', n/'tests/nvn_texture_bridge_probe.h']
    for header in (a.image_header, a.bundle_header, a.probe_header):
        files += list(header.resolve().parent.glob('*.h'))
    inputs = {str(f): sha(f) for f in files}
    proof = dict(passed=False, scope=__doc__, inputs=inputs, runs=[])
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')

    def define(name, value):
        return '-D'+name+'="'+str(value)+'"'

    def run(label, argv):
        argv = list(map(str, argv))
        r = subprocess.run(argv, cwd=root, env=env, capture_output=True, text=True)
        entry = dict(label=label, argv=argv, returncode=r.returncode, stdout=r.stdout, stderr=r.stderr)
        proof['runs'].append(entry)
        if r.returncode:
            raise ValueError(label+' failed')
        return r.stdout

    flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
             *['-I'+str(n/d) for d in ('include', 'third_party/nvn', 'include/nvn_native', 'src', 'android')]]
    modules = [n/'src'/f'{x}.c' for x in ('texture_bridge', 'texture_source', 'compute', 'order', 'graphics', 'render_state', 'sampler_state', 'counter', 'external_image')]
    tests = n/'tests/d2r_bridge'
    fixtures = [
        ('words', [tests/'texture_bridge_words.c', *[n/'src'/f'{x}.c' for x in ('texture_bridge', 'texture_source', 'compute', 'order')]],
         [define('NN_BRIDGE_MEMORY_BACKEND', tests/'memory_backend.c'), define('NN_BRIDGE_BUNDLE', a.bundle_header.resolve()), '-Wl,--wrap=nn_bo_alloc', '-Wl,--wrap=nn_bo_destroy']),
        ('core', [tests/'texture_bridge_core.c', tests/'timeline_backend.c', *modules],
         [define('NN_TEST_NVN_SOURCE', n/'src/nvn.c'), define('NN_BRIDGE_FRONTEND', tests/'device_modes.c'), define('NN_MODE_IMAGE_HEADER', a.image_header.resolve()), define('NN_BRIDGE_BUNDLE', a.bundle_header.resolve()), define('NN_MEMORY_BACKEND_SOURCE', tests/'memory_backend.c')]),
        ('window', [tests/'window_source.c', *modules],
         [define('NN_WINDOW_MEMORY_BACKEND', tests/'memory_backend.c'), define('NN_WINDOW_CORE_SOURCE', n/'src/nvn.c'), define('NN_WINDOW_BRIDGE_BUNDLE', a.bundle_header.resolve())]),
    ]
    label,sources,definitions=fixtures[0]
    fixtures.append(('words-diagnostic',sources,[*definitions,'-DNN_TEX_BRIDGE_QUAL_DIAGNOSTIC=1']))
    try:
        for cc, san in [('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie']), ('clang', ['-fsanitize=function', '-fsanitize-trap=function'])]:
            for label, sources, definitions in fixtures:
                binary = out/(label+'-'+cc)
                run(label+' '+cc+' compile', [cc, '-I'+str(n/'tests/host_android'), *flags, *san, *definitions, *sources, '-lm', '-o', binary])
                row = json.loads(run(label+' '+cc+' execute', [binary]).strip().splitlines()[-1])
                if row.get('passed') is not True or not row.get('checks'):
                    raise ValueError(label+' fixture did not pass')
                proof['runs'][-1]['result'] = row
        probe_defs = [define('NVN_TEXTURE_API_HEADER', a.probe_header.resolve()), define('NVN_TEXTURE_BRIDGE_HEADER', a.bundle_header.resolve()), '-I'+str(n/'tests')]
        probe = n/'tests/nvn_texture_bridge_probe.c'
        binary = out/'probe-host-no-gpu'
        run('HOST probe failure build', ['gcc', '-I'+str(n/'tests/host_android'), *flags, '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie', *probe_defs, define('NN_MEMORY_BACKEND_SOURCE', tests/'memory_backend.c'), tests/'texture_bridge_probe_failure.c', n/'src/nvn.c', tests/'timeline_backend.c', *modules, probe, '-lm', '-o', binary])
        for failure in ('no-gpu', 'unknown'):
            directory = out/failure
            directory.mkdir()
            run('HOST probe '+failure, [binary, directory, failure])
        binary = out/'probe-host-diagnostic'
        run('HOST diagnostic probe failure build', ['gcc', '-I'+str(n/'tests/host_android'), *flags, '-DNN_TEX_BRIDGE_QUAL_DIAGNOSTIC=1', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-pie', '-no-pie', *probe_defs, define('NN_MEMORY_BACKEND_SOURCE', tests/'memory_backend.c'), tests/'texture_bridge_probe_failure.c', n/'src/nvn.c', tests/'timeline_backend.c', *modules, probe, '-lm', '-o', binary])
        for failure in ('no-gpu', 'unknown'):
            directory = out/('diagnostic-'+failure)
            directory.mkdir()
            data=json.loads(run('HOST diagnostic probe '+failure,[binary,directory,failure]))
            if data.get('diagnostic_instrumented') is not True or data.get('diagnostic_overflow') is not False:
                raise ValueError('diagnostic mode metadata')
            if failure=='no-gpu' and data.get('diagnostic_progress')!=[]:
                raise ValueError('retired wrappers must not be observed')
            if failure=='unknown' and data.get('diagnostic_progress')!=[]:
                raise ValueError('an ambiguous submit without accepted status must not expose wrapper pointers')
        shared = out/'libtexture-bridge-api29.so'
        run('NDK actual backend/probe strict link', [a.ndk_clang, *flags, '-O2', '-fPIC', '-shared', '-Wl,--no-undefined', '-I'+str(n/'third_party/kgsl'), *probe_defs, n/'src/nvn.c', n/'src/kgsl.c', *modules, probe, *[n/'android'/f'{x}.c' for x in ('window_session', 'window_android', 'present', 'present_pacer', 'window_display', 'ahb')], '-landroid', '-ldl', '-lm', '-o', shared])
        run('NDK actual diagnostic backend/probe strict link', [a.ndk_clang, *flags, '-O2', '-DNN_TEX_BRIDGE_QUAL_DIAGNOSTIC=1', '-fPIC', '-shared', '-Wl,--no-undefined', '-I'+str(n/'third_party/kgsl'), *probe_defs, n/'src/nvn.c', n/'src/kgsl.c', *modules, probe, *[n/'android'/f'{x}.c' for x in ('window_session', 'window_android', 'present', 'present_pacer', 'window_display', 'ahb')], '-landroid', '-ldl', '-lm', '-o', out/'libtexture-bridge-diagnostic-api29.so'])
        if any(sha(f) != v for f, v in inputs.items()):
            raise ValueError('inputs changed during checks')
        proof['artifacts'] = {str(f): sha(f) for f in out.iterdir() if f.is_file()}
        proof['passed'] = True
    except (ValueError, OSError) as error:
        proof['error'] = str(error)
    (out/'proof.json').write_text(json.dumps(proof, indent=2)+'\n')
    print(json.dumps(dict(passed=proof['passed'], proof=str(out/'proof.json'), gpu_submits=0)))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
