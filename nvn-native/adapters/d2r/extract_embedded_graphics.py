#!/usr/bin/env python3
"""Extract original untextured embedded VS/FS from the pinned, verified NSO.

Reuse the existing NSO verification path. No PC shader, reflection or game
rendering implementation supplies code or specialization facts to this adapter.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from workspace_paths import d2r_root


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--d2r-root', type=Path, help='External D2R validation checkout; or D2R_ROOT')
    p.add_argument('--nso', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    try:
        d2r = d2r_root(args.d2r_root)
    except ValueError as exc:
        p.error(str(exc))
    args.out.mkdir(parents=True, exist_ok=True)
    verifier = Path(__file__).with_name('extract_embedded_cs.py')
    subprocess.run([sys.executable, str(verifier), '--d2r-root', str(d2r), '--nso', str(args.nso), '--out', str(args.out / 'verified')], check=True)
    raw = (args.out / 'verified/main.raw').read_bytes()
    extraction = json.loads((args.out / 'verified/extraction.json').read_text())
    reference_path = Path(extraction['outputs'][0]['fixture']).name
    reference = json.loads((args.out / 'verified' / reference_path).read_text())
    # CS caller-container metadata must not be inherited by graphics prefixes.
    for name in ('container_offset', 'code_offset', 'caller_control_bytes', 'caller_code_bytes'):
        reference['provenance'].pop(name, None)
    # Explicit translation/draw profile for these inputs. These are not inferred
    # from a PC counterpart or an implicit IGpuAccessor answer.
    graphics = dict(early_z_force=False, topology=3, tess_cw=False, tess_patch_type=1,
        tess_spacing=0, alpha_to_coverage_enable=False, alpha_to_coverage_dither_enable=False,
        viewport_transform_disable=False, depth_mode=True, program_point_size_enable=False,
        point_size=1.0, alpha_test_compare=8, alpha_test_reference=0.0,
        attribute_types=[0] * 32, has_constant_buffer_draw_parameters=False,
        fragment_output_types=[0] * 8, dual_source_blend_enable=False,
        y_negate_enabled=False, origin_upper_left=True, halve_primitive_id=False)
    for offset, expected, stage in [(0x1db4eac, '8ac93c54cfb3cbe3', 'vs'), (0x1db5840, '0d721a0acc36c0bf', 'fs')]:
        control = raw[offset:offset + 0x880]
        if sha(control)[:16] != expected or control[:4] != bytes.fromhex('34127698'):
            p.error('original graphics control profile changed')
        end, total = struct.unpack_from('<II', control, 0x700)
        if not (0x80 <= end <= total <= 4096) or total % 8:
            p.error('unsupported graphics code/C1 container range')
        code = raw[offset - total:offset - total + end]
        c1 = raw[offset - total + end:offset]
        if code[:4] != bytes.fromhex('78563412') or ((struct.unpack_from('<I', code, 0x30)[0] >> 10) & 15) != (1 if stage == 'vs' else 5):
            p.error('graphics container/SPH mismatch')
        fixture = json.loads(json.dumps(reference))
        fixture.update(stage=stage, profile={'name': 'd2r-embedded-graphics-0x880'},
            provenance={**reference['provenance'], 'control_offset': offset,
                        'extractor_sha256': sha(Path(__file__).read_bytes()),
                        'container_code_offset': offset - total})
        fixture['provenance'].pop('local_bytes_basis')
        fixture['state']['compute'] = None
        fixture['state']['graphics'] = graphics
        fixture['state']['resources']['queries']['QueryConstantBufferUse'] = 8
        fixture['state']['host'].update(QueryHasConstantBufferDrawParameters=False,
            QueryHostSupportsDepthClipControl=True, QueryHostSupportsGeometryShader=False,
            QueryHostSupportsLayerVertexTessellation=False, QueryHostSupportsViewportIndexVertexTessellation=False)
        fixture['state']['host']['QueryHostSupportsScaledVertexFormats'] = False
        # Native render-target format/swizzle path; avoid the emitter's optional
        # support-buffer emulation ABI. Hardware qualification remains separate.
        fixture['state']['host']['QueryHostSupportsBgraFormat'] = True
        for name, data in [('code', code), ('control', control), ('c1', c1)]:
            filename = f'{expected}.{name}.bin'; (args.out / filename).write_bytes(data)
            fixture['blobs'][name] = dict(path=filename, offset=0, size=len(data), sha256=sha(data))
        path = args.out / f'{expected}.fixture.json'
        path.write_text(json.dumps(fixture, indent=2) + '\n')
        print(path)


if __name__ == '__main__':
    main()
