#!/usr/bin/env python3
"""Original-host depth policy/forwarding and real ABI1/ABI2 NDK links; no GPU/game."""
import argparse
import copy
import importlib.util
import json
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from workspace_paths import d2r_root
from native_depth_features import prepare, audit, validate_options, sha, SOURCES
from native_sampled_features import prepare as sampled_prepare, audit as sampled_audit


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check(args):
    root, ndk, out = args.root.resolve(), args.ndk.resolve(), args.out.resolve()
    native = root / 'nvn-native'
    if out.exists() or not out.is_relative_to(root / '.local'):
        raise ValueError('fresh ignored output required')
    out.mkdir(parents=True)
    assertions, rejections, commands, inputs = 0, [], [], {}

    def require(value):
        nonlocal assertions
        if not value:
            raise ValueError('depth HOST assertion failed')
        assertions += 1

    def reject(label, operation):
        try:
            operation()
        except (ValueError, OSError, KeyError):
            rejections.append(label)
            return
        raise ValueError('invalid depth feature accepted: ' + label)

    def pin(path):
        path = Path(path).resolve()
        value = sha(path)
        if str(path) in inputs and inputs[str(path)] != value:
            raise ValueError('depth input changed: ' + str(path))
        inputs[str(path)] = value

    def run(argv, expected=0):
        argv = list(map(str, argv))
        r = subprocess.run(argv, text=True, capture_output=True)
        commands.append(dict(argv=argv, returncode=r.returncode, stdout=r.stdout, stderr=r.stderr))
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if r.returncode != expected:
            raise ValueError((r.stdout + r.stderr)[-8000:])
        return r.stdout, r.stderr

    require(prepare(root, False, None, None, None, None, None, None, None, False) ==
            (['-DNVN_D2R_DEPTH_TARGETS=OFF', '-DNVN_D2R_DEPTH_LIBRARIES=',
              '-DNVN_D2R_DEPTH_SAMPLED=OFF'], None, {}))
    values = [args.depth_fdl_check, args.depth_a2d_check, args.depth_runtime_check,
              args.graphics_check, args.fdl_check, args.block_check, True]
    for mask in (0, *(127 ^ (1 << i) for i in range(7))):
        reject('missing depth prerequisites ' + str(mask),
               lambda mask=mask: validate_options(True, *[v if mask & (1 << i) else False if i == 6 else None for i, v in enumerate(values)]))
    validate_options(True, *values)
    require(True)
    reject('nonboolean depth', lambda: validate_options(1, *values))
    reject('nonboolean depth sampling', lambda: validate_options(True, *values, 1))
    reject('sampling without depth', lambda: validate_options(False, None, None, None, None, None, None, False, True))
    for index in range(3):
        checks = [None, None, None]
        checks[index] = values[index]
        reject('unrequested depth helper ' + str(index),
               lambda checks=checks: validate_options(False, *checks, None, None, None, False))
    fixture = out / 'verification-fixture'
    fixture.write_text('HOST command forwarding fixture\n')
    inventory = {str(fixture): sha(fixture)}
    options = dict(host_abi=2, rewrite_abi='elfdesc', rules='native', private_gates='1',
                   abi='arm64-v8a', execute=False, deploy=False, library_kind='shared',
                   python=sys.executable, exefs='/original/exefs', ndk=str(ndk), jobs=4)
    host = dict(options=options, modules=str(out / 'modules'), artifacts=inventory,
                sources=inventory, tools=inventory)
    apk = load(native / 'tools/build_d2r_activity_apk.py', 'depth_features_apk')
    cli = load(native / 'tools/run_d2r_host.py', 'depth_features_cli')
    for enabled in (False, True):
        h = copy.deepcopy(host)
        h['options'].update(depth_targets=enabled,
                            depth_sampled=enabled,
                            depth_fdl_check=str(args.depth_fdl_check.resolve()) if enabled else None,
                            depth_a2d_check=str(args.depth_a2d_check.resolve()) if enabled else None,
                            depth_runtime_check=str(args.depth_runtime_check.resolve()) if enabled else None)
        for fn, abi, rewrite in ((apk.host_verify, 2, 'elfdesc'), (cli.host_verify_argv, 1, 'elf')):
            h['options'].update(host_abi=abi, rewrite_abi=rewrite)
            argv = fn(h, root)
            require(('--depth-targets' in argv) == enabled)
            require(('--depth-sampled' in argv) == enabled)
            for field in ('depth_fdl_check', 'depth_a2d_check', 'depth_runtime_check'):
                flag = '--' + field.replace('_', '-')
                require((flag in argv) == enabled)
                if enabled:
                    require(argv[argv.index(flag) + 1] == h['options'][field])
        h['options']['depth_targets'] = 'false'
        reject('nonboolean forwarded depth CLI', lambda h=h: cli.host_verify_argv(h, root))
        h['options'].update(host_abi=2, rewrite_abi='elfdesc')
        reject('nonboolean forwarded depth APK', lambda h=h: apk.host_verify(h, root))
        h['options'].update(depth_targets=enabled, depth_sampled='false')
        reject('nonboolean forwarded depth sampling APK', lambda h=h: apk.host_verify(h, root))
        h['options'].update(host_abi=1, rewrite_abi='elf')
        reject('nonboolean forwarded depth sampling CLI', lambda h=h: cli.host_verify_argv(h, root))
    for script in ('build_d2r_host.py', 'build_d2r_activity.py'):
        argv = [sys.executable, native / 'tools' / script, '--exefs', '/must-not-read',
                '--ndk', ndk, '--out', out / 'must-not-build']
        for extra, message in ((['--depth-targets'], '--depth-targets requires'),
                               (['--depth-sampled'], '--depth-sampled requires'),
                               (['--depth-a2d-check', str(args.depth_a2d_check)], 'depth helper checks require')):
            _, stderr = run([*argv, *extra], expected=2)
            require(message in stderr and not (out / 'must-not-build').exists())
            rejections.append(script + ': ' + message)
    for name in ('native_depth_features.py', 'check_native_depth_features.py',
                 'build_d2r_host.py', 'build_d2r_activity.py',
                 'build_d2r_activity_apk.py', 'run_d2r_host.py'):
        pin(native / 'tools' / name)
    pin(Path(sys.executable).resolve())
    if args.policy_only:
        result = dict(schema=1, kind='original-host-native-depth-policy-only-v1', passed=True,
                      assertions=assertions, rejections=rejections, actual_NDK_links=0,
                      inputs=inputs, artifacts={str(out / 'commands.json'): sha(out / 'commands.json')},
                      GPU_execution=False, original_module_execution=False)
        (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
        return out / 'check.json'

    library, fdl = load(native / 'tools/build_runtime_fdl.py', 'depth_features_fdl').verify(root, args.fdl_check.resolve())
    header, graphics = load(native / 'tools/graphics_pipeline_cache.py', 'depth_features_graphics').build(root, args.graphics_check.resolve())
    flags, manifest, mapping = prepare(root, True, args.depth_fdl_check, args.depth_a2d_check,
                                       args.depth_runtime_check, args.graphics_check,
                                       args.fdl_check, args.block_check, library, True)
    inputs.update(mapping)
    require(manifest['depth_targets'] and not manifest['gpu_qualification'])
    for folder in (native, d2r_root() / 'dm/host', d2r_root() / 'dm/third_party/runtime-codecs', d2r_root() / 'dm/third_party/libdeflate'):
        for p in folder.rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:
                pin(p)
    for p in (args.names, d2r_root() / 'dm/gfx/hle_vi.c', d2r_root() / 'dm/gfx/hle_nv.c'):
        pin(p)
    inputs.update(graphics['inputs'])
    inputs.update(fdl['inputs'])
    inputs.update(fdl['artifacts'])
    kernel = load(native / 'tools/compile_block_copy.py', 'depth_features_kernel').verify(root, args.block_check.resolve())
    results = []
    for mode, enabled, sampled, depth_sampled in (
            ('host', True, False, False), ('host', True, True, True),
            ('activity', True, True, True), ('activity', False, False, False)):
        dest = out / (mode + ('-on' if enabled else '-off') + ('-depth-sampled' if depth_sampled else ''))
        target = 'dmhost_activity_kgsl' if mode == 'activity' else 'dmhost_native_kgsl'
        localflags, localmanifest, _ = prepare(root, enabled,
            args.depth_fdl_check if enabled else None, args.depth_a2d_check if enabled else None,
            args.depth_runtime_check if enabled else None, args.graphics_check, args.fdl_check,
            args.block_check, library, True, depth_sampled)
        sampledflags, sampledmanifest, _ = sampled_prepare(root, sampled, sampled, sampled,
            args.graphics_check, args.fdl_check, args.block_check, None, library, None, True)
        run(['cmake', '-S', native / 'adapters/d2r' / mode, '-B', dest, '-G', 'Ninja',
             '-DCMAKE_TOOLCHAIN_FILE=' + str(ndk / 'build/cmake/android.toolchain.cmake'),
             '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-29',
             '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
             '-DNVN_D2R_NAMES_HEADER=' + str(args.names.resolve()),
             '-DNVN_D2R_GRAPHICS_PIPELINE_HEADER=' + str(header),
             '-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY=' + str(library),
             '-DNVN_D2R_BLOCK_COPY_HEADER=' + kernel['header'], '-DNVN_D2R_COLOR_TARGETS=ON',
             *sampledflags, *localflags])
        run(['cmake', '--build', dest, '--target', target, '--parallel', '4'])
        actual, _ = run(['ninja', '-C', dest, '-t', 'commands', target])
        audit(native, localmanifest, actual)
        sampled_audit(native, sampledmanifest, actual)
        bad = actual + '\n' + shlex.join(['clang', '-c', str(native / 'src/depth_shadow.c'), '-o', 'duplicate.o'])
        reject('duplicate/unrequested depth owner ' + mode + str(enabled), lambda bad=bad: audit(native, localmanifest, bad))
        if enabled:
            for macro in (('NN_NVN_DEPTH_TARGETS', 'NN_PIPELINE_DEPTH_NATIVE') +
                          (('NN_NVN_DEPTH_SAMPLED', 'NN_PIPELINE_DEPTH_SAMPLED') if depth_sampled else ())):
                reject('missing actual macro ' + macro, lambda macro=macro: audit(native, localmanifest, actual.replace('-D' + macro + '=1', '')))
            if depth_sampled:
                for name in ('depth_sampled.c', 'depth_sampled_native.c'):
                    bad_source = actual + '\n' + shlex.join(['clang', '-c', str(native / 'src' / name), '-o', 'duplicate.o'])
                    reject('duplicate sampled source ' + mode + name, lambda bad_source=bad_source: audit(native, localmanifest, bad_source))
            reject('missing actual depth helper link', lambda: audit(native, localmanifest, actual.replace(manifest['libraries'][-1], '/unverified/depth.o')))
            reject('different actual depth archive', lambda: audit(native, localmanifest, actual.replace(manifest['graphics_header'], '/unverified/pipelines.h')))
        compiled = []
        for line in actual.splitlines():
            args_ = shlex.split(line)
            if '-c' in args_:
                source = Path(args_[args_.index('-c') + 1]).resolve()
                compiled.append(source)
                pin(source)
        hs = [Path(p).resolve() for p in (dest / 'host-sources.txt').read_text().splitlines()]
        require(len(hs) == len(set(hs)) == 36 and all(p in compiled for p in hs))
        require(not any('/tests/' in str(p) or '/gfx/metal/' in str(p) or '/gfx/vulkan/' in str(p) for p in compiled))
        elf = dest / ('libdmhost_activity_kgsl.so' if mode == 'activity' else 'dmhost_native_kgsl')
        readelf = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
        info, _ = run([readelf, '-h', '-d', '-s', '--wide', elf])
        require('AArch64' in info and not any(x in info.lower() for x in ('libvulkan', 'libegl', 'libgles', 'libstdc++')))
        require(('nn_depth_shadow_create' in info) == enabled)
        require(('nn_depth_sampled_create' in info) == depth_sampled)
        (dest / 'link-audit.txt').write_text(actual + '\n' + info)
        results.append(dict(mode=mode, depth=enabled, depth_sampled=depth_sampled, sampled_images=sampled,
                            original_sources=36, compiled_sources=len(compiled),
                            elf=str(elf), sha256=sha(elf), android_execution=False))
        deps, _ = run(['ninja', '-C', dest, '-t', 'deps'])
        for line in deps.splitlines():
            if line.startswith('    '):
                p = Path(line.strip())
                p = p if p.is_absolute() else dest / p
                if p.is_file():
                    pin(p)
    for tool in ('cmake', 'ninja'):
        pin(Path(shutil.which(tool)).resolve())
    for rel in ('source.properties', 'build/cmake/android.toolchain.cmake',
                'toolchains/llvm/prebuilt/linux-x86_64/bin/clang',
                'toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld',
                'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'):
        pin(ndk / rel)
    for path, value in inputs.items():
        if sha(path) != value:
            raise ValueError('depth feature input changed: ' + path)
    result = dict(schema=1, kind='original-host-native-depth-features-host-v1', passed=True,
                  assertions=assertions, rejections=rejections, manifest=manifest,
                  results=results, inputs=inputs,
                  artifacts={str(p): sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.json', '.txt', '.a', '.o', '.so')},
                  GPU_execution=False, original_module_execution=False)
    (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
    return out / 'check.json'


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for field in ('root', 'ndk', 'depth-fdl-check', 'depth-a2d-check', 'depth-runtime-check', 'graphics-check', 'fdl-check', 'block-check', 'names', 'out'):
        p.add_argument('--' + field, type=Path, required=True)
    p.add_argument('--policy-only', action='store_true', help='Only policy and command forwarding; no build/source or GPU qualification')
    print(check(p.parse_args()))
