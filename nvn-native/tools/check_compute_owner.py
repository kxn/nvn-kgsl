#!/usr/bin/env python3
"""HOST common-compute failure-owner regression + strict ARM64 linking; no GPU."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def check(root, kernel, ndk, out):
    if out.exists() or not out.is_relative_to(root / '.local'):
        raise ValueError('fresh ignored output below ROOT/.local required')
    producer = root / 'nvn-native/tools/compile_block_copy.py'
    spec = importlib.util.spec_from_file_location('nn_owner_normal_kernel', producer)
    package_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(package_module)
    package = package_module.verify(root, kernel)
    image = Path(package['header'])
    fixture = Path(__file__).resolve().parents[1] / 'tests/d2r_bridge/compute_owner.c'
    compute = root / 'nvn-native/src/compute.c'
    headers = [root / 'nvn-native/include/nvn_native' / name for name in ('compute.h', 'compute_image.h', 'kgsl.h', 'pm4.h')]
    backend = root / 'nvn-native/src/kgsl.c'
    vendor = root / 'nvn-native/third_party/kgsl/msm_kgsl.h'
    cc = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
    readelf = cc.parent / 'llvm-readelf'
    inputs = {str(kernel): sha(kernel), **package['inputs'], **package['artifacts']}
    for p in [Path(__file__).resolve(), fixture, compute, *headers, producer, backend, vendor, cc.resolve(), readelf]:
        inputs[str(p)] = sha(p)
    out.mkdir(parents=True)
    commands, results = [], []

    def run(argv):
        argv = list(map(str, argv))
        process = subprocess.run(argv, capture_output=True, text=True,
                                 env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                                          UBSAN_OPTIONS='halt_on_error=1'))
        commands.append(dict(argv=argv, returncode=process.returncode, stdout=process.stdout, stderr=process.stderr))
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if process.returncode:
            raise ValueError('command failed: ' + ' '.join(argv) + '\n' + process.stderr[-4000:])
        return process.stdout

    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I' + str(root / 'nvn-native/include')]
    defines = ['-DNN_COMPUTE_OWNER_IMAGE=' + json.dumps(str(image))]
    for compiler, sanitizer in (('gcc', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']),
                                ('clang', ['-fsanitize=function', '-fsanitize-trap=function'])):
        path = Path(shutil.which(compiler)).resolve()
        inputs[str(path)] = sha(path)
        binary = out / ('owner-' + compiler)
        run([compiler, *flags, *defines, *sanitizer, '-fno-pie', '-no-pie', fixture, compute, '-o', binary])
        value = json.loads(run([binary]))
        if (value.get('passed') is not True or value.get('gpu_execution') is not False or
                value.get('live') != 0 or value.get('allocations') != value.get('destroys') or
                value.get('retained_error_owners') != value.get('retried_error_owners') or
                value.get('retained_error_owners', 0) < 20 or
                value.get('direct_success') != 1 or value.get('indirect_success') != 1):
            raise ValueError('owner fixture incomplete/failed')
        results.append(dict(compiler=compiler, **value))
    if {k: v for k, v in results[0].items() if k != 'compiler'} != {k: v for k, v in results[1].items() if k != 'compiler'}:
        raise ValueError('sanitizer fixture counters differ')
    run([cc, *flags, *defines, fixture, compute, '-Wl,--no-undefined', '-o', out / 'owner-fixture-api29'])
    run([cc, *flags, '-fPIC', '-shared', compute, backend,
         '-I' + str(root / 'nvn-native/third_party/kgsl'), '-Wl,--no-undefined', '-o', out / 'compute-real-kgsl-api29.so'])
    elf = run([readelf, '-h', '-d', out / 'compute-real-kgsl-api29.so'])
    if 'AArch64' not in elf or any(v in elf.lower() for v in ('libvulkan', 'libegl', 'libgles')):
        raise ValueError('unexpected real backend ARM64 link')
    (out / 'elf.txt').write_text(elf)
    for p, digest in inputs.items():
        if sha(p) != digest:
            raise ValueError('source/artifact changed during fixture ' + p)
    result = dict(schema=1, kind='compute-owner-fault-host-v1', passed=True,
                  gpu_execution=False, android_execution=False, normal_kernel_artifacts_unchanged=True,
                  scope=__doc__, inputs=inputs, results=results,
                  artifacts={str(p): sha(p) for p in out.rglob('*') if p.is_file()})
    (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
    return out / 'check.json'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('root', 'kernel-check', 'ndk', 'out'):
        parser.add_argument('--' + option, required=True, type=Path)
    args = parser.parse_args()
    try:
        print(check(args.root.resolve(), args.kernel_check.resolve(), args.ndk.resolve(), args.out.resolve()))
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')
