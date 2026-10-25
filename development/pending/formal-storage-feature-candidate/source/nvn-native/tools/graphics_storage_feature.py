"""Explicit original-host VS/FS storage role; independently qualified native KGSL.

A published archive is a compilation input, never proof of public NVN or game
execution. Query/selector validation remains in the independent runtime.
"""
from pathlib import Path
import shlex
from native_sampled_features import sha

SOURCES = ('pipeline_storage.c',)
MACROS = ('NN_NVN_GRAPHICS_STORAGE', 'NN_PIPELINE_GRAPHICS_STORAGE')


def validate_options(enabled, graphics, fdl):
    if type(enabled) is not bool:
        raise ValueError('graphics storage switch must be an explicit boolean')
    if enabled and not (graphics and fdl):
        raise ValueError('--graphics-storage requires --graphics-pipeline-check and --pipeline-target-fdl-check')


def prepare(root, enabled, graphics_check, fdl_check, target_library):
    validate_options(enabled, graphics_check, fdl_check)
    flags = ['-DNVN_D2R_GRAPHICS_STORAGE=' + ('ON' if enabled else 'OFF')]
    if not enabled:
        return flags, None, {}
    root = Path(root).resolve()
    native = Path(__file__).resolve().parents[1]
    from graphics_pipeline_cache import build as verify_graphics
    from build_runtime_fdl import verify as verify_fdl
    header, graphics = verify_graphics(root, Path(graphics_check).resolve())
    if graphics.get('storage_archive') is not True or graphics.get('native_texture_types') is not True:
        raise ValueError('graphics storage requires an explicitly published normal typed storage archive')
    library, fdl = verify_fdl(root, Path(fdl_check).resolve())
    if not target_library or Path(target_library).resolve() != Path(library).resolve():
        raise ValueError('graphics storage requires the same verified runtime FDL archive as targets')
    inputs = dict(graphics['inputs'])
    inputs.update({str(Path(fdl_check).resolve()): sha(fdl_check), **fdl['inputs'], **fdl['artifacts']})
    for rel in ('src/pipeline_storage.c', 'src/nvn_pipeline_draw_prepare.inc',
                'include/nvn_native/pipeline_storage.h', 'include/nvn_native/pipeline_recipe.h',
                'src/pipeline.c', 'src/pipeline_program.c', 'src/nvn_pipeline_source.inc',
                'include/nvn_native/nvn.h'):
        p = native / rel
        inputs[str(p)] = sha(p)
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    manifest = dict(schema=1, kind='original-host-native-graphics-storage-v1',
        graphics_storage=True, default_enabled=False,
        graphics_check=str(Path(graphics_check).resolve()), graphics_header=str(header),
        fdl_check=str(Path(fdl_check).resolve()), runtime_fdl_library=str(Path(library).resolve()),
        shader_roles='typed VS/FS 32-bit read/write/size and sparse bindings; 16 slots per stage',
        unknown_reflection='ENOTSUP; no invented compiler query answers',
        gpu_qualification=False, original_module_execution=False, inputs=inputs)
    return flags, manifest, inputs


def audit(native, manifest, commands):
    native = Path(native).resolve()
    rows, links = [], []
    for line in commands.splitlines():
        argv = shlex.split(line)
        if '-c' in argv:
            rows.append((Path(argv[argv.index('-c') + 1]).resolve(), argv))
        else:
            links.append(argv)
    enabled = manifest is not None
    for name in SOURCES:
        if sum(p == native / 'src' / name for p, _ in rows) != int(enabled):
            raise ValueError('storage source missing, duplicated or unrequested: ' + name)
    for name in ('nvn.c', 'pipeline.c', 'pipeline_program.c', *SOURCES):
        selected = [a for p, a in rows if p == native / 'src' / name]
        if name in SOURCES and not enabled:
            continue
        if len(selected) != 1:
            raise ValueError('storage consumer missing or duplicate: ' + name)
        for macro in MACROS:
            prefix = '-D' + macro
            actual = [a for a in selected[0] if a == prefix or a.startswith(prefix + '=')]
            if actual != ([prefix + '=1'] if enabled else []):
                raise ValueError('storage actual compile mode differs: ' + name + ' ' + macro)
    bridges = [a for p, a in rows if p == native / 'adapters/d2r/host_bridge.c']
    if len(bridges) != 1:
        raise ValueError('storage bridge missing or duplicate')
    macro = '-DNN_NVN_GRAPHICS_STORAGE'
    actual = [a for a in bridges[0] if a == macro or a.startswith(macro + '=')]
    if actual != ([macro + '=1'] if enabled else []):
        raise ValueError('storage bridge actual registration mode differs')
    if enabled:
        header = '-DNN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER="' + manifest['graphics_header'] + '"'
        if header not in bridges[0]:
            raise ValueError('storage bridge actual archive differs from checked catalogue')
        if not any(manifest['runtime_fdl_library'] in argv for argv in links):
            raise ValueError('storage verified FDL missing from actual link')
        for path, value in manifest['inputs'].items():
            if sha(path) != value:
                raise ValueError('storage feature source/product changed: ' + path)
    if any('/tests/' in str(p) for p, _ in rows):
        raise ValueError('storage qualification source entered production host')
