"""Package a checked original sampled-pair artifact; no game texture data."""
import hashlib
import importlib.util
import json
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(check_path, out):
    check = json.loads(check_path.read_text())
    spec = importlib.util.spec_from_file_location('nn_graphics_image_contract', Path(__file__).with_name('graphics_bundle.py'))
    contract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(contract)
    contract.verify_check_sources(check)
    if not check['passed']:
        raise ValueError('sampled pair compilation did not pass')
    for key in ('frontend_build', 'ir3_build'):
        for filename, expected in check[key]['artifacts'].items():
            if sha(Path(filename)) != expected:
                raise ValueError('compiler build changed: ' + filename)
    for name, expected in check['artifacts'].items():
        path = (check_path.parent / name).resolve()
        if not path.is_relative_to(check_path.parent.resolve()) or sha(path) != expected:
            raise ValueError('sampled compiler artifact changed: ' + name)
    for stage in ('vs', 'fs'):
        entry = check['inputs'][stage]
        if (entry['resources']['stage'] != stage or
                entry['resources']['code_sha256'] != entry['envelope']['inputs']['code']['sha256']):
            raise ValueError('original shader identity mismatch')
    device = check['device_evidence']['result']
    if (not device['passed'] or device['uid'] < 10000 or type(device['chip_id']) is not int or
            device['chip_id'] not in (0x43030b00, 0x06050002) or
            check['program']['chip_id'] != device['chip_id'] or
            check['sparse_binding_check']['program']['chip_id'] != device['chip_id'] or
            type(device['gmem_bytes']) is not int or device['gmem_bytes'] <= 0 or device['gmem_bytes'] % 4096 or
            check['program']['device_cache']['gmem_bytes'] != device['gmem_bytes'] or
            check['resource_tables']['texture_slots'] != 2 or check['resource_tables']['sampler_slots'] != 2 or
            check['program']['fs']['num_samplers'] != 2 or
            not all(check['rejections'].get(key) for key in
                    ('missing_coordinate_query', 'missing_host_query', 'texture_descriptor_set',
                     'texture_binding_limit', 'texture_descriptor_array', 'insufficient_gmem'))):
        raise ValueError('sampled native qualification contract is incomplete')
    for name in ('pair.graphics.h', 'pair.graphics.json'):
        if name not in check['artifacts']:
            raise ValueError('no native sampled image')
    image = json.loads((check_path.parent / 'pair.graphics.json').read_text())
    contract.validate_image_metadata(image, chip=device['chip_id'], gmem=device['gmem_bytes'],
                                     modes=check.get('modes', False), profile=1)
    if 'image' in check and image != check['image']:
        raise ValueError('sampled image differs from checked metadata')
    out.mkdir(parents=True, exist_ok=True)
    header = out / 'texture_bundle.h'
    header.write_bytes((check_path.parent / 'pair.graphics.h').read_bytes())
    return header, check, {str(header): sha(header)}
