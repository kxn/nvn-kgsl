"""Explicit original-host sampled/Gather consumer policy and actual link audit.

No shader selection, image conversion, GPU qualification or game claims. The
shared texture_bc_shadow owner ledger does not imply the BC consumer is enabled.
"""
import hashlib
import shlex
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_options(sampled, mip_filter, gather, graphics, fdl, block, bridge):
    if any(type(value) is not bool for value in (sampled, mip_filter, gather)):
        raise ValueError('sampled feature switches must be explicit booleans')
    if mip_filter and not sampled:
        raise ValueError('--sampled-mip-filter requires --sampled-images')
    if sampled and not all((graphics, fdl, block)):
        raise ValueError('--sampled-images requires graphics, runtime FDL and block-copy checks')
    if gather and (not graphics or not (fdl or bridge)):
        raise ValueError('--gather requires graphics and runtime FDL or native-view bridge inputs')


def prepare(root, sampled, mip_filter, gather, graphics_check, fdl_check,
            block_check, bridge_check=None, target_library=None, bc_library=None,
            color_targets=False):
    validate_options(sampled, mip_filter, gather, graphics_check, fdl_check,
                     block_check, bridge_check)
    flags = ['-DNVN_D2R_SAMPLED_IMAGES=' + ('ON' if sampled else 'OFF'),
             '-DNVN_D2R_SAMPLED_MIP_FILTER=' + ('ON' if mip_filter else 'OFF'),
             '-DNVN_D2R_GATHER=' + ('ON' if gather else 'OFF')]
    if not sampled and not gather:
        return flags, None, {}
    native = root / 'nvn-native'
    inputs = {}
    library = None
    block = None
    if sampled:
        from build_runtime_fdl import verify
        path = Path(fdl_check).resolve()
        library, record = verify(root, path)
        library = Path(library).resolve()
        if not target_library or library != Path(target_library).resolve():
            raise ValueError('sampled and target features require the same verified runtime FDL archive')
        if bc_library and library != Path(bc_library).resolve():
            raise ValueError('sampled and BC features require the same verified runtime FDL archive')
        inputs.update({str(path): sha(path), **record['inputs'], **record['artifacts']})
        from compile_block_copy import verify as verify_block
        block_path = Path(block_check).resolve()
        block = verify_block(root, block_path)
        inputs.update({str(block_path): sha(block_path), **block['inputs'], **block['artifacts']})
        for rel in ('src/sampled_image.c', 'src/texture_bc_shadow.c',
                    'include/nvn_native/sampled_image.h',
                    'include/nvn_native/texture_bc_shadow.h',
                    'include/nvn_native/runtime_fdl.h'):
            p = native / rel
            inputs[str(p)] = sha(p)
    for rel in ('src/pipeline.c', 'src/pipeline_program.c',
                'src/nvn_pipeline_draw_prepare.inc', 'src/nvn_pipeline_draw_submit.inc',
                'include/nvn_native/pipeline_texture_types.h',
                'include/nvn_native/sampler_state.h', 'src/sampler_state.c'):
        p = native / rel
        inputs[str(p)] = sha(p)
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    for p, expected in inputs.items():
        if sha(p) != expected:
            raise ValueError('sampled feature input changed: ' + p)
    manifest = dict(
        schema=1, kind='original-host-native-sampled-features-v1',
        sampled_images=sampled, sampled_mip_filter=mip_filter, gather=gather,
        runtime_fdl_check=str(Path(fdl_check).resolve()) if sampled else None,
        runtime_fdl_library=str(library) if library else None,
        runtime_fdl_library_sha256=sha(library) if library else None,
        block_copy_check=str(Path(block_check).resolve()) if sampled else None,
        block_copy_header=block['header'] if block else None,
        source_role='actual FDL FLOAT/uncompressed/ordinary2D/layer1, full/contiguous mip view' if sampled else 'legacy RGBA8 normalized2D/mip1',
        semantic_operations=['SAMPLE', 'FETCH', 'SIZE'] + (['GATHER16'] if gather else []),
        typed_facts='exact Program-owned recipe/SPIRV; Gather other==corroborated count, no offsets',
        sampler_profile='actual mipmapped encoder; multilevel aniso1, single-level existing validated profile' if mip_filter else 'existing encoder; multi-mip nearest/clamp',
        canonical_target_path='typed COLOR+block' if color_targets else 'legacy RGBA8 bridge' if bridge_check else 'unavailable; LINEAR RGBA8 target remains possible',
        canonical_source_path='sampled-image owner' if sampled else 'legacy RGBA8 bridge only' if bridge_check else 'unavailable; LINEAR RGBA8 source remains possible',
        shared_owner_ledger='texture_bc_shadow.c once; BC admission is separately explicit',
        public_gather_source='original full source/control/C1 matching only; authored native artifacts cannot register; CASC FragDepth still needs complete public depth path',
        canonical_source_preserved=True, cpu_pixel_conversion=False,
        gpu_qualification=False, game_compatibility=False, default_enabled=False,
        inputs=inputs)
    return flags, manifest, inputs


def audit(native, manifest, commands, bc_manifest=None):
    native = Path(native).resolve()
    rows, links = [], []
    for line in commands.splitlines():
        parts = shlex.split(line)
        if '-c' in parts:
            rows.append((Path(parts[parts.index('-c') + 1]).resolve(), parts))
        else:
            links.append(parts)
    sampled = bool(manifest and manifest['sampled_images'])
    mip_filter = bool(manifest and manifest['sampled_mip_filter'])
    gather = bool(manifest and manifest['gather'])
    for name, required in (('sampled_image.c', sampled),
                           ('texture_bc_shadow.c', sampled or bool(bc_manifest))):
        count = sum(p == native / 'src' / name for p, _ in rows)
        if count != int(required):
            raise ValueError('sampled/BC owner source missing, duplicated or unrequested: ' + name)
    for name in ('nvn.c', 'pipeline.c', 'sampler_state.c') + (('sampled_image.c', 'texture_bc_shadow.c') if sampled else ()):
        selected = [parts for p, parts in rows if p == native / 'src' / name]
        if len(selected) != 1:
            raise ValueError('sampled actual compile source missing or duplicated: ' + name)
        for macro, enabled in (('NN_PIPELINE_SAMPLED_IMAGES', sampled),
                               ('NN_NVN_SAMPLED_IMAGES', sampled),
                               ('NN_PIPELINE_SAMPLED_MIP_FILTER', mip_filter),
                               ('NN_PIPELINE_GATHER', gather)):
            prefix = '-D' + macro
            actual = [x for x in selected[0] if x == prefix or x.startswith(prefix + '=')]
            if actual != ([prefix + '=1'] if enabled else []):
                raise ValueError('sampled actual compile mode differs: ' + name + ' ' + macro)
    if sampled:
        core = next(parts for p, parts in rows if p == native / 'src/nvn.c')
        if '-DNN_NVN_BLOCK_COPY=1' not in core:
            raise ValueError('sampled source conversion not compiled')
        bridge = [parts for p, parts in rows if p == native / 'adapters/d2r/host_bridge.c']
        # A build has to install the actual immutable kernel at Device creation;
        # merely compiling block_copy.c or selecting an option is insufficient.
        if len(bridge) != 1 or '-DNN_NVN_BLOCK_COPY_HEADER="' + manifest['block_copy_header'] + '"' not in bridge[0]:
            raise ValueError('sampled raw-block kernel absent from actual Device bridge')
        library = manifest['runtime_fdl_library']
        if not any(library in line for line in links):
            raise ValueError('sampled normal runtime FDL archive absent from actual link')
    if any('sampled_probe' in str(p) or 'pipeline_gather_sampled_images' in str(p) for p, _ in rows):
        raise ValueError('sampled qualification code present in production host')
    if manifest:
        for p, expected in manifest['inputs'].items():
            if sha(p) != expected:
                raise ValueError('sampled source/product changed: ' + p)
