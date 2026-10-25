"""Explicit original-host depth consumer inputs and actual source/link audit.

Normal helper records establish compilation provenance. They do not establish
public NVN, GPU execution, or original-game compatibility.
"""
import json
import shlex
from pathlib import Path
from native_sampled_features import sha


SOURCES = ('depth_attachment.c', 'depth_transfer.c', 'depth_shadow.c',
           'pipeline_depth_archive.c')
MACROS = ('NN_NVN_DEPTH_TARGETS', 'NN_PIPELINE_DEPTH_NATIVE')
SAMPLED_SOURCES = ('depth_sampled.c', 'depth_sampled_native.c')
SAMPLED_MACROS = ('NN_NVN_DEPTH_SAMPLED', 'NN_PIPELINE_DEPTH_SAMPLED')


def validate_options(enabled, foundation, a2d, runtime, graphics, fdl, block, color_targets, sampled_depth=False):
    if type(enabled) is not bool:
        raise ValueError('depth feature switch must be an explicit boolean')
    if type(color_targets) is not bool:
        raise ValueError('color target feature switch must be an explicit boolean')
    if type(sampled_depth) is not bool:
        raise ValueError('depth sampling switch must be an explicit boolean')
    if sampled_depth and not enabled:
        raise ValueError('--depth-sampled requires --depth-targets')
    checks = (foundation, a2d, runtime)
    if enabled and not all((*checks, graphics, fdl, block, color_targets)):
        raise ValueError('--depth-targets requires depth FDL/A2D/runtime, graphics, runtime FDL, block-copy checks and --color-targets')
    if not enabled and any(checks):
        raise ValueError('depth helper checks require --depth-targets')


def checked(root, path, kind, tool, required):
    path = Path(path).resolve()
    c = json.loads(path.read_text())
    if (type(c.get('schema')) is not int or c['schema'] != 1 or
            c.get('kind') != kind or c.get('passed') is not True or
            c.get('gpu_execution') is not False):
        raise ValueError('wrong normal depth helper record: ' + str(path))
    inputs = {str(path): sha(path)}
    for key in ('inputs', 'artifacts'):
        mapping = c.get(key)
        if not isinstance(mapping, dict) or not mapping:
            raise ValueError('missing normal depth ' + key)
        for p, h in mapping.items():
            if not Path(p).is_absolute() or sha(p) != h:
                raise ValueError('depth helper input/product changed: ' + p)
            inputs[p] = h
    for rel in (tool, *required):
        p = root / 'nvn-native' / rel
        if c['inputs'].get(str(p)) != sha(p):
            raise ValueError('depth helper must use current normal source: ' + rel)
    return c, inputs


def prepare(root, enabled, foundation_check, a2d_check, runtime_check,
            graphics_check, fdl_check, block_check, target_library, color_targets, sampled_depth=False):
    validate_options(enabled, foundation_check, a2d_check, runtime_check,
                     graphics_check, fdl_check, block_check, color_targets, sampled_depth)
    flags = ['-DNVN_D2R_DEPTH_TARGETS=' + ('ON' if enabled else 'OFF'),
             '-DNVN_D2R_DEPTH_LIBRARIES=',
             '-DNVN_D2R_DEPTH_SAMPLED=' + ('ON' if sampled_depth else 'OFF')]
    if not enabled:
        return flags, None, {}
    native = root / 'nvn-native'
    from build_runtime_fdl import verify as verify_fdl
    library, fdl = verify_fdl(root, Path(fdl_check).resolve())
    if not target_library or Path(library).resolve() != Path(target_library).resolve():
        raise ValueError('depth and target features require the same verified FDL archive')
    from compile_block_copy import verify as verify_block
    block = verify_block(root, Path(block_check).resolve())
    from graphics_pipeline_cache import build as verify_graphics
    _, graphics = verify_graphics(root, Path(graphics_check).resolve())
    if graphics.get('depth_archive') is not True:
        raise ValueError('depth targets require an explicitly published normal depth archive')
    inputs = {}
    for path, record in ((fdl_check, fdl), (block_check, block)):
        inputs.update({str(Path(path).resolve()): sha(path), **record['inputs'], **record['artifacts']})
    inputs.update(graphics['inputs'])
    foundation, mapping = checked(root, foundation_check, 'native-depth-fdl-foundation-v1',
                                  'tools/check_depth_fdl.py',
                                  ('src/depth_fdl.cc', 'include/nvn_native/depth_fdl.h'))
    inputs.update(mapping)
    a2d, mapping = checked(root, a2d_check, 'native-depth-a2d-host-v1',
                          'tools/check_depth_a2d.py',
                          ('src/depth_a2d.cc', 'include/nvn_native/depth_a2d.h'))
    inputs.update(mapping)
    runtime, mapping = checked(root, runtime_check, 'native-depth-zero-color-host-v1',
                              'tools/check_depth_runtime.py',
                              ('src/depth_native.cc', 'include/nvn_native/depth_native.h',
                               'src/pipeline.c', 'src/pipeline_program.c'))
    inputs.update(mapping)
    libraries = [Path(foundation['static_library']).resolve(),
                 Path(a2d['static_library']).resolve(),
                 Path(runtime_check).resolve().parent / 'depth-native-api29.o']
    for record, product in zip((foundation, a2d, runtime), libraries):
        if record['artifacts'].get(str(product)) != sha(product):
            raise ValueError('depth library/object is not a normal checked artifact: ' + str(product))
    for rel in (*('src/' + s for s in SOURCES),
                'src/nvn_depth_target.inc', 'src/nvn_depth_draw.inc',
                'src/nvn_depth_clear_record.inc', 'src/nvn_depth_clear_prepare.inc',
                'src/nvn_depth_clear_submit.inc',
                'include/nvn_native/depth_shadow.h',
                'include/nvn_native/pipeline_depth_archive.h'):
        p = native / rel
        inputs[str(p)] = sha(p)
    if sampled_depth:
        for rel in (*('src/' + s for s in SAMPLED_SOURCES),
                    'src/nvn_depth_sampled.inc', 'src/sampler_state.c',
                    'include/nvn_native/depth_sampled.h',
                    'include/nvn_native/depth_sampled_native.h'):
            p = native / rel
            inputs[str(p)] = sha(p)
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    flags[1] += ';'.join(map(str, libraries))
    manifest = dict(schema=1, kind='original-host-native-depth-features-v1',
                    depth_targets=True, depth_sampled=sampled_depth,
                    foundation_check=str(Path(foundation_check).resolve()),
                    a2d_check=str(Path(a2d_check).resolve()),
                    runtime_check=str(Path(runtime_check).resolve()),
                    libraries=list(map(str, libraries)),
                    runtime_fdl_library=str(Path(library).resolve()),
                    block_copy_header=block['header'],
                    graphics_header=graphics['header'],
                    source_role='canonical GOB D16/D24X8/D32F, selected 2D/array mip and layer',
                    source_preserved=True, cpu_pixel_conversion=False,
                    default_enabled=False, gpu_qualification=False,
                    original_module_execution=False, inputs=inputs)
    return flags, manifest, inputs


def audit(native, manifest, commands):
    native = Path(native).resolve()
    rows, links = [], []
    for line in commands.splitlines():
        args = shlex.split(line)
        if '-c' in args:
            rows.append((Path(args[args.index('-c') + 1]).resolve(), args))
        else:
            links.append(args)
    enabled = bool(manifest)
    sampled_depth = bool(manifest and manifest.get('depth_sampled'))
    for name in SOURCES:
        matches = [args for p, args in rows if p == native / 'src' / name]
        if len(matches) != int(enabled):
            raise ValueError('depth source missing, duplicated or unrequested: ' + name)
    for name in SAMPLED_SOURCES:
        matches = [args for p, args in rows if p == native / 'src' / name]
        if len(matches) != int(sampled_depth):
            raise ValueError('depth sampled source missing, duplicated or unrequested: ' + name)
    for name in ('nvn.c', 'pipeline.c', 'sampler_state.c', *SOURCES, *SAMPLED_SOURCES):
        selected = [args for p, args in rows if p == native / 'src' / name]
        if not selected:
            continue  # Optional source presence was checked independently above.
        if len(selected) != 1:
            raise ValueError('duplicate depth sampled compilation: ' + name)
        for macro in SAMPLED_MACROS:
            prefix = '-D' + macro
            actual = [x for x in selected[0] if x == prefix or x.startswith(prefix + '=')]
            if actual != ([prefix + '=1'] if sampled_depth else []):
                raise ValueError('depth sampled actual compile mode differs: ' + name + ' ' + macro)
    for name in ('nvn.c', 'pipeline.c', 'pipeline_program.c', 'sampled_image.c', *SOURCES):
        selected = [args for p, args in rows if p == native / 'src' / name]
        if name == 'sampled_image.c' and not selected:
            continue
        if name in SOURCES and not enabled:
            continue
        if len(selected) != 1:
            raise ValueError('depth compile source missing or duplicated: ' + name)
        for macro in MACROS:
            prefix = '-D' + macro
            actual = [x for x in selected[0] if x == prefix or x.startswith(prefix + '=')]
            if actual != ([prefix + '=1'] if enabled else []):
                raise ValueError('depth actual compile mode differs: ' + name + ' ' + macro)
    if enabled:
        core = next(args for p, args in rows if p == native / 'src/nvn.c')
        if '-DNN_NVN_BLOCK_COPY=1' not in core or '-DNN_PIPELINE_RUNTIME_VIEWS=1' not in core:
            raise ValueError('depth requires actual raw-block and runtime-view consumers')
        bridge = [args for p, args in rows if p == native / 'adapters/d2r/host_bridge.c']
        define = '-DNN_NVN_BLOCK_COPY_HEADER="' + manifest['block_copy_header'] + '"'
        if len(bridge) != 1 or define not in bridge[0] or '-DNN_NVN_DEPTH_TARGETS=1' not in bridge[0]:
            raise ValueError('depth requires actual Device archive/kernel registration')
        archive_define = '-DNN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER="' + manifest['graphics_header'] + '"'
        if archive_define not in bridge[0]:
            raise ValueError('actual depth Device archive differs from the normal checked catalogue')
        for library in (*manifest['libraries'], manifest['runtime_fdl_library']):
            if not any(library in line for line in links):
                raise ValueError('normal depth input absent from actual link: ' + library)
        for p, h in manifest['inputs'].items():
            if sha(p) != h:
                raise ValueError('depth source/product changed: ' + p)
    if any('/tests/depth' in str(p) or '/tests/d2r_bridge/' in str(p) for p, _ in rows):
        raise ValueError('depth qualification source entered production host')
