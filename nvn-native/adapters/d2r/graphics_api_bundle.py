"""Package verified raw graphics inputs and cache metadata for an NVN caller.

Original game bytes and translated code are emitted only into ignored build
outputs. No original-input identities or test-specific math enter the driver.
"""
import hashlib
import importlib.util
import json
from pathlib import Path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(check_path, out, *, image_builder=None, sampled=None):
    if image_builder is None:
        spec = importlib.util.spec_from_file_location('nn_graphics_bundle', Path(__file__).with_name('graphics_bundle.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        image_builder = module.build
    image_header, check, hashes = image_builder(check_path, out)
    lines = ['/* Generated D2R-only NVN qualification inputs; not driver logic. */',
             '#include "nvn_native/nvn.h"', f'#include "{image_header.name}"']
    initializers = {}
    for stage in ('vs', 'fs'):
        entry = check['inputs'][stage]
        if json.loads((check_path.parent / (stage + '.resources.json')).read_text()) != entry['resources']:
            raise ValueError('graphics reflection differs from verified compiler artifact')
        fixture = Path(entry['fixture'])
        if sha(fixture.read_bytes()) != entry['fixture_sha256']:
            raise ValueError('original graphics fixture changed')
        document = json.loads(fixture.read_text())
        sizes = {}
        for name in ('code', 'control', 'c1'):
            blob = document['blobs'][name]
            with (fixture.parent / blob['path']).open('rb') as stream:
                stream.seek(blob['offset'])
                data = stream.read(blob['size'])
            expected = entry['envelope']['inputs'][name]
            if len(data) != expected['size'] or sha(data) != expected['sha256'] or sha(data) != blob['sha256']:
                raise ValueError('original graphics input identity changed')
            if name == 'c1':
                if data:
                    raise ValueError('nonempty compile-time C1 is not qualified')
                continue
            sizes[name] = len(data)
            lines.append(f'static const unsigned char nn_{stage}_{name}[] = {{' + ','.join(map(str, data)) + '};')
        cbufs = entry['resources']['cbufs']
        if cbufs:
            lines.append(f'static const struct nn_nvn_cbuf nn_{stage}_cbufs[] = {{' +
                         ','.join(f"{{{b['Slot']},{b['Binding']}}}" for b in cbufs) + '};')
        initializers[stage] = (f'{{nn_{stage}_code,nn_{stage}_control,{sizes["code"]}u,{sizes["control"]}u,' +
                               (f'nn_{stage}_cbufs' if cbufs else 'NULL') + f',{len(cbufs)}u}}')
    extra = ''
    if sampled:
        fields = ('stage', 'native_texture_index', 'native_sampler_index', 'texture_domain', 'texture_slot',
                  'sampler_domain', 'sampler_slot', 'texture_cbuf', 'texture_word', 'sampler_cbuf', 'sampler_word')
        if any(set(entry) != set(fields) or any(type(value) is not int or value < 0 or value > 0xffffffff
                                               for value in entry.values()) for entry in sampled):
            raise ValueError('invalid typed sampled mapping')
        lines.append('static const struct nn_nvn_sampled nn_graphics_sampled[] = {' +
                     ','.join('{' + ','.join(f'.{key}={entry[key]}u' for key in fields) + '}' for entry in sampled) + '};')
        extra = f',.sampled=nn_graphics_sampled,.sampled_count={len(sampled)}u'
    lines.append('static const struct nn_nvn_compiled_graphics nn_test_graphics = {' +
                 '.image=&NVN_GRAPHICS_IMAGE,.vs=' + initializers['vs'] + ',.fs=' + initializers['fs'] + extra + '};')
    bundle = out / 'graphics_api_bundle.h'
    bundle.write_text('\n'.join(lines) + '\n')
    hashes[str(bundle)] = sha(bundle.read_bytes())
    return bundle, check, hashes
