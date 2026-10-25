#!/usr/bin/env python3
"""Normal CPU-metadata-only runtime FDL C qualifier; builds, never runs Android."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def module(path):
    spec = importlib.util.spec_from_file_location('nn_runtime_fdl_cpu_builder', path)
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def verify_hashes(items):
    for name, value in items.items():
        if sha(name) != value:
            raise ValueError('changed input/artifact ' + name)


def metadata(path):
    data = Path(path).read_bytes()
    if len(data) < 40 or data[:8] != b'NNFDMETA':
        raise ValueError('invalid CPU metadata header')
    if struct.unpack_from('<8I', data, 8) != (1, 456, 48, 26, 64, 680, 24, 120):
        raise ValueError('unexpected C-ABI or case counts')
    offset, counts = 40, {1: 0, 2: 0, 3: 0, 4: 0}
    while offset < len(data):
        if len(data) - offset < 24:
            raise ValueError('short record header')
        kind, index, null, dn, vn, error = struct.unpack_from('<6I', data, offset)
        offset += 24
        if kind not in counts or null > 1 or (dn, vn) != ((64, 680) if kind in (1, 3) else (24, 120)):
            raise ValueError('invalid record shape')
        if len(data) - offset < dn + vn:
            raise ValueError('short record value')
        if (kind in (1, 2)) != (error == 0):
            raise ValueError('result/error mismatch')
        expected_index = counts[kind] if kind in (1, 2, 3) else counts[3] + counts[4]
        if index != expected_index:
            raise ValueError('record index/order mismatch')
        if kind in (3, 4) and any(data[offset + dn:offset + dn + vn]):
            raise ValueError('failure output not zero')
        counts[kind] += 1
        offset += dn + vn
    if counts != {1: 456, 2: 48, 3: 19, 4: 7}:
        raise ValueError('incomplete metadata matrix')
    return len(data)


def result(path):
    value = json.loads(Path(path).read_text())
    expected = dict(schema=1, operation='runtime_fdl_cpu_metadata', passed=True,
                    metadata_version=1, images=456, buffers=48, negative_cases=26,
                    records=530, gpu_submits=0, gpu_allocations=0, device_opens=0,
                    image_data_access=False)
    if any(value.get(k) != v or type(value.get(k)) is not type(v) for k, v in expected.items()):
        raise ValueError('CPU fixture failed or scope/counters inconsistent')
    if not isinstance(value.get('checks'), int) or value['checks'] < 5000:
        raise ValueError('missing fixture assertions')
    return value


def produce(args):
    root, out = args.root.resolve(), args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        raise ValueError('fresh ignored output below ROOT/.local required')
    normal = module(root / 'nvn-native/tools/build_runtime_fdl.py')
    archive, check = normal.verify(root, args.runtime_fdl_check.resolve())
    if check.get('passed') is not True:
        raise ValueError('runtime FDL normal producer did not pass')
    native = root / 'nvn-native'
    fixture = Path(__file__).resolve().parents[1] / 'tests/runtime_fdl_cpu.c'
    source = native / 'src/runtime_fdl.cc'
    header = native / 'include/nvn_native/runtime_fdl.h'
    ir3 = Path(check['normal_ir3'])
    host, ndk = ir3 / 'build', Path(check['ndk'])
    tools = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
    compiler = tools / 'aarch64-linux-android29-clang'
    ninja = shutil.which('ninja')
    if not ninja:
        raise ValueError('normal ninja unavailable')
    inputs = {str(args.runtime_fdl_check.resolve()): sha(args.runtime_fdl_check)}
    inputs.update(check['inputs'])
    inputs.update(check['artifacts'])
    for path in [Path(__file__).resolve(), fixture, source, header, host / 'compile_commands.json',
                 compiler.resolve(), tools / 'llvm-readelf', tools / 'llvm-nm', Path(ninja).resolve()]:
        inputs[str(path)] = sha(path)
    out.mkdir(parents=True)
    commands = []
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1')

    def run(argv, cwd=None):
        argv = list(map(str, argv))
        process = subprocess.run(argv, cwd=cwd, text=True, capture_output=True, env=env)
        row = dict(argv=argv, cwd=str(cwd) if cwd else None, returncode=process.returncode,
                   stdout=process.stdout, stderr=process.stderr)
        commands.append(row)
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if process.returncode:
            raise ValueError('command failed: ' + shlex.join(argv) + '\n' + process.stdout + process.stderr)
        return process.stdout

    entry = next(e for e in json.loads((host / 'compile_commands.json').read_text())
                 if e['file'].endswith('/fd6_view.cc'))
    _, hf = normal.flags(entry)
    link = shlex.split(run([ninja, '-C', host, '-t', 'commands', 'src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1])
    libs, skip = [], False
    for item in link[1:]:
        if skip:
            skip = False
            continue
        if item == '-o':
            skip = True
            continue
        if not item.endswith('.o'):
            libs.append(item)
    for item in libs:
        path = (host / item).resolve()
        if path.is_file():
            inputs[str(path)] = sha(path)
    host_files = []
    for cc, cpp, sanitizer in (
            ('gcc', 'g++', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']),
            ('clang', 'clang++', ['-fsanitize=function', '-fsanitize-trap=function'])):
        for tool in (cc, cpp):
            inputs[str(Path(shutil.which(tool)).resolve())] = sha(Path(shutil.which(tool)).resolve())
        folder = out / cc
        folder.mkdir()
        obj, facade, binary = folder / 'fixture.o', folder / 'facade.o', folder / 'runtime-fdl-cpu'
        run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-pie',
             '-I' + str(native / 'include'), *sanitizer, '-c', fixture, '-o', obj])
        flags = hf if cpp == 'g++' else [x for x in hf if x not in ('-flifetime-dse=1', '-Wno-error=maybe-uninitialized')]
        run([cpp, *flags, '-I' + str(native / 'include'), '-Werror', '-fno-pie',
             *sanitizer, '-c', source, '-o', facade], host)
        run([cpp, '-no-pie', *sanitizer, obj, facade, *libs, '-o', binary], host)
        for repeat in range(2):
            data, summary = folder / ('metadata-' + str(repeat) + '.bin'), folder / ('result-' + str(repeat) + '.json')
            run([binary, data, summary], host)
            metadata(data)
            parsed = result(summary)
            host_files.append((data, parsed))
    expected = host_files[0][0]
    for path, summary in host_files:
        if path.read_bytes() != expected.read_bytes() or summary != host_files[0][1]:
            raise ValueError('HOST sanitizer/replay metadata differs')
    # Existing normal HOST reference independently compares all the same positive
    # inputs to direct original FDL API. No new layout algorithm in the fixture.
    reference = args.runtime_fdl_check.resolve().parent / 'host-clangpp'
    reference_text = run([reference], host)
    if 'images=456 buffers=48' not in reference_text:
        raise ValueError('normal direct HOST FDL reference case matrix changed')
    android = out / 'runtime-fdl-cpu-api29'
    run([compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-fPIE', '-pie',
         '-I' + str(native / 'include'), fixture, archive, '-Wl,--no-undefined',
         '-Wl,--gc-sections', '-lm', '-o', android])
    elf = run([tools / 'llvm-readelf', '-h', android])
    dynamic = run([tools / 'llvm-readelf', '-d', android])
    undefined = run([tools / 'llvm-nm', '-u', android])
    if 'AArch64' not in elf or 'DYN' not in elf:
        raise ValueError('Android fixture is not ARM64 PIE')
    if not set(re.findall(r'Shared library: \[(.*?)\]', dynamic)) <= {'libc.so', 'libdl.so', 'libm.so'}:
        raise ValueError('unexpected Android dependency; metadata must link static FDL')
    if any(re.search(r'\b' + symbol + r'(?:@|\s|$)', undefined) for symbol in ('ioctl', 'socket', 'dlopen', 'mmap', 'open', 'open64')):
        raise ValueError('device/driver access dependency entered standalone fixture')
    (out / 'elf-report.json').write_text(json.dumps(dict(header=elf, dynamic=dynamic, undefined=undefined), indent=2) + '\n')
    shutil.copyfile(expected, out / 'expected-metadata.bin')
    (out / 'expected-result.json').write_text(json.dumps(host_files[0][1], indent=2) + '\n')
    verify_hashes(inputs)
    artifacts = {str(p): sha(p) for p in out.rglob('*') if p.is_file()}
    record = dict(schema=1, kind='runtime-fdl-cpu-qualifier-v1', passed=True,
                  android_execution=False, gpu_execution=False, scope=__doc__,
                  inputs=inputs, artifacts=artifacts, executable=str(android),
                  expected_metadata=str(out / 'expected-metadata.bin'),
                  expected_result=str(out / 'expected-result.json'),
                  metadata_bytes=metadata(expected), images=456, buffers=48, negative_cases=26,
                  host_fixture_checks=host_files[0][1]['checks'], host_runs=4,
                  direct_original_fdl_reference=True,
                  run_arguments=['METADATA_BIN_NEW_FILE', 'RESULT_JSON_NEW_FILE'])
    (out / 'check.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps(dict(passed=True, check=str(out / 'check.json'), android_execution=False)))


def compare(args):
    check = json.loads(args.check.read_text())
    if check.get('kind') != 'runtime-fdl-cpu-qualifier-v1' or check.get('passed') is not True:
        raise ValueError('unverified qualifier package')
    verify_hashes(check['inputs'])
    verify_hashes(check['artifacts'])
    if args.out.exists():
        raise ValueError('fresh comparison output required')
    size = metadata(args.metadata)
    summary = result(args.result)
    if summary != result(check['expected_result']):
        raise ValueError('Android/HOST counters differ')
    observed, expected = args.metadata.read_bytes(), Path(check['expected_metadata']).read_bytes()
    if observed != expected:
        first = next((i for i, (a, b) in enumerate(zip(observed, expected)) if a != b), min(len(observed), len(expected)))
        raise ValueError('Android/HOST returned value differs at byte ' + str(first))
    if size != check['metadata_bytes']:
        raise ValueError('Android metadata size differs')
    # Collection/launch/UID evidence is supplied independently by root. Merely
    # passing copied HOST files here is never Android execution evidence.
    proof = dict(schema=1, operation='runtime_fdl_cpu_metadata_compare', passed=True,
                 cpu_metadata_equal=True, android_execution_proven_by_comparison_alone=False,
                 gpu_execution=False, images=456, buffers=48, negative_cases=26,
                 check_sha256=sha(args.check), metadata_sha256=sha(args.metadata),
                 result_sha256=sha(args.result), bytes=size,
                 inputs={str(args.check.resolve()): sha(args.check), str(args.metadata.resolve()): sha(args.metadata),
                         str(args.result.resolve()): sha(args.result)})
    args.out.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(dict(passed=True, output=str(args.out), gpu_execution=False)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('build')
    for option in ('root', 'runtime-fdl-check', 'out'):
        p.add_argument('--' + option, type=Path, required=True)
    p = sub.add_parser('compare')
    for option in ('check', 'metadata', 'result', 'out'):
        p.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    try:
        (produce if args.command == 'build' else compare)(args)
    except (ValueError, OSError, StopIteration, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
