#!/usr/bin/env python3
"""Check original VS/FS inputs through strict Maxwell, NIR/IR3 and VPC linkage.

Only host compilation/linking is covered. Pixel/GPU execution needs the native
graphics state emitter and resources; successful machine code is not that proof.
"""
import argparse
import copy
import json
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
from shader_fixture import validate, digest
from check_maxwell import verify_build


def image_contract():
    path = Path(__file__).resolve().parents[1] / 'adapters/d2r/graphics_bundle.py'
    spec = importlib.util.spec_from_file_location('nn_graphics_image_contract', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def checked_image(directory, *, chip, gmem, modes, profile):
    image = json.loads((directory / 'pair.graphics.json').read_text())
    image_contract().validate_image_metadata(image, chip=chip, gmem=gmem, modes=modes, profile=profile)
    return image


def check_sources():
    native = Path(__file__).resolve().parents[1]
    names = ('tools/check_graphics.py', 'tools/check_textures.py', 'tools/shader_fixture.py',
             'tools/check_maxwell.py', 'adapters/d2r/graphics_bundle.py', 'adapters/d2r/texture_bundle.py')
    return {str(native / name): digest((native / name).read_bytes()) for name in names}


def compiler_inputs(frontend_path, ir3_path):
    """Revalidate compiler source manifests, tools and products, not just binaries."""
    root = Path(__file__).resolve().parents[2]
    frontend, ir3 = verify_build(frontend_path), verify_build(ir3_path)
    counts = {}
    for label, path, record in (('frontend', frontend_path, frontend), ('ir3', ir3_path, ir3)):
        manifest_path = path.parent / 'source-hashes.json'
        manifest_bytes = manifest_path.read_bytes()
        if digest(manifest_bytes) != record['source_manifest_sha256']:
            raise ValueError(label + ' source manifest mismatch')
        manifest = json.loads(manifest_bytes)
        if label == 'frontend' and manifest != record['sources']:
            raise ValueError('frontend source inventory mismatch')
        source_root = root if label == 'frontend' else path.parent / 'source/mesa-26.2.5'
        for name, expected in manifest.items():
            relative = Path(name)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('invalid compiler source path: ' + name)
            if label == 'frontend' and record.get('source_roots'):
                namespace, *parts = relative.parts
                if namespace not in record['source_roots'] or not parts:
                    raise ValueError('invalid frontend source namespace: ' + name)
                source = Path(record['source_roots'][namespace]).resolve().joinpath(*parts)
            else:
                source = source_root / relative
            actual = digest(('symlink:' + os.readlink(source)).encode()) if source.is_symlink() else digest(source.read_bytes())
            if actual != expected:
                raise ValueError(label + ' source changed: ' + name)
        for name, expected in record.get('tools', {}).items():
            if digest(Path(name).read_bytes()) != expected:
                raise ValueError('compiler tool changed: ' + name)
        if label == 'ir3':
            if digest((root / 'nvn-native/tools/build_ir3.py').read_bytes()) != record['builder_sha256']:
                raise ValueError('IR3 builder source changed')
            for name, expected in record['source_rules'].items():
                if digest((root / 'nvn-native' / name).read_bytes()) != expected:
                    raise ValueError('IR3 source rule changed: ' + name)
            if any(manifest.get(name) != expected for name, expected in record['overlay'].items()):
                raise ValueError('IR3 overlay/source manifest differs')
        else:
            for name, expected in record['restore_assets'].items():
                if digest(Path(name).read_bytes()) != expected:
                    raise ValueError('frontend restore asset changed: ' + name)
            for name, expected in record.get('compiler_overlay', {}).items():
                if Path(name).is_absolute() and digest(Path(name).read_bytes()) != expected:
                    raise ValueError('frontend overlay changed: ' + name)
        counts[label] = {'source_files': len(manifest), 'artifacts': len(record['artifacts']),
                         'build_record_sha256': digest(path.read_bytes())}
    return frontend, ir3, counts


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dotnet', type=Path, required=True)
    p.add_argument('--frontend-build', type=Path, required=True)
    p.add_argument('--ir3-build', type=Path, required=True)
    p.add_argument('--spirv-val', type=Path, required=True)
    p.add_argument('--vs', type=Path, required=True)
    p.add_argument('--fs', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--gmem-evidence', type=Path, help='Ordinary UID KGSL qualification with measured GMEM size')
    p.add_argument('--modes', action='store_true', help='Explicit schema3 origin/depth scalar declarations; requires measured GMEM evidence')
    p.add_argument('--chip-id', type=lambda value: int(value, 0), help='Exact target chip; defaults to measured device, or A735 without evidence')
    args = p.parse_args()
    if args.modes and not args.gmem_evidence:
        p.error('--modes requires --gmem-evidence')
    frontend, ir3, compiler_verification = compiler_inputs(args.frontend_build / 'build-record.json',
                                                         args.ir3_build / 'build-record.json')
    if digest(args.dotnet.read_bytes()) != frontend['dotnet_sha256']:
        p.error('dotnet executable mismatch')
    tools = {name: args.spirv_val.parent / name for name in ('spirv-val', 'spirv-dis', 'spirv-as')}
    compiler = args.ir3_build.resolve() / 'build/src/freedreno/fdl/nvn-graphics-ir3'
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy(); env['IR3_SHADER_DEBUG'] = 'asmroundtrip'
    commands, inputs = [], {}

    def run(argv, *, rejection=None):
        argv = list(map(str, argv)); commands.append(argv)
        result = subprocess.run(argv, text=True, capture_output=True, env=env)
        if rejection is None:
            if result.returncode:
                raise ValueError(f'command failed: {argv}\n{result.stderr}')
        elif result.returncode == 0 or rejection not in result.stderr:
            raise ValueError('expected rejection: ' + rejection + '\n' + result.stderr)
        return result.stderr.strip()

    for stage, fixture in [('vs', args.vs.resolve()), ('fs', args.fs.resolve())]:
        document = json.loads(fixture.read_text())
        if document['stage'] != stage:
            p.error('fixture stage mismatch')
        envelope = validate(document, fixture.parent)
        run([args.dotnet, frontend['entrypoint'], fixture, out / stage])
        run([args.spirv_val, out / (stage + '.spv')])
        metadata = json.loads((out / (stage + '.resources.json')).read_text())
        if metadata['stage'] != stage or metadata['code_sha256'] != envelope['inputs']['code']['sha256']:
            raise ValueError('frontend stage or original input identity mismatch')
        inputs[stage] = {'fixture': str(fixture), 'fixture_sha256': digest(fixture.read_bytes()), 'envelope': envelope,
                         'provenance': document['provenance'], 'resources': metadata}
    device, gmem = None, 0
    chip = args.chip_id if args.chip_id is not None else 0x43030b00
    if args.gmem_evidence:
        device = json.loads(args.gmem_evidence.read_text())
        r = device['result']
        if (not r['passed'] or r['uid'] < 10000 or type(r['chip_id']) is not int or
            r['chip_id'] not in (0x43030b00, 0x06050002) or type(r.get('gmem_bytes')) is not int or
            r['gmem_bytes'] <= 0 or r['gmem_bytes'] % 4096):
            p.error('GMEM evidence is not a successful ordinary-UID qualification of this device')
        if args.chip_id is not None and chip != r['chip_id']:
            p.error('target chip differs from measured device evidence')
        chip, gmem = r['chip_id'], r['gmem_bytes']
    if chip not in (0x43030b00, 0x06050002):
        p.error('unsupported exact target chip')
    host_args = [0, hex(chip)]
    run([compiler, out / 'vs.spv', out / 'fs.spv', out / 'pair', gmem, hex(chip)] + (['--modes'] if args.modes else []))
    link = json.loads((out / 'pair.link.json').read_text())
    if link['chip_id'] != chip:
        raise ValueError('compiled graphics chip differs from selected target')
    if gmem and not all((out / ('pair' + suffix)).is_file() for suffix in ('.graphics.h', '.graphics.json')):
        raise ValueError('missing native graphics state artifact')
    if bool(link.get('graphics_modes', False)) != args.modes:
        raise ValueError('compiler mode opt-in differs from requested profile')
    image = checked_image(out, chip=chip, gmem=gmem, modes=args.modes, profile=0) if gmem else None
    if not link['linkage']['fragment_varying_count']:
        raise ValueError('expected a real vertex-to-fragment varying')
    rejections = {}
    with tempfile.TemporaryDirectory(dir=out) as temporary:
        temporary = Path(temporary)
        fixture = args.vs.resolve()
        original = json.loads(fixture.read_text())
        for name in ('missing_graphics_state', 'sph_stage'):
            bad = copy.deepcopy(original)
            for blob in bad['blobs'].values():
                blob['path'] = str(fixture.parent / blob['path'])
            if name == 'missing_graphics_state':
                del bad['state']['graphics']['depth_mode']
                message = 'missing explicit graphics state: depth_mode'
            else:
                bad['stage'] = 'fs'; message = 'graphics container/SPH stage mismatch'
            path = temporary / 'bad.json'; path.write_text(json.dumps(bad))
            rejections[name] = run([args.dotnet, frontend['entrypoint'], path, temporary / 'bad'], rejection=message)
        asm = temporary / 'bad.spvasm'; spv = temporary / 'bad.spv'
        run([tools['spirv-dis'], out / 'fs.spv', '-o', asm])
        text = asm.read_text()
        if 'Location 1' not in text:
            raise ValueError('the qualified FS varying location changed')
        asm.write_text(text.replace('Location 1', 'Location 2'))
        run([tools['spirv-as'], '--target-env', 'spv1.3', asm, '-o', spv])
        run([args.spirv_val, spv])
        rejections['missing_vertex_producer'] = run([compiler, out / 'vs.spv', spv, temporary / 'badlink', *host_args],
            rejection='fragment input lacks vertex producer')
        if gmem:
            rejections['insufficient_gmem'] = run([compiler, out / 'vs.spv', out / 'fs.spv', temporary / 'badgmem', 4096, hex(chip)],
                rejection='device GMEM is smaller than the common cache layout')
    result = {'schema': 1, 'scope': 'original Maxwell VS/FS through target compilation and VPC linkage; host only, no draw or game execution',
              'frontend_build': frontend, 'ir3_build': ir3, 'inputs': inputs, 'program': link, 'rejections': rejections,
              'device_evidence': device, 'chip_id': chip, 'modes': args.modes, 'image': image,
              'compiler_verification': compiler_verification, 'check_sources': check_sources(),
              'tools': {name: digest(path.read_bytes()) for name, path in tools.items()},
              'check_source_sha256': digest(Path(__file__).read_bytes()), 'commands': commands,
              'artifacts': {p.name: digest(p.read_bytes()) for p in out.iterdir()
                            if p.is_file() and p.suffix in ('.spv', '.nir', '.bin', '.asm', '.json', '.h') and p.name != 'check.json'}}
    (out / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
    print(out / 'check.json')


if __name__ == '__main__':
    main()
