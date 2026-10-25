#!/usr/bin/env python3
"""Build/verify an original ARM64 shared Activity host with the independent native NVN bridge.

Only builds private artifacts; never deploys or executes the host/game. Original
ExeFS modules pass through the repository's canonical rewrite-native.py pipeline,
which performs read-only git queries for pinned upstream and checkout provenance.
The shared target uses HOST_ABI2 (elfdesc), original native rules, and original HLE/CPU gates.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
from native_contract import native_contract
from bc_sampled_feature import prepare as prepare_bc_sampled, audit as audit_bc_sampled
from pipeline_views_feature import prepare as prepare_pipeline_views, audit as audit_pipeline_views
from native_color_features import prepare as prepare_native_colors, audit as audit_native_colors
from native_sampled_features import prepare as prepare_native_sampled, audit as audit_native_sampled, validate_options as validate_sampled_options
from native_depth_features import prepare as prepare_native_depth, audit as audit_native_depth, validate_options as validate_depth_options
from compile_block_copy import verify as verify_block_copy
from compile_volume_copy import verify as verify_volume_copy
from offline_compute_cache import build as build_compute_cache
from texture_bridge_bundle import build as build_texture_bridge
from graphics_pipeline_cache import build as build_graphics_pipeline_cache
from graphics_storage_feature import prepare as prepare_graphics_storage, audit as audit_graphics_storage, validate_options as validate_graphics_storage
from shader_source_feature import prepare as prepare_shader_sources, audit as audit_shader_sources, validate_options as validate_shader_sources


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def hashes(paths):
    return {str(p): sha(p) for p in sorted(set(paths))}


def check_hashes(values, label):
    for name, expected in values.items():
        p = Path(name)
        if not p.is_file() or sha(p) != expected:
            raise ValueError(f'{label} integrity mismatch: {p}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exefs', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--python', type=Path, default=Path('${LOCAL_VALIDATION_ROOT}/.local/venv/bin/python'))
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--compute-cache-check', type=Path, help='Explicit verified finite offline compute cache; default disabled')
    parser.add_argument('--texture-bridge-check', type=Path, help='Verified normal native canonical texture conversion bundle; default disabled')
    parser.add_argument('--block-copy-check', type=Path, help='Verified normal raw-block compute kernel; default disabled')
    parser.add_argument('--volume-copy-check', type=Path, help='Verified normal separate true3D raw-block kernel; default disabled')
    parser.add_argument('--graphics-pipeline-check', type=Path, help='Verified shader catalogue; default disabled, typed Draw requires supported resources and native views')
    parser.add_argument('--shader-source-check', type=Path, help='Explicit normal complete original source catalogue; identity only, separate native Draw catalogue required; default OFF')
    parser.add_argument('--pipeline-target-fdl-check',type=Path,help='Explicit verified runtime FDL RGBA8 target metadata cache; requires graphics source; default OFF')
    parser.add_argument('--bc-sampled-fdl-check',type=Path,help='Explicit verified FDL archive for production BC1/2/3 UNORM sampling; requires graphics, bridge and block kernel; default OFF')
    parser.add_argument('--clear-color-kernel-check', type=Path, help='Explicit verified native clear kernel; requires constant check; default OFF')
    parser.add_argument('--clear-color-constant-check', type=Path, help='Explicit verified one-texel constant archive; requires kernel check; default OFF')
    parser.add_argument('--color-targets', action='store_true', help='Explicit typed canonical color targets; requires graphics/runtime FDL/block kernel; default OFF')
    parser.add_argument('--sampled-images', action='store_true', help='Explicit actual FDL FLOAT2D sampled owners; requires graphics/runtime FDL/block kernel, default OFF')
    parser.add_argument('--sampled-mip-filter', action='store_true', help='Explicit richer normalized2D sampler profile; requires sampled images, default OFF')
    parser.add_argument('--gather', action='store_true', help='Explicit typed TG4 consumer; requires graphics/native views, public depth Draw remains separate, default OFF')
    parser.add_argument('--graphics-storage', action='store_true', help='Explicit typed VS/FS storage; requires published storage recipes and runtime FDL, default OFF')
    parser.add_argument('--depth-targets', action='store_true', help='Explicit canonical depth load/Draw/store; requires color targets and normal depth FDL/A2D/runtime checks, default OFF')
    parser.add_argument('--depth-sampled', action='store_true', help='Typed noncompare depth sampling; requires depth targets and sampled images, default OFF')
    parser.add_argument('--depth-fdl-check', type=Path, help='Verified normal actual depth FDL metadata archive')
    parser.add_argument('--depth-a2d-check', type=Path, help='Verified normal depth bidirectional A2D packet archive')
    parser.add_argument('--depth-runtime-check', type=Path, help='Verified normal native depth helper object')
    parser.add_argument('--bntx-packaged-layout-experiment',action='store_true',help='Explicit BNTX-writer8B experimental source profile, NOT complete NVN packaged semantics; defaultOFF')
    parser.add_argument('--verify', action='store_true', help='Verify cached host/modules/names without rebuilding')
    args = parser.parse_args()
    if args.bc_sampled_fdl_check and not all((args.graphics_pipeline_check,args.texture_bridge_check,args.block_copy_check)):
        parser.error('--bc-sampled-fdl-check requires --graphics-pipeline-check, --texture-bridge-check and --block-copy-check')
    if args.pipeline_target_fdl_check and not args.graphics_pipeline_check:
        parser.error('--pipeline-target-fdl-check requires --graphics-pipeline-check')
    if bool(args.clear_color_kernel_check) != bool(args.clear_color_constant_check):
        parser.error('native clear requires both --clear-color-kernel-check and --clear-color-constant-check')
    if args.color_targets and not all((args.graphics_pipeline_check,args.pipeline_target_fdl_check,args.block_copy_check)):
        parser.error('--color-targets requires graphics, runtime FDL and block-copy checks')
    try:
        validate_graphics_storage(args.graphics_storage,args.graphics_pipeline_check,args.pipeline_target_fdl_check)
        validate_shader_sources(args.shader_source_check,args.graphics_pipeline_check)
        validate_sampled_options(args.sampled_images,args.sampled_mip_filter,args.gather,args.graphics_pipeline_check,args.pipeline_target_fdl_check,args.block_copy_check,args.texture_bridge_check)
        validate_depth_options(args.depth_targets,args.depth_fdl_check,args.depth_a2d_check,args.depth_runtime_check,args.graphics_pipeline_check,args.pipeline_target_fdl_check,args.block_copy_check,args.color_targets,args.depth_sampled)
        if args.depth_sampled and not args.sampled_images:
            raise ValueError('--depth-sampled requires --sampled-images')
    except ValueError as exc:
        parser.error(str(exc))
    root = Path(__file__).resolve().parents[2]
    activity = root / "nvn-native/adapters/d2r/activity"
    native = root / 'nvn-native'
    exefs, ndk, out = args.exefs.resolve(), args.ndk.resolve(), args.out.resolve()
    if not out.is_relative_to(root / '.local') or exefs.is_relative_to(out) or out.is_relative_to(exefs):
        parser.error('output must be a separate ignored directory under this worktree .local')
    if args.jobs < 1 or args.jobs > 32:
        parser.error('--jobs must be 1..32')
    build, names, modules = (out / part for part in ('cmake', 'names', 'modules'))
    record_path = out / 'build-record.json'
    commands = []
    builder_sha = sha(Path(__file__))

    def run(argv, label, quiet=False):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=root, text=True, capture_output=True)
        commands.append({'label': label, 'argv': argv, 'returncode': result.returncode,
                         'stdout': result.stdout, 'stderr': result.stderr})
        if not quiet:
            print(result.stdout, end='')
        if result.returncode:
            print(result.stderr, end='')
            raise ValueError(f'{label} failed ({result.returncode})')
        return result.stdout

    try:
        python = args.python.absolute()  # Keep the venv, not its interpreter symlink.
        llvm = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin'
        toolchain = ndk / 'build/cmake/android.toolchain.cmake'
        tools = [python.resolve(), ndk / 'source.properties', toolchain,
                 llvm / 'clang', llvm / 'ld.lld', llvm / 'llvm-ar', llvm / 'llvm-readelf']
        tools += [ndk/'toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/29'/name for name in ('crtbegin_so.o','crtend_so.o','libc.so','libdl.so','libm.so','libandroid.so')]
        for tool in ('cmake', 'ninja', 'git'):
            found = shutil.which(tool)
            if not found:
                raise ValueError(f'missing tool {tool}')
            tools.append(Path(found).resolve())
        tool_hashes = hashes(tools)
        inputs = {}
        for name in ('main', 'sdk', 'subsdk0'):
            p = exefs / name
            with p.open('rb') as stream:
                header = stream.read(0x100)
            if len(header) != 0x100 or header[:4] != b'NSO0':
                raise ValueError(f'{p}: original NSO0 required, never a rewritten image')
            inputs[name] = {'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha(p),
                            'build_id': header[0x40:0x60].hex(),
                            'declared_segment_sha256': [header[0xa0+i*32:0xc0+i*32].hex() for i in range(3)]}
        options = {'exefs': str(exefs), 'ndk': str(ndk), 'python': str(python),
                   'abi': 'arm64-v8a', 'android_api': 29, 'host_abi': 2, 'rewrite_abi': 'elfdesc',
                   'rules': 'native', 'private_gates': '1', 'jobs': args.jobs,
                   'target': 'dmhost_activity_kgsl', 'library_kind': 'shared',
                   'controller_included': False, 'build_type': 'RelWithDebInfo',
                   'deploy': False, 'execute': False, 'tlsdesc': True, 'thinlto': False,
                   'probe_modes': False, 'old_renderer': False,
                   'NVN_AHB_PRESENT': 'OFF', 'NVN_AHB_GRAPHICS_HEADER': '',
                   'compute_cache_check': str(args.compute_cache_check.resolve()) if args.compute_cache_check else None,
                   'bc_sampled_fdl_check':str(args.bc_sampled_fdl_check.resolve())if args.bc_sampled_fdl_check else None,
                   'pipeline_target_fdl_check':str(args.pipeline_target_fdl_check.resolve())if args.pipeline_target_fdl_check else None,
                   'clear_color_kernel_check':str(args.clear_color_kernel_check.resolve())if args.clear_color_kernel_check else None,
                   'clear_color_constant_check':str(args.clear_color_constant_check.resolve())if args.clear_color_constant_check else None,
                   'color_targets':args.color_targets,
                   'sampled_images':args.sampled_images,'sampled_mip_filter':args.sampled_mip_filter,'gather':args.gather,
                   'depth_targets':args.depth_targets,
                   'depth_sampled':args.depth_sampled,
                   'graphics_storage':args.graphics_storage,
                   'depth_fdl_check':str(args.depth_fdl_check.resolve())if args.depth_fdl_check else None,
                   'depth_a2d_check':str(args.depth_a2d_check.resolve())if args.depth_a2d_check else None,
                   'depth_runtime_check':str(args.depth_runtime_check.resolve())if args.depth_runtime_check else None,
                   'bntx_packaged_layout_experiment':args.bntx_packaged_layout_experiment,
                   'texture_bridge_check': str(args.texture_bridge_check.resolve()) if args.texture_bridge_check else None,
                   'block_copy_check': str(args.block_copy_check.resolve()) if args.block_copy_check else None,
                   'volume_copy_check': str(args.volume_copy_check.resolve()) if args.volume_copy_check else None,
                   'shader_source_check':str(args.shader_source_check.resolve()) if args.shader_source_check else None,
                   'graphics_pipeline_check': str(args.graphics_pipeline_check.resolve()) if args.graphics_pipeline_check else None}
        prior = json.loads(record_path.read_text()) if record_path.is_file() else None
        if prior:
            check_hashes(prior['artifacts'], 'cached host/module artifact')
        cache_header, cache_manifest, cache_outputs = None, None, {}
        if args.compute_cache_check:
            cache_header, cache_manifest, cache_outputs = build_compute_cache(
                args.compute_cache_check, out / 'offline-compute-cache', root=root, verify=args.verify)
        pipeline_header, pipeline_manifest, pipeline_inputs = None, None, {}
        if args.graphics_pipeline_check:
            pipeline_header, pipeline_manifest = build_graphics_pipeline_cache(root,args.graphics_pipeline_check)
            pipeline_inputs = pipeline_manifest['inputs']
        if args.verify and prior is not None and prior.get('offline_graphics_pipeline') != pipeline_manifest:
            raise ValueError('source-only graphics pipeline record changed')
        bridge_header, bridge_manifest, bridge_inputs = None, None, {}
        if args.texture_bridge_check:
            bridge_check = args.texture_bridge_check.resolve()
            bridge_header, bridge_manifest = build_texture_bridge(root, bridge_check)
            bridge_producer = native / 'tools/texture_bridge_bundle.py'
            if bridge_manifest['inputs'].get(str(bridge_producer)) != sha(bridge_producer):
                raise ValueError('texture bridge requires the current normal producer')
            bridge_inputs = {str(bridge_check): sha(bridge_check),
                             **bridge_manifest['inputs'], **bridge_manifest['artifacts']}
        block_header,block_manifest,block_inputs=None,None,{}
        if args.block_copy_check:
            block_check=args.block_copy_check.resolve()
            block_manifest=verify_block_copy(root,block_check)
            block_header=Path(block_manifest['header'])
            block_inputs={str(block_check):sha(block_check),**block_manifest['inputs'],**block_manifest['artifacts']}
        if args.verify and prior is not None and prior.get('native_block_copy')!=block_manifest:
            raise ValueError('native raw-block kernel record changed')
        volume_header,volume_manifest,volume_inputs=None,None,{}
        if args.volume_copy_check:
            volume_check=args.volume_copy_check.resolve()
            volume_manifest=verify_volume_copy(root,volume_check)
            volume_header=Path(volume_manifest['header'])
            volume_inputs={str(volume_check):sha(volume_check),**volume_manifest['inputs'],**volume_manifest['artifacts']}
        if args.verify and prior is not None and prior.get('native_volume_copy')!=volume_manifest:
            raise ValueError('native volume kernel record changed')
        bc_library,bc_manifest,bc_inputs=prepare_bc_sampled(root,args.bc_sampled_fdl_check,args.graphics_pipeline_check,args.texture_bridge_check,args.block_copy_check)
        target_library,target_manifest,target_inputs=prepare_pipeline_views(root,args.pipeline_target_fdl_check,args.graphics_pipeline_check,bc_library)
        sampled_flags,sampled_manifest,sampled_inputs=prepare_native_sampled(root,args.sampled_images,args.sampled_mip_filter,args.gather,args.graphics_pipeline_check,args.pipeline_target_fdl_check,args.block_copy_check,args.texture_bridge_check,target_library,bc_library,args.color_targets)
        if args.verify and prior is not None and prior.get('native_sampled_features')!=sampled_manifest:
            raise ValueError('native sampled feature inputs/options changed')
        depth_flags,depth_manifest,depth_inputs=prepare_native_depth(root,args.depth_targets,args.depth_fdl_check,args.depth_a2d_check,args.depth_runtime_check,args.graphics_pipeline_check,args.pipeline_target_fdl_check,args.block_copy_check,target_library,args.color_targets,args.depth_sampled)
        if args.verify and prior is not None and prior.get('native_depth_features')!=depth_manifest:
            raise ValueError('native depth feature inputs/options changed')
        storage_flags,storage_manifest,storage_inputs=prepare_graphics_storage(root,args.graphics_storage,args.graphics_pipeline_check,args.pipeline_target_fdl_check,target_library)
        if args.verify and prior is not None and prior.get('native_graphics_storage')!=storage_manifest:
            raise ValueError('native graphics storage feature inputs/options changed')
        source_flags,source_manifest,source_inputs=prepare_shader_sources(root,args.shader_source_check,args.graphics_pipeline_check)
        if args.verify and prior is not None and prior.get('complete_shader_sources')!=source_manifest:
            raise ValueError('complete source catalogue inputs/options changed')
        color_flags,color_manifest,color_inputs=prepare_native_colors(root,args.clear_color_kernel_check,args.clear_color_constant_check,args.color_targets,target_library,args.graphics_pipeline_check,args.block_copy_check)
        if args.verify and prior is not None and prior.get('native_color_features')!=color_manifest:
            raise ValueError('native color feature inputs/options changed')
        if args.verify and prior is not None and prior.get('native_pipeline_target_views')!=target_manifest:
            raise ValueError('runtime FDL target metadata feature changed')
        if args.verify and prior is not None and prior.get('native_bc_sampled')!=bc_manifest:
            raise ValueError('production BC sampled feature record changed')
        if args.verify and prior is not None and prior.get('native_texture_bridge') != bridge_manifest:
            raise ValueError('native texture bridge record changed')
        if args.verify and prior is not None and prior.get('offline_compute_cache') != cache_manifest:
            raise ValueError('offline compute cache record changed')
        if args.verify and prior is None:
            raise ValueError('missing host build-record.json')
        if args.verify and (prior['tools'] != tool_hashes or prior['inputs'] != inputs or prior['options'] != options):
            raise ValueError('input/tool/options changed; build a fresh verified host')
        if args.verify:
            check_hashes(prior['sources'], 'cached source')
        else:
            out.mkdir(parents=True, exist_ok=True)
        rewrite_command = [python, root / 'scripts/dev/rewrite-native.py', '--source', exefs,
                           '--output', modules, '--abi', 'elfdesc', '--rules', 'native', '--private-gates', '1']
        if not args.verify:
            run(rewrite_command, 'original NSO canonical generation')
        run(rewrite_command + ['--verify-only'], 'original NSO canonical verification')
        extract = [python, native / 'adapters/d2r/extract_nvn_names.py', '--nso', exefs / 'main', '--out', names]
        if not args.verify:
            run(extract, 'original NVN names generation')
        run(extract + ['--verify'], 'original NVN names verification')
        if not args.verify:
            run(['cmake', '-S', activity, '-B', build, '-G', 'Ninja',
                 f'-DCMAKE_TOOLCHAIN_FILE={toolchain}', '-DANDROID_ABI=arm64-v8a',
                 '-DANDROID_PLATFORM=android-29', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
                 '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DNVN_AHB_PRESENT=OFF',
                 '-DNVN_AHB_GRAPHICS_HEADER=', f'-DNVN_D2R_NAMES_HEADER={names / "names.inc.h"}',
                 f'-DNVN_D2R_COMPUTE_CACHE_HEADER={cache_header or ""}',
                 f'-DNVN_D2R_TEXTURE_BRIDGE_HEADER={bridge_header or ""}',
                 f'-DNVN_D2R_BLOCK_COPY_HEADER={block_header or ""}',
                 f'-DNVN_D2R_VOLUME_COPY_HEADER={volume_header or ""}',
                 f'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER={pipeline_header or ""}',
                 f'-DNVN_D2R_BC_SAMPLED_FDL_LIBRARY={bc_library or ""}',
                 f'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY={target_library or ""}',
                 *color_flags,
                 *sampled_flags,
                 *depth_flags,
                 *source_flags,
                 *storage_flags,
                 f'-DNVN_BNTX_PACKAGED_LAYOUT_EXPERIMENT={"ON" if args.bntx_packaged_layout_experiment else "OFF"}'],
                'configure original native host')
        host_sources = [Path(p) for p in (build / 'host-sources.txt').read_text().splitlines()]
        if len(host_sources) != 36 or len(set(host_sources)) != 36:
            raise ValueError('expected the 36 original host/HLE/platform sources')
        # Include the complete selected local header closure before compiling;
        # Ninja dependency output below must be a subset of this inventory.
        sources = host_sources + list(activity.glob('*.c')) + list(activity.glob('*.h')) + [
            activity / 'CMakeLists.txt', activity / 'exports.map', activity / 'README.md',
            Path(__file__), native / 'tools/native_contract.py', native / 'CMakeLists.txt',
            native / 'adapters/d2r/host_bridge.c', native / 'adapters/d2r/host_bridge.h',
            native / 'adapters/d2r/extract_nvn_names.py', root / 'dm/tools/nso.py',
            root / 'dm/gfx/tools/nvn_slots.py', root / 'dm/tools/patch_nso.py',
            root / 'scripts/dev/rewrite-native.py', root / 'scripts/dev/upstream-source.json']
        window_platform_sources = [native / 'android' / (name + '.c') for name in ('window_session', 'window_android', 'present', 'present_pacer', 'window_display', 'ahb')]
        sources += window_platform_sources + sorted((native / 'android').glob('*.h'))
        for folder in (root / 'dm/host', root / 'dm/host/android'):
            sources += [p for p in folder.iterdir() if p.is_file() and p.suffix in ('.h', '.inc')]
        for folder in (native / 'src', native / 'include', native / 'third_party/nvn', native / 'third_party/kgsl',
                       root / 'dm/third_party/runtime-codecs', root / 'dm/third_party/libdeflate'):
            sources += [p for p in folder.rglob('*') if p.is_file() and p.suffix in ('.h', '.inc', '.c', '.S')]
        for name in inputs:
            boot = root / 'dm/data' / ('bootlist_' + inputs[name]['build_id'][:32] + '.tsv')
            if boot.is_file():
                sources.append(boot)
        sources.append(native / 'tools/native_depth_features.py')
        sources += [Path(name)for name in depth_inputs]
        sources.append(native/'tools/graphics_storage_feature.py')
        sources += [Path(name)for name in storage_inputs]
        sources.append(native/'tools/shader_source_feature.py')
        sources += [Path(name)for name in source_inputs]
        sources.append(native / 'tools/native_sampled_features.py')
        sources += [Path(name)for name in sampled_inputs]
        sources.append(native / 'tools/native_color_features.py')
        sources += [Path(name)for name in color_inputs]
        sources.append(native / 'tools/pipeline_views_feature.py')
        sources += [Path(name)for name in target_inputs]
        sources.append(native / 'tools/bc_sampled_feature.py')
        sources += [Path(name)for name in bc_inputs]
        sources.append(native / 'tools/offline_compute_cache.py')
        sources.append(native / 'tools/texture_bridge_bundle.py')
        sources.append(native / 'tools/compile_block_copy.py')
        sources.append(native / 'tools/compile_volume_copy.py')
        sources.append(native / 'tools/compute_driverparams.py')
        sources += [Path(name) for name in block_inputs]
        sources += [Path(name) for name in volume_inputs]
        sources += [native/'tools'/name for name in ('graphics_pipeline_cache.py','package_graphics_pipelines.py','package_graphics_stages.py')]
        sources += [Path(name) for name in pipeline_inputs]
        sources += [Path(name) for name in bridge_inputs]
        if cache_manifest: sources += [Path(name) for name in cache_manifest['inputs']]
        source_hashes = hashes(sources)
        if args.verify and prior['sources'] != source_hashes:
            raise ValueError('source inventory changed')
        if not args.verify:
            run(['cmake', '--build', build, '--target', 'dmhost_activity_kgsl', '--parallel', args.jobs], 'build original native host')
        actual_commands = run(['ninja', '-C', build, '-t', 'commands', 'dmhost_activity_kgsl'], 'selected link closure', quiet=True)
        forbidden = re.compile(r'/gfx/(vulkan|metal)/|/hle_nvn\.c\b|/graphics_boundary\.c\b|/android/jni\.c\b|\b(?:unicorn|qemu|ryujinx|volk)\b|-l(?:vulkan|Metal)\b', re.I)
        if forbidden.search(actual_commands):
            raise ValueError('forbidden renderer/CPU backend in actual host build commands')
        compiled_sources = []
        for line in actual_commands.splitlines():
            parts = shlex.split(line)
            if '-c' in parts:
                compiled_sources.append(Path(parts[parts.index('-c') + 1]).resolve())
        expected = set(host_sources + [activity/'activity_host.c'] + window_platform_sources + [native / 'adapters/d2r/host_bridge.c'] +
                       [native / 'src' / (name + '.c') for name in ('kgsl_probe', 'kgsl', 'nvn', 'compute', 'graphics', 'external_image', 'order', 'render_state', 'sampler_state', 'texture_source', 'texture_bridge', 'block_copy', 'counter', 'offline_graphics', 'pipeline_program', 'pipeline_shader', 'pipeline_vertex', 'pipeline', 'pipeline_archive', 'pipeline_source', 'pipeline_frontend', 'pipeline_resource_queries')])
        if bc_manifest or args.sampled_images:expected.add(native/'src/texture_bc_shadow.c')
        if args.sampled_images:expected.add(native/'src/sampled_image.c')
        if target_manifest:expected.add(native/'src/pipeline_views.c')
        audit_bc_sampled(native,bc_manifest,actual_commands,sampled_manifest)
        audit_native_sampled(native,sampled_manifest,actual_commands,bc_manifest)
        audit_native_depth(native,depth_manifest,actual_commands)
        audit_shader_sources(native,source_manifest,actual_commands)
        audit_graphics_storage(native,storage_manifest,actual_commands)
        if storage_manifest:expected.add(native/'src/pipeline_storage.c')
        if source_manifest:expected.update((native/'src/shader_source.c',Path(source_manifest['source']),Path(source_manifest['assembly'])))
        if args.depth_targets:expected.update(native/'src'/name for name in ('depth_attachment.c','depth_transfer.c','depth_shadow.c','pipeline_depth_archive.c'))
        if args.depth_sampled:expected.update(native/'src'/name for name in ('depth_sampled.c','depth_sampled_native.c'))
        audit_pipeline_views(native,target_manifest,actual_commands)
        audit_native_colors(native,color_manifest,actual_commands)
        if color_manifest and color_manifest['clear']:expected.add(native/'src/clear_color.c')
        if args.color_targets:expected.update(native/'src'/name for name in ('color_target.c','color_target_shadow.c'))
        if args.bntx_packaged_layout_experiment:expected.add(native/'src/packaged_bntx.c')
        if ('-DNN_NVN_BNTX_PACKAGED_EXPERIMENT=1' in actual_commands) != args.bntx_packaged_layout_experiment:
            raise ValueError('actual BNTX experiment macro differs from explicit option')
        if set(compiled_sources) != expected or len(compiled_sources) != len(expected):
            missing=sorted(map(str,expected-set(compiled_sources)))
            extra=sorted(map(str,set(compiled_sources)-expected))
            duplicates=len(compiled_sources)-len(set(compiled_sources))
            raise ValueError('actual compilation closure differs from original host + bridge + independent driver; '
                             f'missing={missing}; extra={extra}; duplicates={duplicates}')
        if '-DDM_TLS_DESC=1' not in actual_commands or '-DDM_NATIVE_FEATURES' in actual_commands:
            raise ValueError('activity requires native TLSDESC and excludes unreviewed full features')
        deps = run(['ninja', '-C', build, '-t', 'deps'], 'selected local dependencies', quiet=True)
        all_local_deps = set()
        local_deps = set()
        for line in deps.splitlines():
            if line.startswith('    '):
                p = Path(line.strip())
                if not p.is_absolute():
                    p = build / p
                p = p.resolve()
                if p.is_relative_to(root): all_local_deps.add(p)
                if p.is_relative_to(root) and not p.is_relative_to(out):
                    local_deps.add(p)
        if not local_deps <= set(sources):
            raise ValueError('unrecorded local compiler dependencies: ' + str(sorted(local_deps-set(sources))))
        if cache_header:
            headers={Path(name) for name in cache_outputs if name.endswith('.h')}
            if not headers <= all_local_deps or 'NN_NVN_OFFLINE_COMPUTE_HEADER' not in actual_commands:
                raise ValueError('offline compute cache missing from actual bridge compilation dependencies')
        if bridge_header and pipeline_header:
            view_header=bridge_header.parent/'texture_native_views.h'
            if bridge_inputs.get(str(view_header)) != sha(view_header) or view_header not in all_local_deps or 'NN_NVN_PIPELINE_NATIVE_VIEWS_HEADER' not in actual_commands:
                raise ValueError('finite graphics native views missing from normal artifact/dependency closure')
        if bridge_header:
            bridge_headers = {bridge_header, bridge_header.parent / 'gob_copy.native.h'}
            if not bridge_headers <= all_local_deps or 'NN_NVN_TEXTURE_BRIDGE_HEADER' not in actual_commands:
                raise ValueError('native texture bridge missing from actual bridge compilation dependencies')
        if block_header:
            if block_header not in all_local_deps or 'NN_NVN_BLOCK_COPY_HEADER' not in actual_commands or 'NN_NVN_BLOCK_COPY=1' not in actual_commands:
                raise ValueError('raw-block kernel missing from normal host dependency closure')
        if pipeline_header:
            if pipeline_header not in all_local_deps or 'NN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER' not in actual_commands or 'NN_NVN_GRAPHICS_SOURCE_CACHE=1' not in actual_commands:
                raise ValueError('source-only graphics catalogue missing from actual bridge/driver compilation dependencies')
            check_hashes(pipeline_inputs, 'source-only graphics final dependency')
        if source_manifest:
            if Path(source_manifest['header']) not in all_local_deps:
                raise ValueError('complete source header absent from actual host dependencies')
            check_hashes(source_inputs,'complete original source final dependency')
        if bridge_inputs:
            check_hashes(bridge_inputs, 'native texture bridge final dependency')
        exe = build / 'libdmhost_activity_kgsl.so'
        elf = run([llvm / 'llvm-readelf', '--file-header', '--program-headers', '--dynamic', '--sections', '--wide', exe],
                  'ARM64 host ELF/dependency audit', quiet=True)
        needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', elf)
        if ('AArch64' not in elf or not re.search(r'Type:\s+DYN\b', elf) or
                any(lib not in ('libm.so', 'libdl.so', 'libc.so', 'libandroid.so') for lib in needed) or
                not re.search(r'\bswforce\s+PROGBITS\b', elf)):
            raise ValueError('activity must be AArch64 shared ELF with original swforce and only declared Android system dependencies')
        loads = [line.split()[-1] for line in elf.splitlines() if line.strip().startswith('LOAD ')]
        if not loads or any(int(alignment, 16) < 16384 for alignment in loads):
            raise ValueError('host LOAD alignment must be at least16KiB')
        symbols = run([llvm / 'llvm-readelf', '--symbols', '--wide', exe], 'original HLE/bootstrap symbols', quiet=True)
        for symbol in ('dm_android_main', 'dm_tls_slots', 'dm_tsd_descriptor', 'dm_vi_native_window_token', 'load_modules', 'guest_thread_init', 'dm_gate', 'nn_d2r_nvn_bootstrap', 'nn_d2r_nvn_get_proc_address', 'nn_d2r_activity_host_attach', 'nn_d2r_activity_host_run'):
            if not re.search(r'\b' + symbol + r'$', symbols, re.M):
                raise ValueError(f'missing original host/bridge symbol {symbol}')
        if re.search(r'\b__emutls_(?:get_address|[vt]\.)', symbols):
            raise ValueError('activity must use native ELF TLSDESC, not compiler-emulated TLS')
        relocations = run([llvm/'llvm-readelf','--relocations','--wide',exe], 'native TLS relocation audit', quiet=True)
        exports = run([llvm/'llvm-readelf','--dyn-symbols','--wide',exe], 'shared entry exports', quiet=True)
        if 'R_AARCH64_TLSDESC' not in relocations or re.search(r'^\s*INTERP\s',elf,re.M):
            raise ValueError('shared host needs native TLSDESC relocations and no interpreter segment')
        for name in ('token','attach','close','run','check_bootstrap','notify_focus'):
            if not re.search(r'\bnn_d2r_activity_host_'+name+r'@@D2R_NATIVE_ACTIVITY_1$',exports,re.M):
                raise ValueError('missing versioned shared entry '+name)
        declared_exports={'nn_d2r_activity_host_'+name for name in
                          ('token','attach','close','run','check_bootstrap','notify_focus')}
        actual_exports=set(re.findall(r'\b(nn_d2r_activity_host_\w+)(?:@@?\S+)?$',exports,re.M))
        if actual_exports!=declared_exports:
            raise ValueError('shared Activity public entry set differs from the six typed APIs')
        rewrite = json.loads((modules / 'rewrite-manifest.json').read_text())
        for name in inputs:
            if rewrite['modules'][name]['recipe']['input']['sha256'] != inputs[name]['sha256']:
                raise ValueError('module provenance does not match original NSO input')
            if '# HOST_ABI\t0x2' not in (modules / (name + '.fixups.tsv')).read_text():
                raise ValueError('module fixups do not declare HOST_ABI2')
        if (source_hashes != hashes(sources) or tool_hashes != hashes(tools) or
                sha(Path(__file__)) != builder_sha):
            raise ValueError('source/tool changed during build; no deployable record produced')
        if args.verify:
            check_hashes(prior['artifacts'], 'verified final artifact')
            print(f'verified {record_path}; no host/game execution')
            return
        audit = {'schema': 1, 'compiled_sources': list(map(str, compiled_sources)), 'original_host_sources': list(map(str, host_sources)),
                 'local_compiler_dependencies': list(map(str, sorted(local_deps))), 'actual_commands': actual_commands,
                 'elf': elf, 'tls_relocations': relocations, 'dynamic_exports': exports, 'needed': needed, 'load_alignments': loads, 'host_abi': 2,
                 'old_renderer_or_cpu_emulator': False, 'deployed': False, 'executed': False}
        (out / 'link-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
        artifact_paths = [exe, build / 'dmhost_activity_kgsl.map', build / 'compile_commands.json',
                          build / 'CMakeCache.txt', build / 'host-sources.txt', out / 'link-audit.json',
                          build / 'driver/libnvn_kgsl.a']
        artifact_paths += list(build.rglob('*.o'))
        artifact_paths += [Path(name) for name in cache_outputs]
        artifact_paths += [p for p in modules.iterdir() if p.is_file()]
        artifact_paths += [p for p in names.iterdir() if p.is_file()]
        record = {'schema': 1, 'scope': 'Original ARM64 shared Activity host + exact VI token bridge + typed native KGSL bridge; build/verify only. No deployment, GPU execution, SDK initialization, constructors or real game module execution.',
                  'inputs': inputs, 'upstream': rewrite['upstream'], 'rewrite_manifest': str(modules / 'rewrite-manifest.json'),
                  'options': options, 'sources': source_hashes, 'tools': tool_hashes, 'commands': commands,
                  'artifacts': hashes(artifact_paths), 'shared_library': str(exe), 'modules': str(modules),
                  'link_audit': str(out / 'link-audit.json'), 'needed': needed,
                  'entry_modes': ['--sdk-init', '--constructors', '--run'],
                  'entry_stage_values': {'sdk-init': 1, 'constructors': 2, 'run': 3},
                  'public_entrypoints': ['nn_d2r_activity_host_' + name for name in
                                        ('token', 'attach', 'close', 'run', 'check_bootstrap', 'notify_focus')],
                  'offline_compute_cache': cache_manifest,
                  'native_texture_bridge': bridge_manifest,
                  'native_bc_sampled':bc_manifest,
                  'native_pipeline_target_views':target_manifest,
                  'native_color_features':color_manifest,
                  'native_sampled_features':sampled_manifest,
                  'native_depth_features':depth_manifest,
                  'native_block_copy': block_manifest,
                  'native_volume_copy': volume_manifest,
                  'offline_graphics_pipeline': pipeline_manifest,
                  'complete_shader_sources':source_manifest,
                  'native_graphics_storage':storage_manifest,
                  'nvn_contract': native_contract(native),
                  'known_blocker': 'Partial native NVN subset. Unsupported properties/flags/services and unknown ABI calls stop explicitly; no success stubs added.'}
        record_path.write_text(json.dumps(record, indent=2) + '\n')
        print(record_path)
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
