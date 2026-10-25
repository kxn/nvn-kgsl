#!/usr/bin/env python3
"""Extract pinned SDK render-state methods for static review; never execute them.

Assembly stays in fresh ignored output. static.json contains only provenance,
method addresses/hashes and grid-table targets, not a claim of dynamic testing.
The semantic interpretation is documented in render-defaults-sdk-research.zh-CN.md.
"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
from workspace_paths import d2r_root
import struct
import subprocess
import sys
import tempfile

from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

SDK_SHA = '1c9f198a91c0bcfee5cf8f5b53fb941895e91334dfb766ce79fc68576797f017'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('fresh ignored output required')
    original = args.sdk.resolve().read_bytes()
    if original[:4] != b'NSO0' or sha(original) != SDK_SHA:
        parser.error('expected pinned unmodified SDK53.314')
    out.mkdir(parents=True)
    unpacker = d2r_root() / 'dm/tools/nso.py'
    with tempfile.TemporaryDirectory(prefix='render-sdk-', dir=out) as directory:
        raw = Path(directory) / 'sdk.raw'
        command = [sys.executable, str(unpacker), str(args.sdk.resolve()), str(raw)]
        subprocess.run(command, check=True, capture_output=True)
        image = raw.read_bytes()
    segments = []
    for i in range(3):
        _, offset, size = struct.unpack_from('<III', original, 0x10 + 16*i)
        digest = sha(image[offset:offset+size])
        if not size or digest != original[0xa0+32*i:0xc0+32*i].hex():
            raise ValueError('SDK segment integrity failure')
        segments.append(dict(offset=offset, size=size, sha256=digest))
    u32 = lambda offset: struct.unpack_from('<I', image, offset)[0]
    mod = u32(4)
    dynamic = mod + struct.unpack_from('<i', image, mod+4)[0]
    tags = {}
    while True:
        tag, value = struct.unpack_from('<QQ', image, dynamic)
        dynamic += 16
        if not tag:
            break
        tags[tag] = value
    relocations = {}
    for offset in range(tags[7], tags[7]+tags[8], 24):
        target, info, addend = struct.unpack_from('<QQq', image, offset)
        relocations[target] = (offset, info, addend)

    def pointer(slot):
        if slot not in relocations:
            return struct.unpack_from('<Q', image, slot)[0]
        _, info, addend = relocations[slot]
        if info & 0xffffffff == 1027:
            return addend
        if info & 0xffffffff not in (257, 1025):
            raise ValueError('unsupported SDK relocation')
        symbol = struct.unpack_from('<IBBHQQ', image, tags[6]+24*(info >> 32))
        return symbol[4] + addend

    names = {}
    for i in range(523):
        address = 0xabafd5 + u32(0xabe798 + 4*i)
        name = 'nvn' + image[address:image.index(0, address)].decode('ascii')
        names[name] = (i, pointer(0xc63e58 + 8*i))
    if len(names) != 523:
        raise ValueError('unexpected SDK method catalog')
    families = ('BlendState', 'ColorState', 'ChannelMaskState', 'PolygonState',
                'DepthStencilState', 'MultisampleState', 'VertexAttribState', 'VertexStreamState')
    selected = [n for n in names if any(f in n for f in families) and
                ('SetDefaults' in n or 'Get' in n or n == 'nvnMultisampleStateSetSampleLocations')]
    if len(selected) != 45:
        raise ValueError('unexpected render-state method count')
    targets = sorted({address for _, address in names.values()})
    disassembler = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    methods, assembly = {}, []
    for name in selected:
        index, address = names[name]
        limit = 4096 if name.endswith('SetSampleLocations') else 512
        end = min([v for v in targets if v > address] + [address+limit])
        end = min(end, address+limit)
        if name.endswith('SetSampleLocations'):
            end = 0x474860  # next internal helper, before exported SyncInitialize
        methods[name] = dict(index=index, target=address, end=end,
                             body_sha256=sha(image[address:end]),
                             method_slot=0xc63e58+index*8,
                             rela=relocations.get(0xc63e58+index*8))
        assembly.append(f'\n{name} SDK+0x{address:x}..0x{end:x}')
        assembly.extend(f'{i.address:x}: {i.mnemonic} {i.op_str}'
                        for i in disassembler.disasm(image[address:end], address))
    report = dict(scope='Original SDK static extraction/review only; no ARM execution, GPU or other renderer',
                  sdk=dict(sha256=SDK_SHA, build_id=original[0x40:0x60].hex(),
                           raw_sha256=sha(image), verified_segments=segments),
                  sources={str(p.relative_to(root)): sha(p.read_bytes())
                           for p in (Path(__file__).resolve(), unpacker)},
                  tools=dict(python=sha(Path(sys.executable).resolve().read_bytes()),
                             capstone=importlib.metadata.version('capstone'),
                             lz4=importlib.metadata.version('lz4')),
                  method_count=len(methods), methods=methods,
                  sample_grid_branch_offsets={str(n): dict(table_byte=image[0xabf0a2+n],
                      branch=0x47453c+4*image[0xabf0a2+n]) for n in (0, 1, 2, 4, 8, 16)})
    (out / 'sdk-methods.asm').write_text('\n'.join(assembly)+'\n')
    (out / 'static.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(scope=report['scope'], method_count=len(methods), report=str(out/'static.json'))))


if __name__ == '__main__':
    main()
