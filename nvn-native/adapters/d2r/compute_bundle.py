"""Package verified original CS/compiler outputs for a D2R-specific caller test.

Only ignored build outputs contain original game bytes or compiled shader code.
The native driver does not contain these identities or kernel-specific oracles.
"""
import hashlib
import json
from pathlib import Path

# Qualification-only oracle identities. Historical0x880 is a prefix profile;
# only the new caller-container profile supplies the complete0x898 blob.
# The production offline_compute_cache provider never imports these mappings.
ORACLES = {
    'aaf657ee393245946601fb3e396dc093c1ce3fb09a3ae29ba4875a093d51f090':
        ((16, 0, 0), {0x880: 'd0f769bb916bbba97e8d5907a2ab40349ed81e7b74383e870d78847114c28a7c',
                       0x898: '77c077e746c11e5c05332a3f3a927b043cdd6af31fae9435febd966c97697aea'}),
    'b8dd773133250801475b34d541c8f8a5f1e349166aba3df81b1f4c86774950bc':
        ((16, 8, 0), {0x880: '16479cc6d75c2b2816eb113c33f96e3917ee172ca363d45de6a49dadf22127cc',
                       0x898: 'ba910e530f373ebd9f7eb9d89ab4ea23071b0e513f42f02c1f1f32173cb7370e'}),
    'ddcb76a04571b0fedf35716c5fd17e4ba6a38c9309da8cf7a011444bd5b2c4be':
        ((32, 0, 1), {0x880: '0687e2ef9513357704103c9726f736575f958f0f314720b6315cff48cb07de5b',
                       0x898: 'bd1a10a81a86da8a1de83d0e9e552f223813c2f5e1dde6ccbe87296bb7f6b3b9'}),
    '9d09120c2f3995ef1b61c2479b9c0b37061db403994914de2fafb760e0431fc1':
        ((32, 8, 1), {0x880: '4df5909e6dabee0515e1d53c9ae001f6af58d55f7346eb881c92b7c1642ea372',
                       0x898: '0e9127ff0aca074d7c7ec2a1df3dfd46414091d04a6baa2324ccba88165f240b'}),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(check_path, out):
    check = json.loads(check_path.read_text())
    out.mkdir(parents=True, exist_ok=True)
    lines = ['/* Generated D2R-only qualification inputs, not native driver logic. */',
             '#include "nvn_native/nvn.h"']
    initializers, oracles, hashes = [], [], {}
    for index, case in enumerate(check['cases']):
        fixture = Path(case['fixture'])
        if sha(fixture) != case['fixture_sha256']:
            raise ValueError('fixture identity changed after compiler check')
        document = json.loads(fixture.read_text())
        code, control = document['blobs']['code'], document['blobs']['control']
        identity = ORACLES.get(code['sha256'])
        if (identity is None or identity[1].get(control['size']) != control['sha256'] or
                any({key: document['blobs'][name][key] for key in ('size', 'sha256')} !=
                    case['fixture_check']['inputs'][name] for name in ('code', 'control'))):
            raise ValueError('no D2R-specific test oracle for this complete source identity')
        profile = {'name': 'd2r-embedded-cs-control-0x880'} if control['size'] == 0x880 else {
            'name': 'nvn-embedded-cs-v5-full-control',
            'control_extent': {'kind': 'caller-container-declared', 'bytes': 0x898, 'container_version': 5}}
        if document['profile'] != profile:
            raise ValueError('qualification control extent/profile mismatch')
        oracles.append(identity[0])
        prefix = check_path.parent / case['fixture_check']['request_sha256'][:16]
        for suffix, expected in case['artifacts'].items():
            if sha(Path(str(prefix) + suffix)) != expected:
                raise ValueError('compiler artifact changed: ' + suffix)
        source = Path(str(prefix) + '.native.h')
        header = out / f'program_{index}.h'
        header.write_bytes(source.read_bytes())
        hashes[str(header)] = sha(header)
        lines += [f'#include "{header.name}"']
        for name in ('code', 'control', 'c1'):
            blob = document['blobs'][name]
            with (fixture.parent / blob['path']).open('rb') as stream:
                stream.seek(blob['offset']); data = stream.read(blob['size'])
            if hashlib.sha256(data).hexdigest() != blob['sha256']:
                raise ValueError('original source blob changed')
            if name == 'c1':
                if data:
                    raise ValueError('nonempty compile-time C1 is not qualified')
                continue
            lines += [f'static const unsigned char source_{name}_{index}[] = {{' + ','.join(map(str, data)) + '};']
        cbufs, sbufs = case['resources']['cbufs'], case['resources']['sbufs']
        lines += [f'static const struct nn_nvn_cbuf cbufs_{index}[] = {{' +
                  ','.join(f"{{{b['Slot']},{b['Binding']}}}" for b in cbufs) + '};',
                  f'static const struct nn_nvn_sbuf sbufs_{index}[] = {{' +
                  ','.join(f"{{{b['Binding']},{b['SbCbSlot']},{b['SbCbOffset']}}}" for b in sbufs) + '};',
                  f'static const struct nn_nvn_compiled_cs compiled_{index} = {{' +
                  f'&NVN_COMPILED_IMAGE,source_code_{index},source_control_{index},sizeof(source_code_{index}),sizeof(source_control_{index}),' +
                  f'cbufs_{index},{len(cbufs)},sbufs_{index},{len(sbufs)}}};',
                  '#undef NVN_COMPILED_IMAGE']
        initializers.append(f'&compiled_{index}')
    if len(initializers) != 4 or len(set(oracles)) != 4:
        raise ValueError('expected all four distinct D2R embedded CS')
    lines += ['static const struct nn_nvn_compiled_cs *test_programs[] = {' + ','.join(initializers) + '};',
              'static const struct {unsigned stride,offset,delta;} test_oracles[] = {' +
              ','.join(f'{{{s},{o},{d}}}' for s, o, d in oracles) + '};']
    bundle = out / 'compute_bundle.h'
    bundle.write_text('\n'.join(lines) + '\n')
    hashes[str(bundle)] = sha(bundle)
    return bundle, check, hashes
