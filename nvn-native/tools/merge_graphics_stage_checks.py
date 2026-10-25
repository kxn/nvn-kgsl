#!/usr/bin/env python3
"""Union checked normal graphics stages/pairs; no translation or new pair generation."""
import argparse
import copy
import hashlib
import importlib.util
import json
import re
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)


def read_json(path):
    def unique(pairs):
        out = {}
        for key, value in pairs:
            if key in out:
                raise ValueError('duplicate JSON key: ' + key)
            out[key] = value
        return out
    return json.loads(Path(path).read_text(), object_pairs_hook=unique,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError('nonfinite JSON')))


def checked_mapping(mapping, label):
    if not isinstance(mapping, dict) or not mapping:
        raise ValueError('missing ' + label)
    for path, value in mapping.items():
        if not isinstance(path, str) or not Path(path).is_absolute() or not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value):
            raise ValueError('invalid ' + label + ' identity')
        if sha(path) != value:
            raise ValueError('changed ' + label + ': ' + path)


def union_mapping(out, incoming, label):
    for path, value in incoming.items():
        if path in out and out[path] != value:
            raise ValueError('conflicting ' + label + ' hash: ' + path)
        out[path] = value


def require_dependency(deps, path, value, label):
    path = str(Path(path).resolve())
    if deps.get(path) != value or sha(path) != value:
        raise ValueError('missing or changed normal ' + label + ': ' + path)


def extend_dependency(deps, path, value, label):
    # Original compiler records keep fixture hashes in inventory/stage entries,
    # not necessarily in inputs. Validate that original contract, then add the
    # full closure to this new record instead of requiring nonexistent fields.
    path = str(Path(path).resolve())
    if sha(path) != value or (path in deps and deps[path] != value):
        raise ValueError('changed ' + label + ': ' + path)
    deps[path] = value


def validate_check(root, path):
    c = read_json(path)
    if type(c.get('schema')) is not int or c['schema'] != 1 or c.get('kind') != 'offline-graphics-stage-compilation-v1' or c.get('gpu_execution') is not False or c.get('draw_pipeline_available') is not False:
        raise ValueError('unknown normal stage check profile')
    if type(c.get('chip_id')) is not int or c['chip_id'] not in (0x06050002, 0x43030b00):
        raise ValueError('unknown chip identity')
    policy = c.get('native_backend_policy')
    if not isinstance(policy, dict) or set(policy) != {'unit_render_scale'} or type(policy['unit_render_scale']) is not bool:
        raise ValueError('unknown native backend policy')
    if type(c.get('compiler_source_files')) is not int or c['compiler_source_files'] <= 0:
        raise ValueError('invalid normal compiler source count')
    checked_mapping(c.get('inputs'), 'input')
    checked_mapping(c.get('artifacts'), 'artifact')
    deps = c['inputs']
    native = root / 'nvn-native'
    for name in ('offline_graphics_stages.py', 'shader_fixture.py'):
        f = native / 'tools' / name
        require_dependency(deps, f, sha(f), 'stage tool')
    # Use the existing normal common-compiler source verifier, including actual
    # Mesa/source/overlay/generated tool identities. Do not invent a lighter one.
    own = load_module(native / 'tools/compute_driverparams.py', 'merge_stage_compiler')
    records = []
    for p in deps:
        if p.endswith('/build-record.json') and read_json(p) == c['ir3_build']:
            records.append(Path(p))
    if len(records) != 1:
        raise ValueError('ambiguous normal IR3 build record')
    ir, compiler_inputs, _, count = own.compiler_inputs(root, records[0].parent)
    if ir != c['ir3_build'] or count != c['compiler_source_files']:
        raise ValueError('normal compiler identity mismatch')
    for p, value in compiler_inputs.items():
        require_dependency(deps, p, value, 'compiler input')
    front = c['frontend_build']
    front_records = [p for p in deps if p.endswith('/build-record.json') and read_json(p) == front]
    if len(front_records) != 1:
        raise ValueError('ambiguous normal frontend build record')
    for field, base in (('sources', root), ('artifacts', None), ('restore_assets', None)):
        for p, value in front[field].items():
            require_dependency(deps, base / p if base else p, value, 'frontend ' + field)
    for p, value in front['compiler_overlay'].items():
        if p != 'original_sha256':
            require_dependency(deps, p, value, 'frontend overlay')
    require_dependency(deps, front['commands'][0][0], front['dotnet_sha256'], 'dotnet tool')
    for name in ('spirv-val', 'spirv-dis'):
        if not any(Path(p).name == name for p in deps):
            raise ValueError('missing normal SPIR-V tool ' + name)
    inv = c['inventory']
    if type(inv.get('schema')) is not int or inv['schema'] != 1 or inv.get('kind') != 'declared-graphics-container-inventory-v1' or not isinstance(inv.get('entries'), list):
        raise ValueError('unknown source inventory')
    checked_mapping(inv.get('source_inputs'), 'inventory input')
    for p, value in inv['source_inputs'].items():
        require_dependency(deps, p, value, 'inventory input')
    fixture = load_module(native / 'tools/shader_fixture.py', 'merge_fixture')
    stages = c['stages']
    if not isinstance(stages, dict) or not stages or not isinstance(c['pairs'], list) or not isinstance(c['commands'], list):
        raise ValueError('invalid stage/pair/command collection')
    inventory_keys = set()
    for entry in inv['entries']:
        f = Path(entry['fixture'])
        extend_dependency(deps, f, entry['fixture_sha256'], 'inventory fixture')
        d = read_json(f)
        env = fixture.validate(d, f.parent)
        inventory_keys.add(env['request_sha256'])
        if env['request_sha256'] not in stages:
            raise ValueError('inventory stage missing from check')
        origin = stages[env['request_sha256']]
        if entry['stage'] != d['stage'] or entry['container_offset'] not in origin['containers'] or entry.get('source_id', str(entry['container_offset'])) not in origin['source_ids']:
            raise ValueError('inventory/container origin differs from stage')
    if inventory_keys != set(stages):
        raise ValueError('stage absent from source inventory')
    for key, entry in stages.items():
        if not re.fullmatch('[0-9a-f]{64}', key) or type(entry.get('frontend_passed')) is not bool or entry.get('stage') not in ('vs', 'fs'):
            raise ValueError('invalid stage key/status')
        f = Path(entry['fixture'])
        extend_dependency(deps, f, entry['fixture_sha256'], 'stage fixture')
        d = read_json(f)
        env = fixture.validate(d, f.parent)
        if env != entry['envelope'] or env['request_sha256'] != key or d['stage'] != entry['stage']:
            raise ValueError('stage fixture/request identity mismatch')
        for blob in d['blobs'].values():
            source = (f.parent / blob['path']).resolve()
            extend_dependency(deps, source, sha(source), 'complete source/control/C1 file')
        if not isinstance(entry.get('containers'), list) or not entry['containers'] or not isinstance(entry.get('source_ids'), list) or not entry['source_ids']:
            raise ValueError('stage source provenance missing')
        if any(type(v) is not int or v < 0 for v in entry['containers']) or any(not isinstance(v, str) or not v for v in entry['source_ids']):
            raise ValueError('invalid stage source provenance')
        if entry['frontend_passed']:
            for suffix in ('.spv', '.resources.json'):
                p = str(Path(entry['prefix'] + suffix).resolve())
                if p not in c['artifacts']:
                    raise ValueError('stage artifact not recorded: ' + p)
            if read_json(entry['prefix'] + '.resources.json') != entry['resources']:
                raise ValueError('stage reflection differs from actual artifact')
    seen = set()
    for pair in c['pairs']:
        key = (pair['vs'], pair['fs'])
        if key in seen or key[0] not in stages or key[1] not in stages or stages[key[0]]['stage'] != 'vs' or stages[key[1]]['stage'] != 'fs' or not all(stages[k]['frontend_passed'] for k in key) or type(pair.get('passed')) is not bool:
            raise ValueError('invalid or duplicate pair key/status')
        seen.add(key)
        if pair['passed']:
            for suffix in ('.link.json', '.vs.bin', '.fs.bin'):
                p = str(Path(pair['prefix'] + suffix).resolve())
                if p not in c['artifacts']:
                    raise ValueError('native pair artifact not recorded: ' + p)
            if read_json(pair['prefix'] + '.link.json') != pair['program']:
                raise ValueError('pair metadata differs from actual artifact')
    return c


def merge_documents(paths, docs):
    # Only semantic identity is deduplicated; every original path/container and
    # command remains in provenance. Native code hashes are separately compared.
    first = docs[0]
    for d in docs[1:]:
        for key in ('schema', 'kind', 'chip_id', 'gpu_execution', 'draw_pipeline_available', 'native_backend_policy', 'frontend_build', 'ir3_build', 'compiler_source_files'):
            if encoded(d[key]) != encoded(first[key]):
                raise ValueError('incompatible normal identity: ' + key)
    out = copy.deepcopy(first)
    out.update(inputs={}, artifacts={}, stages={}, pairs=[], commands=[], pair_selection=[])
    inv = {'schema': 1, 'kind': 'declared-graphics-container-inventory-v1', 'source_inputs': {}, 'entries': [],
           'scope': 'union of already-checked source inventories; no new shader/state/pair generation'}
    provenance = {'schema': 1, 'policy': 'union-existing-checked-pairs-only', 'checks': [], 'stage_origins': {}, 'pair_origins': {}}
    stage_facts = {}; pair_facts = {}; pair_rows = {}
    for path, d in zip(paths, docs):
        origin = str(path)
        provenance['checks'].append({'path': origin, 'sha256': sha(path), 'inventory': d['inventory'], 'pair_selection': d['pair_selection']})
        union_mapping(out['inputs'], d['inputs'], 'input')
        union_mapping(out['artifacts'], d['artifacts'], 'artifact')
        union_mapping(inv['source_inputs'], d['inventory']['source_inputs'], 'inventory input')
        out['inputs'][origin] = sha(path)
        for e in d['inventory']['entries']:
            if e not in inv['entries']:
                inv['entries'].append(copy.deepcopy(e))
        out['commands'].extend(copy.deepcopy(d['commands']))
        for key, entry in d['stages'].items():
            semantic = {k: v for k, v in entry.items() if k not in ('fixture', 'fixture_sha256', 'prefix', 'containers', 'source_ids')}
            if entry['frontend_passed']:
                semantic['spv_sha256'] = d['artifacts'][str(Path(entry['prefix'] + '.spv').resolve())]
            facts = encoded(semantic)
            if key in stage_facts and stage_facts[key] != facts:
                raise ValueError('stage metadata conflict: ' + key)
            stage_facts[key] = facts
            provenance['stage_origins'].setdefault(key, []).append({'check': origin, 'entry': entry})
            if key not in out['stages']:
                out['stages'][key] = copy.deepcopy(entry)
            else:
                for field in ('containers', 'source_ids'):
                    for value in entry[field]:
                        if value not in out['stages'][key][field]:
                            out['stages'][key][field].append(value)
        for pair in d['pairs']:
            key = (pair['vs'], pair['fs'])
            semantic = {k: v for k, v in pair.items() if k != 'prefix'}
            if pair['passed']:
                semantic['native_code_sha256'] = {stage: d['artifacts'][str(Path(pair['prefix'] + '.' + stage + '.bin').resolve())] for stage in ('vs', 'fs')}
            facts = encoded(semantic)
            if key in pair_facts and pair_facts[key] != facts:
                raise ValueError('pair native code/metadata conflict: ' + '/'.join(key))
            pair_facts[key] = facts
            provenance['pair_origins'].setdefault('/'.join(key), []).append({'check': origin, 'entry': pair})
            if key not in pair_rows:
                pair_rows[key] = copy.deepcopy(pair)
                out['pairs'].append(pair_rows[key])
                # Existing compile contract uses source IDs here. Preserve that
                # domain and enumerate ONLY pairs actually present in an input.
                out['pair_selection'].append([d['stages'][key[0]]['source_ids'][0], d['stages'][key[1]]['source_ids'][0]])
    out['inventory'] = inv
    out['merge_provenance'] = provenance
    return out


def build(root, checks, out, verify=False):
    root = Path(root).resolve(); out = Path(out).resolve()
    checks = [Path(p).resolve() for p in checks]
    if len(checks) < 2 or len(set(checks)) != len(checks):
        raise ValueError('at least two distinct checked-stage inputs required')
    if not out.is_relative_to(root / '.local') or out == root / '.local':
        raise ValueError('dedicated ignored output required')
    if out.exists() != verify:
        raise ValueError('fresh output required, or explicit --verify existing output')
    docs = [validate_check(root, p) for p in checks]
    merged = merge_documents(checks, docs)
    own = Path(__file__).resolve()
    merged['inputs'][str(own)] = sha(own)
    packager = root / 'nvn-native/tools/package_graphics_stages.py'
    merged['inputs'][str(packager)] = sha(packager)
    package = load_module(packager, 'merge_normal_stage_archive')
    if not verify:
        out.mkdir(parents=True)
    archives = []
    for i, path in enumerate(checks):
        header, result, artifacts = package.build(path, out / 'input-archives' / str(i), root=root, verify=verify)
        union_mapping(merged['artifacts'], artifacts, 'validation archive')
        archives.append({'header': str(header), 'stages': len(result['stages']), 'pairs': result['pair_count']})
    payload = (json.dumps(merged, indent=2, allow_nan=False) + '\n').encode()
    check = out / 'check.json'
    if verify:
        if check.read_bytes() != payload:
            raise ValueError('merged checked-stage record changed')
    else:
        check.write_bytes(payload)
    header, archive, products = package.build(check, out / 'stage-archive', root=root, verify=verify)
    # Recheck every dependency after materializing all outputs. No stale source
    # can be accepted merely because it matched at the beginning of this call.
    checked_mapping(merged['inputs'], 'merged input')
    checked_mapping(merged['artifacts'], 'merged artifact')
    result = {'schema': 1, 'kind': 'checked-graphics-stage-union-v1', 'passed': True,
              'gpu_execution': False, 'runtime_compiler': False, 'created_pairs': 0,
              'inventory_entries': len(merged['inventory']['entries']), 'unique_stages': len(merged['stages']),
              'attempted_pairs': len(merged['pairs']), 'passed_pairs': sum(p['passed'] for p in merged['pairs']),
              'packaged_stages': len(archive['stages']), 'packaged_pairs': archive['pair_count'],
              'sources': archives, 'inputs': {**merged['inputs'], str(check): sha(check)},
              'artifacts': {**merged['artifacts'], **products}}
    record = (json.dumps(result, indent=2) + '\n').encode(); manifest = out / 'merge-manifest.json'
    if verify:
        if manifest.read_bytes() != record:
            raise ValueError('merge manifest changed')
    else:
        manifest.write_bytes(record)
    return check, result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--check', type=Path, action='append', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    print(build(args.root, args.check, args.out, args.verify)[0])


if __name__ == '__main__':
    main()
