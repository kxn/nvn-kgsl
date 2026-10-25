#!/usr/bin/env python3
"""Extract original embedded CS inputs from a verified D2R NSO, without rewriting.

This adapter qualifies one known container profile, not all NVN control versions.
All game bytes and derived outputs must stay in an ignored local directory.
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

INPUT_SHA = '55fd909bb7230b002357e5b0d8499c11f06888f345c49e69eb4734df44a27d0e'
# Container identities, with full caller-supplied control-blob hashes. These
# do not assert how many bytes an NVIDIA driver internally interprets.
CONTROLS = [
    (0x1df0fd4, '77c077e746c11e5c05332a3f3a927b043cdd6af31fae9435febd966c97697aea'),
    (0x1df1980, 'ba910e530f373ebd9f7eb9d89ab4ea23071b0e513f42f02c1f1f32173cb7370e'),
    (0x1df232c, 'bd1a10a81a86da8a1de83d0e9e552f223813c2f5e1dde6ccbe87296bb7f6b3b9'),
    (0x1df2cd8, '0e9127ff0aca074d7c7ec2a1df3dfd46414091d04a6baa2324ccba88165f240b'),
]


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
    root = Path(__file__).resolve().parents[2]
    data = args.nso.read_bytes()
    if data[:4] != b'NSO0' or sha(data) != INPUT_SHA:
        p.error('expected the pinned unmodified original main NSO')
    args.out.mkdir(parents=True, exist_ok=True)
    raw_path = args.out / 'main.raw'
    tool = d2r / 'dm/tools/nso.py'
    subprocess.run([sys.executable, str(tool), str(args.nso), str(raw_path)], check=True)
    raw = raw_path.read_bytes()
    for i in range(3):
        _, offset, size = struct.unpack_from('<III', data, 0x10 + i * 16)
        if hashlib.sha256(raw[offset:offset + size]).digest() != data[0xa0 + i * 32:0xc0 + i * 32]:
            p.error('original NSO segment hash mismatch')
    # These are explicit translation-profile choices. They are not automatically
    # inherited IGpuAccessor defaults, nor a claim of GPU feature qualification.
    host = {
        'QueryHostSupportsTransformFeedback': False,
        'QueryTransformFeedbackEnabled': False,
        'QueryHasUnalignedStorageBuffer': False,
        'QueryHostSupportsShaderFloat64': False,
        'QueryHostHasVectorIndexingBug': False,
        'QueryHostFragmentQuadTransfer': False,
        'QueryHostReducedPrecision': False,
        'QueryHostSupportsFragmentShaderInterlock': False,
        'QueryHostSupportsFragmentShaderOrderingIntel': False,
        'QueryHostSupportsGeometryShaderPassthrough': False,
        'QueryHostSupportsShaderBallot': False,
        'QueryHostSupportsShaderBarrierDivergence': True,
        'QueryHostSupportsShaderNonUniformIndexing': False,
        'QueryHostSupportsTextureShadowLod': False,
        'QueryHostSupportsViewportMask': False,
    }
    results = []
    def in_segment(offset, size):
        return any(offset >= start and size <= length - (offset - start)
                   for _, start, length in (struct.unpack_from('<III', data, 0x10 + i * 16) for i in range(3))
                   if offset - start <= length)
    for container, expected_full in CONTROLS:
        if not in_segment(container, 20): p.error('container outside verified NSO segments')
        version, control_bytes, control_relative, code_bytes, code_relative = struct.unpack_from('<IIiIi', raw, container)
        offset = container + 8 + control_relative
        code_offset = container + 16 + code_relative
        if version != 5 or control_bytes != 0x898 or code_bytes != 256 or code_relative != 4 or offset != code_offset + code_bytes:
            p.error('original caller container shape changed')
        if not in_segment(offset, control_bytes) or not in_segment(code_offset, code_bytes):
            p.error('declared source range outside verified NSO segments')
        control = raw[offset:offset + control_bytes]
        if control[:4] != bytes.fromhex('34127698') or sha(control) != expected_full:
            p.error('full caller control blob does not match verified original input')
        expected = expected_full[:16]
        code_end, total = struct.unpack_from('<II', control, 0x700)
        if not (0 < code_end <= total == code_bytes <= 4096) or total % 8:
            p.error('unsupported code/constant range')
        code = raw[code_offset:code_offset + code_end]
        c1 = raw[code_offset + code_end:offset]
        local = list(struct.unpack_from('<III', control, 0x748))
        shared = struct.unpack_from('<I', control, 0x754)[0]
        if local != [32, 1, 1] or shared != 0 or c1:
            p.error('embedded CS profile changed; audit metadata before translating')
        blobs = {}
        for name, contents in [('code', code), ('control', control), ('c1', c1)]:
            filename = f'{expected}.{name}.bin'
            (args.out / filename).write_bytes(contents)
            blobs[name] = {'path': filename, 'offset': 0, 'size': len(contents), 'sha256': sha(contents)}
        fixture = {'schema': 1, 'stage': 'cs', 'profile': {'name': 'nvn-embedded-cs-v5-full-control',
                       'control_extent': {'kind': 'caller-container-declared', 'bytes': control_bytes, 'container_version': version}},
                   'compiler': {'frontend': 'vendored Maxwell translator, strict accessor v1',
                       'iadd_overlay_sha256': sha((root / 'shader/maxwell/iadd_overlay.py').read_bytes())},
                   'blobs': blobs,
                   'state': {'resources': {'queries': {'QueryConstantBufferUse': 9,
                       'QuerySamplerArrayLengthFromPool': 0, 'QueryTextureArrayLengthFromPool': 0}},
                       'host': host, 'compute': {'local_size': local, 'shared_bytes': shared, 'local_bytes': 0},
                       'graphics': None, 'precision': {'mode': 'strict'}},
                   'provenance': {'nso_sha256': INPUT_SHA, 'build_id': data[0x40:0x60].hex(),
                       'container_offset': container, 'control_offset': offset, 'code_offset': code_offset,
                       'caller_control_bytes': control_bytes, 'caller_code_bytes': code_bytes, 'extractor_sha256': sha(Path(__file__).read_bytes()),
                       'nso_tool_sha256': sha(tool.read_bytes()),
                       'local_bytes_basis': 'explicit validation profile: these CS kernels use no local memory; not a generic control-field inference'}}
        path = args.out / f'{expected}.fixture.json'
        path.write_text(json.dumps(fixture, indent=2) + '\n')
        results.append({'fixture': str(path), 'control_sha256': sha(control), 'code_sha256': sha(code)})
    (args.out / 'extraction.json').write_text(json.dumps({'input_sha256': INPUT_SHA,
        'raw_sha256': sha(raw), 'segment_hashes_verified': True, 'outputs': results}, indent=2) + '\n')
    print(args.out / 'extraction.json')


if __name__ == '__main__':
    main()
