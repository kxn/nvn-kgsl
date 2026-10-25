"""Map checked original handle sources into separate NVN API binding domains.

Pool IDs are runtime values. CB words, API slots and native bindings stay
distinct. The current sampled profile supports one resolved source pair only.
"""
import importlib.util
import json
from pathlib import Path


def module(name):
    spec = importlib.util.spec_from_file_location('nn_' + name, Path(__file__).with_name(name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def source_slot(source, base, limit):
    if (source.get('cbuf_slot') != 2 or type(source.get('word_offset')) is not int or
            source['word_offset'] < 0):
        raise ValueError('unqualified texture driver cbuf source')
    relative = source['word_offset'] * 4 - base
    if relative < 0 or relative % 8 or relative // 8 >= limit:
        raise ValueError('unqualified separate handle source domain or alignment')
    return relative // 8


def build(check_path, out):
    check = json.loads(check_path.read_text())
    fs, vs = check['inputs']['fs']['resources'], check['inputs']['vs']['resources']
    if vs['textures'] or vs['samplers'] or len(fs['textures']) != 1 or len(fs['samplers']) != 1:
        raise ValueError('only one resolved fragment sampled resource is qualified')
    texture, sampler = fs['textures'][0], fs['samplers'][0]
    sources = texture.get('handle_sources', {})
    if (sources.get('kind') != 'SeparateSamplerHandle' or sources.get('resolved') is not True or
            texture['Set'] != 0 or sampler['Set'] != 0 or texture['Binding'] != 1 or
            sampler['Binding'] != 1 or texture['ArrayLength'] != 1 or sampler['ArrayLength'] != 1 or
            texture['CbufSlot'] != sampler['CbufSlot'] or texture['HandleIndex'] != sampler['HandleIndex']):
        raise ValueError('unqualified sampled resource reflection')
    t, s = sources['texture'], sources['sampler']
    mapping = {'stage': 1, 'native_texture_index': texture['Binding'], 'native_sampler_index': sampler['Binding'],
               'texture_domain': 1, 'texture_slot': source_slot(t, 0x168, 128),
               'sampler_domain': 1, 'sampler_slot': source_slot(s, 0x568, 128),
               'texture_cbuf': t['cbuf_slot'], 'texture_word': t['word_offset'],
               'sampler_cbuf': s['cbuf_slot'], 'sampler_word': s['word_offset']}
    if mapping['texture_slot'] >= 32 or mapping['sampler_slot'] >= 32:
        raise ValueError('sampled source exceeds the current qualified native API selector range')
    # No numeric pool ID appears here. It must be selected by the recorded API
    # handle and pool identity at the backend's qualified consumption epoch.
    return module('graphics_api_bundle').build(check_path, out,
        image_builder=module('texture_bundle').build, sampled=[mapping])
