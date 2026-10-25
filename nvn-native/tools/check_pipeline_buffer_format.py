#!/usr/bin/env python3
"""HOST ONLY explicit non-SNORM BUFFER source-format query; no GPU or actual binding capture."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
from package_graphics_pipelines import build, checked, compiler_record


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('root', 'frontend', 'compiler', 'stages', 'legacy-stages', 'ndk', 'out'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    native = Path(__file__).resolve().parents[1]
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('fresh ignored output required')
    out.mkdir(parents=True)
    inputs = {str(Path(__file__).resolve()): sha(__file__)}
    proof = {'schema': 1, 'scope': __doc__, 'passed': False, 'gpu_submits': 0, 'runs': []}
    checks = 0

    def test(value):
        nonlocal checks
        checks += 1
        if not value:
            raise ValueError('assertion ' + str(checks))

    def run(argv, good=True):
        argv = list(map(str, argv))
        result = subprocess.run(argv, text=True, capture_output=True,
                                env=dict(os.environ, IR3_SHADER_DEBUG='asmroundtrip',
                                         ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                                         UBSAN_OPTIONS='halt_on_error=1'))
        proof['runs'].append({'argv': argv, 'returncode': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        test((result.returncode == 0) == good)
        return result

    try:
        stages, legacy = (json.loads(path.read_text()) for path in (args.stages, args.legacy_stages))
        for path, data in ((args.stages, stages), (args.legacy_stages, legacy)):
            inputs[str(path.resolve())] = sha(path)
            checked(data['inputs'], inputs)
            checked(data['artifacts'], inputs)
        compiler_record(args.compiler.resolve(), native, inputs)
        test(len(stages['pairs']) == 2 and all(pair['passed'] for pair in stages['pairs']))
        reference = next(pair for pair in legacy['pairs'] if any(
            q['name'] == 'QuerySamplerType' and q.get('handle') == 112 and q.get('value') == 2
            for q in legacy['stages'][pair['fs']]['resources']['queries']))
        old = legacy['stages'][reference['fs']]
        identities = []
        for pair in stages['pairs']:
            stage = stages['stages'][pair['fs']]
            test(stage['envelope']['inputs'] == old['envelope']['inputs'])
            test(stage['envelope']['inputs']['c1']['size'] == 256)
            format_query = next(q for q in stage['resources']['queries'] if q['name'] == 'QueryTextureFormat')
            test(format_query['value'] in (25, 10) and format_query['handle'] == 112 and format_query['cbufSlot'] == -1)
            document = json.loads(Path(stage['fixture']).read_text())
            typed = next(value for value in document['state']['resources']['textures']
                         if value['handle'] == 112 and value['cbuf_slot'] == -1)
            test(format_query['value'] == {'R8G8B8A8Unorm': 25, 'R32Float': 10}[typed['texture_format']])
            host = [q for q in stage['resources']['queries'] if q['name'] == 'QueryHostSupportsSnormBufferTextureFormat']
            test(host and all(q['value'] is False for q in host))
            for suffix in ('.spv',):
                a, b = Path(stage['prefix'] + suffix), Path(old['prefix'] + suffix)
                test(a.read_bytes() == b.read_bytes())
                identities.append({'source_shader_format': format_query['value'], 'suffix': suffix, 'sha256': sha(a)})
            for suffix in ('.fs.bin', '.fs.asm', '.fs.nir', '.fs.unit-before.nir'):
                a, b = Path(pair['prefix'] + suffix), Path(reference['prefix'] + suffix)
                test(a.read_bytes() == b.read_bytes())
                identities.append({'source_shader_format': format_query['value'], 'suffix': suffix, 'sha256': sha(a)})
        header = build(root, args.stages.resolve(), args.compiler.resolve(), 1179648, out / 'package')
        build(root, args.stages.resolve(), args.compiler.resolve(), 1179648, out / 'package', True)
        package = json.loads((out / 'package/check.json').read_text())
        for requirement in package['requirements']:
            shader = package['requirements'].index(requirement)
            if shader == 0:
                continue
            required = requirement['queries']
            test(any(q['name'] == 'QueryHostSupportsSnormBufferTextureFormat' and q['values'] == [0] for q in required))
            test(any(q['name'] == 'QueryTextureFormat' and q['values'] in ([25], [10]) for q in required))
        fixture = native / 'tests/d2r_bridge/pipeline_buffer_format.c'
        modules = [native / 'src' / (name + '.c') for name in ('pipeline_resource_queries', 'pipeline_frontend', 'render_state')]
        for folder in (native / 'include', native / 'third_party/nvn'):
            for path in folder.rglob('*.h'):
                inputs[str(path)] = sha(path)
        for path in [fixture, *modules]:
            inputs[str(path)] = sha(path)
        common = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                  '-Wl,--gc-sections', '-I' + str(native / 'include'),
                  '-I' + str(native / 'third_party/nvn')]
        for compiler, sanitizer in (
                ('clang', ['-fsanitize=function', '-fsanitize-trap=function']),
                ('gcc', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'])):
            binary = out / ('format-' + compiler)
            run([compiler, *common, *sanitizer, fixture, *modules, '-o', binary])
            result = json.loads(run([binary]).stdout)
            test(result['rgba8_shader_format'] == 25 and result['r32f_shader_format'] == 10)
            test(result['snorm_policy'] is False and result['gpu_submits'] == 0 and result['public_buffer_registration'] is False)
        compiler = args.ndk.resolve() / 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        inputs[str(compiler.resolve())] = sha(compiler)
        run([compiler, *common, fixture, *modules, '-Wl,--no-undefined', '-o', out / 'format-api29'])
        front = json.loads((args.frontend / 'build-record.json').read_text())
        inputs[str(args.frontend.resolve() / 'build-record.json')] = sha(args.frontend / 'build-record.json')
        dotnet = front['commands'][0][0]
        selected = next(value for value in stages['stages'].values() if value['stage'] == 'fs')
        document = json.loads(Path(selected['fixture']).read_text())
        for name in ('missing', 'unknown', 'foreign-namespace', 'missing-capability'):
            doc = copy.deepcopy(document)
            target = next(value for value in doc['state']['resources']['textures'] if value['handle'] == 112 and value['cbuf_slot'] == -1)
            if name == 'missing':
                del target['texture_format']
            elif name == 'missing-capability':
                del doc['state']['host']['QueryHostSupportsSnormBufferTextureFormat']
            else:
                target['texture_format'] = 'NVN_FORMAT_RGBA8SN' if name == 'foreign-namespace' else 'Unknown'
            path, prefix = out / (name + '.fixture.json'), out / name
            path.write_text(json.dumps(doc, indent=2) + '\n')
            run([dotnet, front['entrypoint'], path, prefix], False)
            test(not Path(str(prefix) + '.spv').exists())
        test(all(sha(path) == value for path, value in inputs.items()))
        proof.update(passed=True, checks=checks, inputs=inputs, identity=identities,
                     actual_runtime_registration=False, actual_nvntarget112_known=False,
                     package=str(out / 'package/check.json'), generated_header=str(header))
        proof['artifacts'] = {str(path): sha(path) for path in out.rglob('*') if path.is_file()}
    except Exception as error:
        proof.update(error=str(error), checks=checks, inputs=inputs)
    (out / 'check.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': proof['passed'], 'checks': checks, 'proof': str(out / 'check.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
