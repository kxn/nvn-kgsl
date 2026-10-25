#!/usr/bin/env python3
"""Audit pinned SDK UBO binding methods/encoding by static inspection only.

Assembly stays in fresh ignored output. static.json contains only provenance,
method addresses/hashes and decoded packet metadata; never a dynamic/GPU test.
Original SDK bytes are emitted only as ignored disassembly, never shipped.
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
    parser.add_argument('--root', type=Path, help='Source checkout; default tool repo root')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--method', action='append', default=[],
                        help='Also disassemble a named original SDK method; repeatable. Static evidence only.')
    args = parser.parse_args()
    root = (args.root or Path(__file__).resolve().parents[2]).resolve()
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
    selected = ['nvnCommandBufferBindUniformBuffer','nvnCommandBufferBindUniformBuffers',
                'nvnCommandBufferUpdateUniformBuffer','nvnDeviceGetInteger','nvnQueueSubmitCommands']
    for name in args.method:
        if name not in names:
            parser.error('unknown original SDK method: ' + name)
        if name not in selected:
            selected.append(name)
    targets = sorted({address for _, address in names.values()})
    disassembler = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    methods, assembly = {}, []
    for name in selected:
        index, address = names[name]
        limit = 8192
        end = min([v for v in targets if v > address] + [address+limit])
        end = min(end, address+limit)
        if name=='nvnCommandBufferBindUniformBuffer':
            end=0x480d08 # following allocator is not this method
        methods[name] = dict(index=index, target=address, end=end,
                             body_sha256=sha(image[address:end]),
                             method_slot=0xc63e58+index*8,
                             rela=relocations.get(0xc63e58+index*8))
        assembly.append(f'\n{name} SDK+0x{address:x}..0x{end:x}')
        assembly.extend(f'{i.address:x}: {i.mnemonic} {i.op_str}'
                        for i in disassembler.disasm(image[address:end], address))
    helpers={}
    for label,address,end in [('device_helper',0x45b0a0,0x45b190),
          ('device_consumer',0x46afc0,0x46b100),('control_consumer',0x472da0,0x473070),
          ('compute_bind',0x486f40,0x487048)]:
        helpers[label]=dict(address=address,end=end,sha256=sha(image[address:end]))
        assembly.append('\n'+label+' SDK+0x%x..0x%x'%(address,end))
        assembly.extend(f'{i.address:x}: {i.mnemonic} {i.op_str}'
                        for i in disassembler.disasm(image[address:end],address))
    # Stage==5 branches before this graphics table; the sixth entry is not a
    # graphics method. No instruction emulation or reconstructed SDK execution.
    expected=[0x2410,0x2490,0x2470,0x2430,0x2450]
    stages=[]
    for stage in range(5):
        address=0xabf4c8+4*stage;method=u32(address)
        if method!=expected[stage]:raise ValueError('unexpected stage table')
        stages.append(dict(api_stage=stage,table_address=address,method=method,
                           hardware_group=(method-0x2410)//32))
    queries={}
    for prop,value in [(2,14),(3,65536),(4,256),(0x3c,4)]:
        address=0xabaa84+prop-2;target=0x45b0d0+4*image[address]
        consumer=0x46b00c+4*image[0xabaf72+prop]
        if consumer!=0x46b028:raise ValueError('device override must be reviewed')
        if prop!=2:
            instruction=next(disassembler.disasm(image[target:target+4],target))
            if (instruction.mnemonic!='mov' or not instruction.op_str.startswith('w8, #') or
                    int(instruction.op_str.split('#',1)[1],0)!=value):
                raise ValueError('device query immediate requires review')
        queries[str(prop)]=dict(table_address=address,target=target,
            consumer_table_address=0xabaf72+prop,consumer=consumer,value=value)
    default_count=next(disassembler.disasm(image[0x45b0c8:0x45b0cc],0x45b0c8))
    if (image[0xabaa84]!=43 or default_count.mnemonic!='mov' or
            default_count.op_str!='w8, #0xe'):
        raise ValueError('uniform binding count default is not fourteen')
    # Verify algebra for legal API index domain against public field locations.
    algebra=[]
    for stage in stages:
        for index in range(14):
            word=0x80010000|(((index<<20)+0x300000)&0x1f00000)|(stage['method']>>2)
            slot=(word>>20)&31;valid=(word>>16)&1;method=(word&0x1fff)*4
            if(slot,valid,method)!=(index+3,1,stage['method']):
                raise ValueError('unexpected static bit algebra')
            algebra.append(dict(stage=stage['api_stage'],index=index,source_cb=slot))
    opcode_address=0xabf08e+12-3;opcode_target=0x472dcc+4*image[opcode_address]
    if opcode_target!=0x472e54:raise ValueError('compute control opcode mismatch')
    report=dict(schema=1,options=dict(root=str(root),sdk=str(args.sdk.resolve()),out=str(out)),scope='Pinned SDK53.314 static audit; no ARM execution, CPU emulation, GPU or renderer',
        sdk=dict(path=str(args.sdk.resolve()),sha256=SDK_SHA,build_id=original[0x40:0x60].hex(),
                 raw_sha256=sha(image),verified_segments=segments),
        sources={str(path):sha(path.read_bytes()) for path in (Path(__file__).resolve(),unpacker)},
        tools=dict(python=sha(Path(sys.executable).resolve().read_bytes()),
                   capstone=importlib.metadata.version('capstone'),lz4=importlib.metadata.version('lz4')),
        method_count=len(methods),methods=methods,helpers=helpers,graphics_stages=stages,
        device_queries=queries,graphics_mapping=dict(api_first=0,api_count=14,source_cb_first=3,
          source_cb_last=16,formula='sourceCB = APIindex + 3',stage_domain=[0,1,2,3,4],
          legal_field_algebra_checks=len(algebra)),
        compute=dict(control_opcode=12,table_address=opcode_address,consumer=opcode_target,
          callee=0x486f40,direct_qmd_formula='sourceCB = APIindex + 3',direct_qmd_api_first=0,
          direct_qmd_api_last=4,remaining_api='5..13 saved in driver metadata; not a direct QMD bind here'),
        update=dict(has_stage_or_index=False,selector=0x2380,load_offset=0x238c,data_method=0x2390,
          record_time_inline_copy=True,max_chunk_words=127),
        policy=dict(target_backend_implemented_by_this_tool=False,unknown_reserved_cb_contents_not_synthesized=True))
    (out / 'sdk-methods.asm').write_text('\n'.join(assembly)+'\n')
    (out / 'static.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(scope=report['scope'], method_count=len(methods), report=str(out/'static.json'))))


if __name__ == '__main__':
    main()
