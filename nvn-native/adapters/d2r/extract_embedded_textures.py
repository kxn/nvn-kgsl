#!/usr/bin/env python3
"""Extract an original VS/sampled FS pair with an explicit test profile.

The profile supplies shader specialization, not captured game texture state or
native image formats. All original bytes and derived outputs stay under .local.
"""
import argparse
import copy
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
    p.add_argument('--texture-profile', type=Path, required=True,
                   help='JSON with explicit textures array and host query overrides; no implicit texture facts')
    args = p.parse_args()
    try:
        d2r = d2r_root(args.d2r_root)
    except ValueError as exc:
        p.error(str(exc))
    root = Path(__file__).resolve().parents[3]
    out = args.out.resolve()
    if not out.is_relative_to(root / '.local'):
        p.error('original shader artifacts must stay under this worktree .local directory')
    profile_bytes = args.texture_profile.read_bytes()
    profile = json.loads(profile_bytes)
    if set(profile) != {'textures', 'host'} or not isinstance(profile['textures'], list) or not isinstance(profile['host'], dict):
        p.error('texture profile requires exactly textures array and host query object')
    keys = set()
    for texture in profile['textures']:
        if set(texture) != {'handle', 'cbuf_slot', 'sampler_type', 'coord_normalized'}:
            p.error('texture requires handle, cbuf_slot, sampler_type, coord_normalized; native formats belong to resource layout')
        key = (texture['handle'], texture['cbuf_slot'])
        if (any(type(v) is not int for v in key) or key in keys or
            texture['sampler_type'] != 'Texture2D' or texture['coord_normalized'] is not True):
            p.error('first profile requires unique explicit keys, sampled Texture2D and normalized coordinates')
        keys.add(key)
    if not keys:
        p.error('texture profile must explicitly describe at least one resource')
    # Reuse original input hash, NSO segment verification and existing explicit
    # graphics profile. No already rewritten binary or foreign shader is read.
    extractor = Path(__file__).with_name('extract_embedded_graphics.py')
    subprocess.run([sys.executable, str(extractor), '--d2r-root', str(d2r), '--nso', str(args.nso), '--out', str(out / 'reference')], check=True)
    raw_path = out / 'reference/verified/main.raw'
    raw = raw_path.read_bytes()
    outputs = {}
    for stage, offset, expected, reference in [
        ('vs', 0x1db3884, '96757bb36a024b91', '8ac93c54cfb3cbe3'),
        ('fs', 0x1db4418, 'a7672f1d3bfeb7d0', '0d721a0acc36c0bf'),
    ]:
        source = json.loads((out / f'reference/{reference}.fixture.json').read_text())
        control = raw[offset:offset + 0x880]
        if sha(control)[:16] != expected or control[:4] != bytes.fromhex('34127698'):
            p.error('original sampled graphics control profile changed: ' + stage)
        end, total = struct.unpack_from('<II', control, 0x700)
        if not (0x80 <= end <= total <= 4096) or total % 8 or end != 768:
            p.error('unsupported graphics code/C1 container range: ' + stage)
        code, c1 = raw[offset - total:offset - total + end], raw[offset - total + end:offset]
        if code[:4] != bytes.fromhex('78563412') or ((struct.unpack_from('<I', code, 0x30)[0] >> 10) & 15) != (1 if stage == 'vs' else 5):
            p.error('graphics container/SPH mismatch: ' + stage)
        fixture = copy.deepcopy(source)
        fixture['profile'] = {'name': 'd2r-embedded-sampled-2d-test-0x880',
            'scope': 'original shader with declared test specialization; not captured original draw resource state'}
        if stage == 'fs':
            fixture['state']['resources']['textures'] = profile['textures']
        fixture['state']['host'].update(profile['host'])
        fixture['provenance'].update(control_offset=offset, container_code_offset=offset - total,
            extractor_sha256=sha(Path(__file__).read_bytes()),
            graphics_extractor_sha256=sha(extractor.read_bytes()),
            texture_profile_sha256=sha(profile_bytes),
            specialization_basis='explicit caller-supplied normalized 2D test profile; no native format inference')
        for name, data in [('code', code), ('control', control), ('c1', c1)]:
            filename = f'{expected}.{name}.bin'
            (out / filename).write_bytes(data)
            fixture['blobs'][name] = dict(path=filename, offset=0, size=len(data), sha256=sha(data))
        fixture_path = out / f'{expected}.fixture.json'
        fixture_path.write_text(json.dumps(fixture, indent=2) + '\n')
        outputs[stage] = {'fixture': str(fixture_path), 'fixture_sha256': sha(fixture_path.read_bytes()),
                         'code_sha256': sha(code), 'control_sha256': sha(control)}
        print(fixture_path)
    verification = json.loads((out / 'reference/verified/extraction.json').read_text())
    result = {'schema': 1, 'scope': 'bounded original texture VS/FS extraction, no translation or GPU execution',
        'input_sha256': verification['input_sha256'], 'raw_sha256': verification['raw_sha256'],
        'segment_hashes_verified': verification['segment_hashes_verified'],
        'texture_profile_sha256': sha(profile_bytes), 'outputs': outputs}
    (out / 'extraction.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
