#!/usr/bin/env python3
"""Qualify the original normalized 2D VS/FS pair offline, without a GPU draw.

Use the existing strict frontend, SPIR-V tools, NIR/IR3 and VPC linker. Optional
measured GMEM evidence also generates a native template; this is not GPU proof.
"""
import argparse
import copy
import json
import os
from pathlib import Path
import re
import subprocess

from shader_fixture import validate, digest
from check_graphics import checked_image, check_sources, compiler_inputs


def texture_array_variant(text, variable):
    """Make a valid one-element descriptor array using SPIR-V assembly tools.

    Only resource layout changes in this negative input; the original NSO and
    shader fixture are never modified. This also supplies a non-scalar parent
    for the combined-image texture/sampler type casts produced by the importer.
    """
    variable_pattern = rf'(?m)^\s*{re.escape(variable)} = OpVariable (\S+) UniformConstant$'
    match = re.search(variable_pattern, text)
    if not match:
        raise ValueError('sampled resource variable missing')
    pointer = match.group(1)
    pointee = re.search(rf'(?m)^\s*{re.escape(pointer)} = OpTypePointer UniformConstant (\S+)$', text).group(1)
    uint = re.search(r'(?m)^\s*(\S+) = OpTypeInt 32 0$', text).group(1)
    declaration = (f'%nvn_array_len = OpConstant {uint} 1\n'
                   f'%nvn_array_zero = OpConstant {uint} 0\n'
                   f'%nvn_array_type = OpTypeArray {pointee} %nvn_array_len\n'
                   '%nvn_array_ptr = OpTypePointer UniformConstant %nvn_array_type\n'
                   f'{variable} = OpVariable %nvn_array_ptr UniformConstant')
    text, count = re.subn(variable_pattern, lambda _: declaration, text)
    if count != 1:
        raise ValueError('expected exactly one sampled variable declaration')
    load_pattern = rf'(?m)^(\s*)(\S+) = OpLoad {re.escape(pointee)} {re.escape(variable)}$'
    index = 0

    def replace_load(match):
        nonlocal index
        index += 1
        access = f'%nvn_array_access_{index}'
        return (f'{match[1]}{access} = OpAccessChain {pointer} {variable} %nvn_array_zero\n'
                f'{match[1]}{match[2]} = OpLoad {pointee} {access}')

    text = re.sub(load_pattern, replace_load, text)
    if index == 0:
        raise ValueError('sampled image load missing')
    return text


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dotnet', type=Path, required=True)
    p.add_argument('--frontend-build', type=Path, required=True)
    p.add_argument('--ir3-build', type=Path, required=True)
    p.add_argument('--spirv-val', type=Path, required=True)
    p.add_argument('--extraction', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--gmem-evidence', type=Path, help='Ordinary UID KGSL device qualification with measured GMEM')
    p.add_argument('--modes', action='store_true', help='Explicit schema3 origin/depth scalar declarations; requires measured GMEM evidence')
    p.add_argument('--chip-id', type=lambda value: int(value, 0), help='Exact target chip; defaults to measured device, or A735 without evidence')
    args = p.parse_args()
    out = args.out.resolve()
    root = Path(__file__).resolve().parents[2]
    if not out.is_relative_to(root / '.local'):
        p.error('original and derived shader artifacts must remain under this worktree .local')
    out.mkdir(parents=True, exist_ok=True)
    if args.modes and not args.gmem_evidence:
        p.error('--modes requires --gmem-evidence')
    frontend, ir3, compiler_verification = compiler_inputs(args.frontend_build / 'build-record.json',
                                                         args.ir3_build / 'build-record.json')
    if digest(args.dotnet.read_bytes()) != frontend['dotnet_sha256']:
        p.error('dotnet executable mismatch')
    tools = {name: args.spirv_val.parent / name for name in ('spirv-val', 'spirv-dis', 'spirv-as')}
    compiler = args.ir3_build.resolve() / 'build/src/freedreno/fdl/nvn-graphics-ir3'
    extraction = json.loads(args.extraction.read_text())
    if not extraction['segment_hashes_verified'] or set(extraction['outputs']) != {'vs', 'fs'}:
        p.error('expected a verified original VS/FS extraction record')
    env = os.environ.copy()
    env['IR3_SHADER_DEBUG'] = 'asmroundtrip'
    result = {'schema': 1, 'scope': 'original Maxwell texture VS/FS through target compilation and VPC linkage; host only, no GPU sampling, draw or game execution',
        'frontend_build': frontend, 'ir3_build': ir3, 'inputs': {}, 'commands': [],
        'extraction': extraction, 'extraction_sha256': digest(args.extraction.read_bytes()),
        'tools': {name: {'path': str(path), 'sha256': digest(path.read_bytes())} for name, path in tools.items()},
        'check_source_sha256': digest(Path(__file__).read_bytes()), 'check_sources': check_sources(),
        'compiler_verification': compiler_verification, 'modes': args.modes, 'rejections': {}}
    gmem = 0
    chip = args.chip_id if args.chip_id is not None else 0x43030b00
    if args.gmem_evidence:
        device = json.loads(args.gmem_evidence.read_text())
        r = device['result']
        if (not r['passed'] or r['uid'] < 10000 or type(r['chip_id']) is not int or
                r['chip_id'] not in (0x43030b00, 0x06050002) or type(r.get('gmem_bytes')) is not int or
                r['gmem_bytes'] <= 0 or r['gmem_bytes'] % 4096):
            p.error('invalid measured GMEM qualification')
        result['device_evidence'] = device
        if args.chip_id is not None and chip != r['chip_id']:
            p.error('target chip differs from measured device evidence')
        chip, gmem = r['chip_id'], r['gmem_bytes']
    if chip not in (0x43030b00, 0x06050002):
        p.error('unsupported exact target chip')
    result['chip_id'] = chip
    host_args = [0, hex(chip)]

    def run(argv, *, rejection=None):
        argv = list(map(str, argv))
        completed = subprocess.run(argv, text=True, capture_output=True, env=env)
        execution = {'argv': argv, 'returncode': completed.returncode, 'stderr': completed.stderr.strip()}
        result['commands'].append(execution)
        if rejection is None:
            if completed.returncode:
                raise ValueError(f'command failed: {argv}\n{completed.stderr}')
        elif completed.returncode == 0 or rejection not in completed.stderr:
            raise ValueError(f'expected rejection {rejection}: {argv}\n{completed.stderr}')
        return execution

    try:
        fixtures = {}
        for stage in ('vs', 'fs'):
            selected = extraction['outputs'][stage]
            fixture = Path(selected['fixture']).resolve()
            if digest(fixture.read_bytes()) != selected['fixture_sha256']:
                raise ValueError('fixture differs from extraction record')
            document = json.loads(fixture.read_text())
            if document['stage'] != stage or document['provenance']['nso_sha256'] != extraction['input_sha256']:
                raise ValueError('stage or original input identity mismatch')
            envelope = validate(document, fixture.parent)
            if any(envelope['inputs'][key]['sha256'] != selected[key + '_sha256'] for key in ('code', 'control')):
                raise ValueError('original selected blob differs from extraction record')
            run([args.dotnet, frontend['entrypoint'], fixture, out / stage])
            run([tools['spirv-val'], out / (stage + '.spv')])
            assembly = out / (stage + '.spvasm')
            run([tools['spirv-dis'], out / (stage + '.spv'), '-o', assembly])
            assembly_text = assembly.read_text()
            storage = 'Output' if stage == 'vs' else 'Input'
            interface_ids = set(re.findall(r'(?m)^\s*(\S+) = OpVariable \S+ ' + storage + r'$', assembly_text))
            interface = {int(location): variable for variable, location in
                re.findall(r'OpDecorate (\S+) Location (\d+)', assembly_text) if variable in interface_ids}
            if not {1, 2}.issubset(interface):
                raise ValueError('original UV/color locations 1 and 2 are missing: ' + stage)
            metadata = json.loads((out / (stage + '.resources.json')).read_text())
            if metadata['stage'] != stage or metadata['code_sha256'] != selected['code_sha256'] or metadata['spv_sha256'] != digest((out / (stage + '.spv')).read_bytes()):
                raise ValueError('frontend input/output identity mismatch')
            fixtures[stage] = (fixture, document)
            result['inputs'][stage] = {'fixture': str(fixture), 'fixture_sha256': selected['fixture_sha256'], 'envelope': envelope,
                'provenance': document['provenance'], 'resources': metadata,
                'interface': {'storage': storage, 'locations': interface}}
        resources = result['inputs']['fs']['resources']
        if len(resources['textures']) != 1 or len(resources['samplers']) != 1 or result['inputs']['vs']['resources']['textures']:
            raise ValueError('expected one fragment texture/sampler pair and no vertex texture')
        texture = resources['textures'][0]
        if texture['Set'] != 0 or texture['Type'] != 3 or texture['ArrayLength'] != 1 or texture['Separate'] or texture['Format'] != 0:
            raise ValueError('original sampled texture profile changed')
        negative = out / 'negative'
        negative.mkdir(exist_ok=True)
        fixture, original = fixtures['fs']
        for name, diagnostic in [('missing_coordinate_query', 'missing explicit texture query: QueryTextureCoordNormalized'),
                                 ('missing_host_query', 'missing explicit query: QueryHostGatherBiasPrecision')]:
            bad = copy.deepcopy(original)
            for blob in bad['blobs'].values():
                blob['path'] = str(fixture.parent / blob['path'])
            if name == 'missing_coordinate_query':
                del bad['state']['resources']['textures'][0]['coord_normalized']
            else:
                del bad['state']['host']['QueryHostGatherBiasPrecision']
            path = negative / (name + '.fixture.json')
            path.write_text(json.dumps(bad, indent=2) + '\n')
            result['rejections'][name] = run([args.dotnet, frontend['entrypoint'], path, negative / name], rejection=diagnostic)
        disassembly = out / 'fs.spvasm'
        run([tools['spirv-dis'], out / 'fs.spv', '-o', disassembly])
        text = disassembly.read_text()
        bindings = re.findall(r'OpDecorate (\S+) Binding ' + str(texture['Binding']) + r'\b', text)
        candidates = [var for var in bindings if re.search(r'OpDecorate ' + re.escape(var) + r' DescriptorSet 0\b', text)]
        if len(candidates) != 1:
            raise ValueError('unique sampled descriptor decoration missing')
        variable = candidates[0]
        variants = {
            'texture_descriptor_set': (text.replace(f'OpDecorate {variable} DescriptorSet 0', f'OpDecorate {variable} DescriptorSet 1'), 'sampled resource declaration layout'),
            'texture_binding_limit': (text.replace(f'OpDecorate {variable} Binding {texture["Binding"]}', f'OpDecorate {variable} Binding 16'), 'sampled resource declaration layout'),
            'texture_descriptor_array': (texture_array_variant(text, variable), 'only scalar floating non-array non-shadow sampled 2D declarations'),
        }
        for name, (assembly, diagnostic) in variants.items():
            asm, spv = negative / (name + '.spvasm'), negative / (name + '.spv')
            asm.write_text(assembly)
            run([tools['spirv-as'], '--target-env', 'spv1.3', asm, '-o', spv])
            run([tools['spirv-val'], spv])
            result['rejections'][name] = run([compiler, out / 'vs.spv', spv, negative / name, *host_args], rejection=diagnostic)
        # The legal highest binding verifies that table bounds come from the
        # actual sparse resource index, rather than declaration count one.
        highest_asm, highest_spv = out / 'binding15.spvasm', out / 'binding15.spv'
        highest_asm.write_text(text.replace(f'OpDecorate {variable} Binding {texture["Binding"]}', f'OpDecorate {variable} Binding 15'))
        run([tools['spirv-as'], '--target-env', 'spv1.3', highest_asm, '-o', highest_spv])
        run([tools['spirv-val'], highest_spv])
        run([compiler, out / 'vs.spv', highest_spv, out / 'binding15', *host_args])
        sparse = json.loads((out / 'binding15.link.json').read_text())
        if sparse['chip_id'] != chip:
            raise ValueError('sparse binding program chip differs from selected target')
        if sparse['fs']['num_samplers'] != 16:
            raise ValueError('highest valid sparse binding did not reserve 16 native slots')
        result['sparse_binding_check'] = {'scope': 'synthetic SPIR-V resource-layout variant, not original shader deployment',
            'binding': 15, 'num_samplers': sparse['fs']['num_samplers'], 'program': sparse}
        run([compiler, out / 'vs.spv', out / 'fs.spv', out / 'pair', gmem, hex(chip)] + (['--modes'] if args.modes else []))
        link = json.loads((out / 'pair.link.json').read_text())
        if link['chip_id'] != chip:
            raise ValueError('compiled sampled program chip differs from selected target')
        if bool(link.get('graphics_modes', False)) != args.modes:
            raise ValueError('compiler mode opt-in differs from requested sampled profile')
        if gmem:
            result['image'] = checked_image(out, chip=chip, gmem=gmem, modes=args.modes, profile=1)
        if link['linkage']['fragment_varying_count'] < 2:
            raise ValueError('UV/color producer linkage missing')
        for stage in ('vs', 'fs'):
            if link[stage]['binary_bytes'] != (out / ('pair.' + stage + '.bin')).stat().st_size:
                raise ValueError('machine code byte count differs from reflection')
        prefetch = link['fs']['sampler_prefetch']
        if prefetch['count'] != len(prefetch['entries']):
            raise ValueError('prefetch count differs from entries')
        result['program'] = link
        texture_slots = max(t['Binding'] for t in resources['textures']) + 1
        sampler_slots = max(s['Binding'] for s in resources['samplers']) + 1
        result['resource_tables'] = {'texture_slots': texture_slots, 'sampler_slots': sampler_slots,
            'mapping': 'set-zero SPIR-V binding is native table index; holes remain reserved'}
        if link['fs']['num_samplers'] < max(texture_slots, sampler_slots):
            raise ValueError('IR3 sampler count does not cover actual texture/sampler binding indices')
        if gmem:
            if not all((out / ('pair' + suffix)).is_file() for suffix in ('.graphics.h', '.graphics.json')):
                raise ValueError('missing native sampled graphics artifact')
            result['rejections']['insufficient_gmem'] = run(
                [compiler, out / 'vs.spv', out / 'fs.spv', negative / 'smallgmem', 4096, hex(chip)],
                rejection='device GMEM is smaller than the common cache layout')
        result['passed'] = True
        result['artifacts'] = {str(path.relative_to(out)): digest(path.read_bytes()) for path in out.rglob('*')
            if path.is_file() and path.suffix in ('.spv', '.spvasm', '.nir', '.bin', '.asm', '.json', '.h')
            and path.name not in ('check.json', 'check.failed.json')}
        (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
        print(out / 'check.json')
    except Exception as error:
        result.update(passed=False, error=str(error))
        result['artifacts'] = {str(path.relative_to(out)): digest(path.read_bytes()) for path in out.rglob('*')
            if path.is_file() and path.suffix in ('.spv', '.spvasm', '.nir', '.bin', '.asm', '.json', '.h')
            and path.name not in ('check.json', 'check.failed.json')}
        (out / 'check.failed.json').write_text(json.dumps(result, indent=2) + '\n')
        raise


if __name__ == '__main__':
    main()
