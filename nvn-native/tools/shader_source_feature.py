"""Explicit complete-source identity provider; never a native shader compiler."""
import hashlib
import json
import shlex
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def validate_options(check, graphics):
    if check is not None and (not isinstance(check, (str, Path)) or not str(check)):
        raise ValueError('--shader-source-check must be an explicit normal record path')
    if check is not None and not graphics:
        raise ValueError('--shader-source-check requires --graphics-pipeline-check for real Draw capabilities')


def prepare(root, check, graphics):
    validate_options(check, graphics)
    flags = ['-DNVN_D2R_SHADER_SOURCE_HEADER=', '-DNVN_D2R_SHADER_SOURCE_C=',
             '-DNVN_D2R_SHADER_SOURCE_ASM=', '-DNVN_D2R_SHADER_SOURCE_BLOB=']
    if check is None:
        return flags, None, {}
    native = Path(__file__).resolve().parents[1]
    check = Path(check).resolve()
    d = json.loads(check.read_text())
    if (type(d.get('schema')) is not int or d['schema'] != 1 or
            d.get('kind') != 'certified-original-graphics-source-catalogue-v1' or
            d.get('passed') is not True or d.get('gpu_execution') is not False or
            d.get('shader_compilation') is not False or d.get('query_expectations') is not False or
            type(d.get('native_variants')) is not int or d['native_variants'] != 0 or
            not isinstance(d.get('entries'), list) or not 2 <= len(d['entries']) <= 65536):
        raise ValueError('normal certified complete original source-only catalogue required')
    inputs = {str(check): sha(check)}
    for key in ('inputs', 'artifacts'):
        if not isinstance(d.get(key), dict) or not d[key]:
            raise ValueError('missing complete source ' + key)
        for path, expected in d[key].items():
            if not Path(path).is_absolute() or sha(path) != expected:
                raise ValueError('complete source input/product changed: ' + path)
            inputs[path] = expected
    for rel in ('tools/package_shader_sources.py', 'include/nvn_native/shader_source.h'):
        current = native / rel
        # An isolated normal producer may live at a different absolute source
        # root; source bytes must be exactly the current implementation.
        if not any(p.endswith('/nvn-native/' + rel) and h == sha(current)
                   for p, h in d['inputs'].items()):
            raise ValueError('complete source producer/header is not current: ' + rel)
    products = [Path(d[k]).resolve() for k in ('header', 'source', 'assembly', 'blob')]
    for product, name in zip(products, ('shader_sources.h', 'shader_sources.c', 'shader_sources.S', 'shader_sources.bin')):
        if product.parent != check.parent or product.name != name or d['artifacts'].get(str(product)) != sha(product):
            raise ValueError('unrecorded or foreign source product: ' + str(product))
    if type(d.get('blob_bytes')) is not int or products[3].stat().st_size != d['blob_bytes']:
        raise ValueError('complete source blob length mismatch')
    for i, product in enumerate(products):
        flags[i] += str(product)
    for rel in ('src/shader_source.c', 'src/nvn_shader_sources.inc', 'src/pipeline_source.c',
                'include/nvn_native/pipeline_source.h', 'include/nvn_native/pipeline_nvn.h'):
        inputs[str(native / rel)] = sha(native / rel)
    inputs[str(Path(__file__).resolve())] = sha(__file__)
    return flags, dict(schema=1, kind='original-host-complete-source-binding-v1',
        check=str(check), header=str(products[0]), source=str(products[1]), assembly=str(products[2]),
        blob=str(products[3]), entries=len(d['entries']), blob_bytes=d['blob_bytes'],
        stage_counts=d['stage_counts'], default_enabled=False, shader_compilation=False,
        native_variant_selection='actual immutable enqueue state and independent compiled catalogue',
        complete_source_match=True, selected_program_deep_copy=True,
        provider_storage='immutable read-only blob; Device copies index only',
        unsupported_draw='ENOTSUP before GPU allocations; never empty Draw', inputs=inputs), inputs


def audit(native, manifest, commands):
    native = Path(native).resolve()
    rows, links = [], []
    for line in commands.splitlines():
        args = shlex.split(line)
        if '-c' in args:
            rows.append((Path(args[args.index('-c') + 1]).resolve(), args))
        else:
            links.append(args)
    enabled = manifest is not None
    expected = [native / 'src/shader_source.c']
    if enabled:
        expected += [Path(manifest['source']), Path(manifest['assembly'])]
    for path in expected:
        if sum(p == path for p, _ in rows) != int(enabled):
            raise ValueError('complete source missing, duplicate or unrequested: ' + str(path))
    for name in ('nvn.c', 'pipeline_source.c'):
        selected = [a for p, a in rows if p == native / 'src' / name]
        if len(selected) != 1:
            raise ValueError('source consumer compile missing or duplicate: ' + name)
        for macro in ('NN_NVN_SHADER_SOURCE_CATALOGUE', 'NN_PIPELINE_SOURCE_CATALOGUE'):
            prefix = '-D' + macro
            actual = [x for x in selected[0] if x == prefix or x.startswith(prefix + '=')]
            if actual != ([prefix + '=1'] if enabled else []):
                raise ValueError('complete source compile mode differs: ' + name + ' ' + macro)
    bridges = [a for p, a in rows if p == native / 'adapters/d2r/host_bridge.c']
    if len(bridges) != 1:
        raise ValueError('source host bridge missing or duplicate')
    prefix = '-DNN_NVN_COMPLETE_SHADER_SOURCE_HEADER'
    actual = [x for x in bridges[0] if x.startswith(prefix)]
    if len(actual) != int(enabled) or (enabled and manifest['header'] not in actual[0]):
        raise ValueError('source host header compile mode differs')
    if enabled:
        if not any(manifest['assembly'] in str(p) for p, _ in rows):
            raise ValueError('source original blob object absent')
        # Parent builders also pin all generated products, including the incbin
        # blob. CMake OBJECT_DEPENDS makes Ninja rebuild on blob mutation.
        for path, expected_hash in manifest['inputs'].items():
            if sha(path) != expected_hash:
                raise ValueError('source feature input changed after build: ' + path)
