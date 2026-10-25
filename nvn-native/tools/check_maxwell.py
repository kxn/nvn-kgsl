#!/usr/bin/env python3
"""Qualify explicit Maxwell CS fixtures through the independent host compiler.

Game bytes/derived code stay in ignored local outputs. Evidence records hashes,
queries and resource metadata; host compilation is not GPU semantic validation.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile

from shader_fixture import validate, digest


def verify_build(path):
    record = json.loads(path.read_text())
    for name, expected in record['artifacts'].items():
        if digest(Path(name).read_bytes()) != expected:
            raise ValueError('build artifact mismatch: ' + name)
    return record


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dotnet', type=Path, required=True)
    p.add_argument('--frontend-build', type=Path, required=True)
    p.add_argument('--ir3-build', type=Path, required=True)
    p.add_argument('--spirv-val', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--chip-id', type=lambda s: int(s, 0), default=0x43030b00,
                   choices=(0x43030b00, 0x06050002))
    p.add_argument('fixtures', type=Path, nargs='+')
    args = p.parse_args()
    spirv_tools = {name: args.spirv_val.parent / name for name in ('spirv-val', 'spirv-as', 'spirv-dis')}
    frontend = verify_build(args.frontend_build / 'build-record.json')
    ir3 = verify_build(args.ir3_build / 'build-record.json')
    if digest(args.dotnet.read_bytes()) != frontend['dotnet_sha256']:
        p.error('dotnet executable hash mismatch')
    compiler = args.ir3_build.resolve() / 'build/src/freedreno/fdl/nvn-spirv-ir3'
    args.out.mkdir(parents=True, exist_ok=True)
    results, commands = [], []
    env = os.environ.copy()
    env['IR3_SHADER_DEBUG'] = 'asmroundtrip'

    def run(argv, *, reject=False):
        argv = list(map(str, argv))
        commands.append(argv)
        result = subprocess.run(argv, capture_output=True, text=True, env=env)
        if (result.returncode != 0) != reject:
            raise ValueError(f'unexpected return {result.returncode}: {argv}\n{result.stderr}')
        return result.stderr

    for fixture in args.fixtures:
        fixture = fixture.resolve()
        document = json.loads(fixture.read_text())
        checked = validate(document, fixture.parent)
        prefix = args.out.resolve() / checked['request_sha256'][:16]
        run([args.dotnet, frontend['entrypoint'], fixture, prefix])
        run([args.spirv_val, str(prefix) + '.spv'])
        run([compiler, str(prefix) + '.spv', prefix, hex(args.chip_id)])
        resources = json.loads(Path(str(prefix) + '.resources.json').read_text())
        program = json.loads(Path(str(prefix) + '.json').read_text())
        if resources['code_sha256'] != checked['inputs']['code']['sha256']:
            raise ValueError('frontend input identity mismatch')
        if resources['spv_sha256'] != digest(Path(str(prefix) + '.spv').read_bytes()):
            raise ValueError('frontend output identity mismatch')
        if program['local_size'] != document['state']['compute']['local_size']:
            raise ValueError('workgroup size changed')
        if program['binary_bytes'] != Path(str(prefix) + '.bin').stat().st_size:
            raise ValueError('machine code size mismatch')
        # Delete a query that this exact input used, rather than assume that a
        # missing unused field should prevent translation.
        query = next(q['name'] for q in resources['queries'] if q['name'] in document['state']['host'])
        rejections = {}
        with tempfile.TemporaryDirectory(dir=fixture.parent) as temporary:
            for name in ('missing_query', 'hash_mismatch', 'code_bounds', 'truncated_code', 'precision'):
                bad = copy.deepcopy(document)
                # Preserve original absolute input paths for the temporary fixture.
                for blob in bad['blobs'].values():
                    blob['path'] = str(fixture.parent / blob['path'])
                if name == 'missing_query':
                    del bad['state']['host'][query]
                elif name == 'hash_mismatch':
                    bad['blobs']['code']['sha256'] = '0' * 64
                elif name == 'code_bounds':
                    bad['blobs']['code']['size'] = 16 * 1024 * 1024 + 1
                elif name == 'truncated_code':
                    blob = bad['blobs']['code']
                    with Path(blob['path']).open('rb') as stream:
                        stream.seek(blob['offset'])
                        data = stream.read(8)
                    short = Path(temporary) / 'short.code'
                    short.write_bytes(data)
                    blob.update(path=str(short), offset=0, size=len(data), sha256=digest(data))
                else:
                    bad['state']['precision']['mode'] = 'unqualified'
                bad_path = Path(temporary) / 'bad.json'
                bad_path.write_text(json.dumps(bad))
                rejections[name] = run([args.dotnet, frontend['entrypoint'], bad_path,
                                        Path(temporary) / 'rejected'], reject=True).strip()
            if not results:
                assembly = Path(temporary) / 'layout.spvasm'
                modified = Path(temporary) / 'layout.spv'
                run([spirv_tools['spirv-dis'], str(prefix) + '.spv', '-o', assembly])
                text = assembly.read_text()
                if 'DescriptorSet 0' not in text:
                    raise ValueError('expected set-zero buffers for the layout rejection check')
                assembly.write_text(text.replace('DescriptorSet 0', 'DescriptorSet 1'))
                run([spirv_tools['spirv-as'], '--target-env', 'spv1.3', assembly, '-o', modified])
                run([args.spirv_val, modified])
                diagnostic = run([compiler, modified, Path(temporary) / 'layout', hex(args.chip_id)], reject=True).strip()
                if 'unqualified SPIR-V profile: buffer declaration layout' not in diagnostic:
                    raise ValueError('unexpected descriptor-layout rejection: ' + diagnostic)
                rejections['descriptor_set'] = diagnostic
        results.append({'fixture': str(fixture), 'fixture_sha256': digest(fixture.read_bytes()),
                        'provenance': document['provenance'], 'fixture_check': checked, 'resources': resources,
                        'program': program, 'rejections': rejections,
                        'artifacts': {suffix: digest(Path(str(prefix) + suffix).read_bytes())
                                      for suffix in ('.spv', '.import.nir', '.nir', '.bin', '.asm', '.json', '.resources.json', '.native.h', '.native.json')}})
    evidence = {'schema': 1, 'scope': 'original Maxwell CS input to target IR3 machine code; host only, no GPU or game execution',
                'chip_id': args.chip_id, 'frontend_build': frontend, 'ir3_build': ir3,
                'spirv_tools': {name: {'path': str(path), 'sha256': digest(path.read_bytes())}
                                for name, path in spirv_tools.items()},
                'check_source_sha256': digest(Path(__file__).read_bytes()), 'commands': commands, 'cases': results}
    path = args.out / 'check.json'
    path.write_text(json.dumps(evidence, indent=2) + '\n')
    print(path)


if __name__ == '__main__':
    main()
