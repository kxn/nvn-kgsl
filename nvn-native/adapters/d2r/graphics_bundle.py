"""Package a verified original-shader graphics artifact for the native probe."""
import hashlib
import json
import struct
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_image_metadata(image, *, chip, gmem, modes, profile):
    """Validate the generated bounded image contract, without scanning PM4."""
    schema = 3 if modes else 2 if chip == 0x06050002 or profile else 1
    numeric = ('schema', 'chip', 'gmem_bytes', 'width', 'height', 'target_pitch',
               'target_bytes', 'target_layer_bytes', 'target_alignment')
    if not isinstance(image, dict) or any(type(image.get(key)) is not int for key in numeric):
        raise ValueError('invalid native graphics image metadata types')
    if (type(modes) is not bool or image.get('schema') != schema or image.get('chip') != chip or
            image.get('gmem_bytes') != gmem or image.get('width') != 64 or image.get('height') != 64 or
            image.get('target_pitch') != 256 or image.get('target_bytes') != 16384 or
            image.get('target_layer_bytes') != 16384 or image.get('target_alignment') != 64 or
            image.get('profile', 0) != profile or
            image.get('sequence_bytes', 0) != (4 if chip == 0x06050002 else 0)):
        raise ValueError('native graphics image profile/schema/device mismatch')
    words, addresses = image.get('command_words'), image.get('patches')
    if (type(words) is not int or not 0 < words <= 8192 or not isinstance(addresses, list) or
            not addresses or len(addresses) > words // 2):
        raise ValueError('invalid native graphics command/address metadata')
    for patch in addresses:
        if (not isinstance(patch, dict) or set(patch) != {'word', 'kind', 'offset', 'bytes'} or
                any(type(value) is not int or not 0 <= value <= 0xffffffff for value in patch.values()) or
                patch['word'] >= words - 1 or patch['kind'] >= 11 or not patch['bytes']):
            raise ValueError('invalid native graphics address patch')
    scalar = image.get('scalar_state')
    if not modes:
        if scalar is not None:
            raise ValueError('legacy raw image contains mode declarations')
        return
    if (not isinstance(scalar, dict) or scalar.get('snapshot_required') is not True or
            scalar.get('full_viewport_scissor_only') is not True or scalar.get('no_cull_depth_fragcoord') is not True or
            not isinstance(scalar.get('patches'), list) or len(scalar['patches']) != 4 or
            image.get('vertex_stride') != (32 if profile else 24)):
        raise ValueError('mode image requires all four explicit scalar declarations')
    bits = lambda value: int.from_bytes(struct.pack('<f', value), 'little')
    registers = [0x8013, 0x8014, 0x8015, 0x8000]
    baseline = [bits(32), 0, bits(1), 0xc0 if chip == 0x06050002 else 0xe0]
    values = [[bits(32), bits(-32)], [bits(.5), 0], [bits(.5), bits(1)], [0, 0x40]]
    seen, ranges = set(), []
    for patch in scalar['patches']:
        if (not isinstance(patch, dict) or set(patch) != {'word', 'reg', 'header', 'expected', 'mask', 'kind', 'values'} or
                any(type(patch.get(key)) is not int or not 0 <= patch[key] <= 0xffffffff
                    for key in ('word', 'reg', 'header', 'expected', 'mask', 'kind')) or
                not isinstance(patch.get('values'), list) or len(patch['values']) != 2 or
                any(type(value) is not int or not 0 <= value <= 0xffffffff for value in patch['values']) or
                patch['kind'] not in range(4) or patch['kind'] in seen or not 0 < patch['word'] < words):
            raise ValueError('invalid or duplicated scalar patch metadata')
        kind, word = patch['kind'], patch['word']
        reg = registers[kind]
        parity = lambda value: 1 ^ (value.bit_count() & 1)
        header = 0x40000001 | (parity(1) << 7) | (reg << 8) | (parity(reg) << 27)
        if (patch['reg'] != reg or patch['header'] != header or patch['expected'] != baseline[kind] or
                patch['mask'] != (0x40 if kind == 3 else 0xffffffff) or patch['values'] != values[kind] or
                any(word-1 <= other['word']+1 and other['word'] <= word for other in addresses) or
                any(word-1 <= end and start <= word for start, end in ranges)):
            raise ValueError('scalar patch register/mask/value or address alias mismatch')
        seen.add(kind)
        ranges.append((word-1, word))


def verify_check_sources(check):
    sources = check.get('check_sources')
    if sources is None and not check.get('modes', False):
        return  # Historical raw checks predate this source inventory.
    if not isinstance(sources, dict) or not sources:
        raise ValueError('missing graphics check source inventory')
    for name, expected in sources.items():
        path = Path(name)
        if not path.is_absolute() or sha(path) != expected:
            raise ValueError('graphics check source changed: ' + name)


def build(check_path, out):
    check = json.loads(check_path.read_text())
    verify_check_sources(check)
    for record_name in ('frontend_build', 'ir3_build'):
        for path, expected in check[record_name]['artifacts'].items():
            if sha(Path(path)) != expected:
                raise ValueError('compiler build artifact changed: ' + path)
    for name, expected in check['artifacts'].items():
        if Path(name).name != name or sha(check_path.parent / name) != expected:
            raise ValueError('graphics artifact changed: ' + name)
    for stage in ('vs', 'fs'):
        entry = check['inputs'][stage]
        if (entry['resources']['stage'] != stage or
                entry['resources']['code_sha256'] != entry['envelope']['inputs']['code']['sha256']):
            raise ValueError('original shader identity mismatch')
    device = check['device_evidence']['result']
    if (not device['passed'] or device['uid'] < 10000 or type(device['chip_id']) is not int or
            device['chip_id'] not in (0x43030b00, 0x06050002) or
            check['program']['chip_id'] != device['chip_id'] or
            type(device['gmem_bytes']) is not int or device['gmem_bytes'] <= 0 or device['gmem_bytes'] % 4096 or
            device['gmem_bytes'] != check['program']['device_cache']['gmem_bytes'] or
            not all(check['rejections'].get(key) for key in
                    ('missing_graphics_state', 'sph_stage', 'missing_vertex_producer', 'insufficient_gmem'))):
        raise ValueError('graphics qualification contract is incomplete')
    source = check_path.parent / 'pair.graphics.h'
    if source.name not in check['artifacts'] or 'pair.graphics.json' not in check['artifacts']:
        raise ValueError('no native graphics state artifact')
    image = json.loads((check_path.parent / 'pair.graphics.json').read_text())
    validate_image_metadata(image, chip=device['chip_id'], gmem=device['gmem_bytes'],
                            modes=check.get('modes', False), profile=0)
    if 'image' in check and image != check['image']:
        raise ValueError('graphics image differs from checked metadata')
    out.mkdir(parents=True, exist_ok=True)
    header = out / 'graphics_bundle.h'
    header.write_bytes(source.read_bytes())
    return header, check, {str(header): sha(header)}
