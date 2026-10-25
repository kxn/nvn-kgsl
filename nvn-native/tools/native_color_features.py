"""Explicit original-host color features and actual compiler/link provenance.

This prepares existing native capabilities; it confers no GPU/game qualification.
The CPU constant archive packs one texel, never a target image.
"""
import hashlib
import json
import shlex
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def prepare(root, kernel_check, constant_check, color_targets, target_library,
            graphics_check, block_check):
    if bool(kernel_check) != bool(constant_check):
        raise ValueError('native clear requires both kernel and constant checks')
    if color_targets and not all((target_library, graphics_check, block_check)):
        raise ValueError('color targets require graphics, runtime FDL and block copy')
    native = root / 'nvn-native'
    inputs = {}
    manifest = dict(schema=1, kind='original-host-native-color-features-v1',
                    clear=None, color_targets=None, gpu_qualification=False)
    flags = ['-DNVN_D2R_CLEAR_COLOR_HEADER=', '-DNVN_CLEAR_COLOR_PATTERN_LIBRARY=',
             '-DNVN_D2R_COLOR_TARGETS=' + ('ON' if color_targets else 'OFF')]
    if kernel_check:
        from compile_clear_color import verify
        kernel_check, constant_check = Path(kernel_check).resolve(), Path(constant_check).resolve()
        kernel = verify(root, kernel_check)
        constants = json.loads(constant_check.read_text())
        if (constants.get('schema') != 1 or not constants.get('passed') or
                constants.get('kind') != 'native-clear-color-constants-v1' or
                constants.get('gpu_execution') is not False):
            raise ValueError('normal native clear constant package required')
        library = Path(constants['static_library']).resolve()
        if constants['artifacts'].get(str(library)) != sha(library):
            raise ValueError('native clear constant archive is not a checked artifact')
        for rel in ('src/clear_color_pattern.c', 'include/nvn_native/clear_color.h',
                    'tools/check_clear_color.py'):
            p = native / rel
            if constants['inputs'].get(str(p)) != sha(p):
                raise ValueError('native clear constant source differs: ' + rel)
        for check, value in ((kernel_check, kernel), (constant_check, constants)):
            inputs.update({str(check): sha(check), **value['inputs'], **value['artifacts']})
        for rel in ('src/clear_color.c', 'src/clear_color_record.inc',
                    'tools/compile_clear_color.py'):
            p = native / rel
            inputs[str(p)] = sha(p)
        manifest['clear'] = dict(kernel_check=str(kernel_check), header=kernel['header'],
                                 constant_check=str(constant_check), library=str(library),
                                 library_sha256=sha(library), cpu_work='one texel only')
        flags[:2] = ['-DNVN_D2R_CLEAR_COLOR_HEADER=' + kernel['header'],
                     '-DNVN_CLEAR_COLOR_PATTERN_LIBRARY=' + str(library)]
    if color_targets:
        for rel in ('src/color_target.c', 'src/color_target_shadow.c',
                    'include/nvn_native/color_target.h', 'include/nvn_native/color_target_shadow.h'):
            p = native / rel
            inputs[str(p)] = sha(p)
        manifest['color_targets'] = dict(library=str(Path(target_library).resolve()),
                                         library_sha256=sha(target_library),
                                         canonical_storage_preserved=True)
    if not kernel_check and not color_targets:
        return flags, None, {}
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    for p, expected in inputs.items():
        if sha(p) != expected:
            raise ValueError('native color feature input changed: ' + p)
    manifest['inputs'] = inputs
    return flags, manifest, inputs


def audit(native, manifest, commands):
    rows = []
    links = []
    for line in commands.splitlines():
        parts = shlex.split(line)
        if '-c' in parts:
            rows.append((Path(parts[parts.index('-c') + 1]).resolve(), parts))
        else:
            links.append(parts)
    clear = manifest.get('clear') if manifest else None
    targets = manifest.get('color_targets') if manifest else None
    for enabled, names, macros in (
            (clear, ('clear_color.c',), ('NN_NVN_CLEAR_COLOR',)),
            (targets, ('color_target.c', 'color_target_shadow.c'),
             ('NN_NVN_COLOR_TARGETS', 'NN_PIPELINE_COLOR_TARGETS'))):
        for name in names:
            selected = [args for p, args in rows if p == (native / 'src' / name).resolve()]
            if len(selected) != (1 if enabled else 0):
                raise ValueError('native color module missing, duplicated or unrequested: ' + name)
        for macro in macros:
            actual = [args for p, args in rows if p == (native / 'src/nvn.c').resolve()]
            if len(actual) != 1 or (('-D' + macro + '=1') in actual[0]) != bool(enabled):
                raise ValueError('native color compile mode differs: ' + macro)
    if clear:
        bridge = [args for p, args in rows if p == (native / 'adapters/d2r/host_bridge.c').resolve()]
        define = '-DNN_NVN_CLEAR_COLOR_HEADER="' + clear['header'] + '"'
        if len(bridge) != 1 or define not in bridge[0]:
            raise ValueError('native clear kernel absent from actual Device bridge')
    for value in (clear, targets):
        if value and not any(value['library'] in line for line in links):
            raise ValueError('native color archive absent from actual link: ' + value['library'])
    if manifest:
        for p, expected in manifest['inputs'].items():
            if sha(p) != expected:
                raise ValueError('native color source/product changed: ' + p)
