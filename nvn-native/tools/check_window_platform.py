#!/usr/bin/env python3
"""Check tracked Window session/identity ownership; no Android or GPU execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

NATIVE = Path(__file__).resolve().parents[1]
PACER_CASES = ('normal','close','timeout','unavailable','protocol','create-looper','create-choreographer')
CASES = ('normal2', 'normal3', 'ticket-recreate', 'alias', 'release', 'close-wake',
         'concurrent', 'close-create', 'interval-snapshot', 'interval-mailbox', 'interval-close', 'interval-init', 'timeout', 'create-fail', 'show-fail',
         'detach-fail', 'stats-fail')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path,
                        default=NATIVE.parent / '.local/nvn-native/window-platform-check')
    parser.add_argument('--cc', default='cc', help='Host C compiler executable')
    parser.add_argument('--sanitizers', action='store_true',
                        help='Also run independent AddressSanitizer and UndefinedBehaviorSanitizer builds')
    parser.add_argument('--ndk', type=Path, help='Also strictly link ARM64 API29 against this NDK')
    args = parser.parse_args()
    out = args.out.resolve()
    ignored = (NATIVE.parent / '.local').resolve()
    if out == ignored or not out.is_relative_to(ignored) or out.exists():
        parser.error('--out must be a new directory below this worktree .local')
    out.mkdir(parents=True, exist_ok=False)
    source_names = ('tools/check_window_platform.py', 'tests/d2r_bridge/window_session.c',
                    'tests/d2r_bridge/window_identity.c', 'android/window_session.c',
                    'android/window_platform.h', 'include/nvn_native/window_session.h',
                    'src/external_image.c', 'include/nvn_native/external_image.h',
                    'include/nvn_native/kgsl.h')
    files = [NATIVE / name for name in source_names]
    files += [NATIVE / name for name in ('tests/d2r_bridge/present_pacer.c',
        'android/present_pacer.c','android/present_pacer.h',
        'tests/host_android/android/choreographer.h','tests/host_android/android/looper.h',
        'tests/host_android/android/surface_control.h')]
    if args.ndk:
        files += [NATIVE / name for name in ('android/window_android.c', 'android/present.c',
                  'android/present.h', 'android/present_pacer.c', 'android/present_pacer.h', 'android/ahb.h', 'src/kgsl.c', 'third_party/kgsl/msm_kgsl.h')]
        files.append(args.ndk.resolve() / 'source.properties')
    before = {str(f): digest(f) for f in files}
    records, binaries = [], []

    def run(command, *, variant='normal', execute=False, identity=False):
        env = os.environ.copy()
        if variant == 'address':
            # Session error fixtures deliberately keep unknown owners to exit.
            # Identity fixtures retire every import and undergo the leak check.
            env['ASAN_OPTIONS'] = 'detect_leaks=' + ('1' if identity else '0')
        if variant == 'undefined':
            env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
        result = subprocess.run(list(map(str, command)), capture_output=True, text=True,
                                timeout=30, env=env)
        row = dict(command=list(map(str, command)), variant=variant,
                   returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        if execute and variant == 'address':
            row['asan_options'] = env['ASAN_OPTIONS']
        if execute and variant == 'undefined':
            row['ubsan_options'] = env['UBSAN_OPTIONS']
        records.append(row)
        (out / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError(result.stderr or result.stdout or 'Window platform check failed')
        if execute:
            value = json.loads(result.stdout)
            if (type(value.get('gpu_executes')) is not int or value['gpu_executes'] != 0 or
                    type(value.get('checks')) is not int or value['checks'] <= 0):
                raise RuntimeError('host result does not declare a bounded no-GPU test')
        return result

    compiler = shutil.which(args.cc)
    if not compiler:
        parser.error('host C compiler is unavailable: ' + args.cc)
    compiler_path = Path(compiler).resolve()
    compiler_record = dict(path=str(compiler_path), sha256=digest(compiler_path),
                           version=run([compiler, '--version']).stdout)
    includes = ['-I' + str(NATIVE / 'include'), '-I' + str(NATIVE / 'android'),
                '-I' + str(NATIVE / 'include/nvn_native')]
    variants = ('normal', 'address', 'undefined') if args.sanitizers else ('normal',)
    for variant in variants:
        flags = ['-std=c11', '-O2' if variant == 'normal' else '-O1',
                 '-Wall', '-Wextra', '-Werror', '-pthread', *includes]
        if variant != 'normal':
            flags += ['-fsanitize=' + variant, '-fno-omit-frame-pointer']
        identity = out / (variant + '-identity')
        session = out / (variant + '-session')
        run([compiler, *flags, NATIVE / 'tests/d2r_bridge/window_identity.c',
             NATIVE / 'src/external_image.c', NATIVE / 'android/window_session.c',
             '-Wl,--wrap=fstat', '-o', identity], variant=variant)
        binaries.append(identity)
        run([identity], variant=variant, execute=True, identity=True)
        run([compiler, *flags, NATIVE / 'tests/d2r_bridge/window_session.c', '-o', session],
            variant=variant)
        binaries.append(session)
        for case in CASES:
            result = run([session, case], variant=variant, execute=True)
            if json.loads(result.stdout).get('case') != case:
                raise RuntimeError('host session case identity differs')
        # HOST fake declarations are local to this compilation only. They are
        # never appended to shared includes/flags or the actual NDK command.
        pacer = out / (variant + '-pacer')
        run([compiler, *flags, '-I'+str(NATIVE/'tests/host_android'),
             '-DNN_PRESENT_PACER_SOURCE="'+str(NATIVE/'android/present_pacer.c')+'"',
             NATIVE/'tests/d2r_bridge/present_pacer.c','-ldl','-o',pacer],variant=variant)
        binaries.append(pacer)
        for case in PACER_CASES:
            result=run([pacer,case],variant=variant,execute=True,identity=True)
            if json.loads(result.stdout).get('case')!=case:
                raise RuntimeError('HOST pacer case identity differs')
    ndk_record = None
    if args.ndk:
        tool = args.ndk.resolve() / 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        linked = out / 'libwindow-platform-api29.so'
        ndk_record = dict(path=str(tool), sha256=digest(tool),
                          version=run([tool, '--version']).stdout)
        run([tool, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-fPIC', '-shared',
             '-Wl,--no-undefined', *includes, '-I' + str(NATIVE / 'third_party/kgsl'),
             NATIVE / 'android/window_session.c', NATIVE / 'android/window_android.c',
             NATIVE / 'src/external_image.c', NATIVE / 'android/present.c', NATIVE / 'android/present_pacer.c', NATIVE / 'src/kgsl.c',
             '-landroid', '-ldl', '-o', linked])
        binaries.append(linked)
        # API29 dynamically resolves the newer functions. API33 additionally
        # checks their exact public NDK types without HOST declaration overlays.
        type_tool = tool.with_name('aarch64-linux-android33-clang')
        type_object = out / 'present-pacer-api33.o'
        ndk_record['api33_type_compiler'] = dict(path=str(type_tool), sha256=digest(type_tool))
        run([type_tool, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-fPIC',
             *includes, '-c', NATIVE / 'android/present_pacer.c', '-o', type_object])
        binaries.append(type_object)
    after = {str(f): digest(f) for f in files}
    if before != after:
        raise RuntimeError('Window sources changed during checks; rerun on a stable source tree')
    proof = dict(schema=1, passed=True, scope='host ownership tests and optional API29 link only',
                 gpu_executed=False, android_executed=False, game_modules_executed=False,
                 session_cases=list(CASES),pacer_cases=list(PACER_CASES), variants=list(variants), api29_link_checked=bool(args.ndk),
                 host_compiler=compiler_record, ndk_compiler=ndk_record,
                 sources_sha256=after, sources_stable=True,
                 artifacts_sha256={str(f): digest(f) for f in binaries},
                 results_sha256=digest(out / 'results.json'))
    (out / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(dict(passed=True, host_only=True, proof=str(out / 'proof.json'))))


if __name__ == '__main__':
    main()
