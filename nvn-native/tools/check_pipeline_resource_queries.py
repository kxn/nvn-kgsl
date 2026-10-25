#!/usr/bin/env python3
"""HOST ONLY actual NVN registry resource queries; no GPU or game execution."""
import argparse
import hashlib
import json
import os
import re
import shutil
from pathlib import Path
import subprocess
from workspace_paths import shader_frontend_root


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shader-frontend-root',type=Path,help='Public shader frontend checkout; or NVN_SHADER_FRONTEND_ROOT')
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, help='explicit immutable nvn-native source snapshot; default ROOT/nvn-native')
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    native = Path(__file__).resolve().parents[1]
    runtime = (args.runtime or root / 'nvn-native').resolve()
    out = args.out.resolve()
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('fresh ignored output under ROOT/.local required')
    out.mkdir(parents=True)
    proof = {'schema': 1, 'scope': __doc__, 'passed': False, 'gpu_submits': 0,
             'runtime': str(runtime), 'source': str(native), 'runs': []}
    inputs = {}

    def run(argv):
        argv = list(map(str, argv))
        result = subprocess.run(argv, text=True, capture_output=True,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
                                         UBSAN_OPTIONS='halt_on_error=1'))
        proof['runs'].append({'argv': argv, 'returncode': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        if result.returncode:
            raise ValueError('command failed: ' + result.stderr[-3000:])
        return result.stdout

    try:
        # Independent semantic-name correspondence. Resolve the actual C# enum
        # and NVN declarations rather than copying the runtime ordinal ranges.
        shader_enum = shader_frontend_root(args.shader_frontend_root) / 'src/Ryujinx.Graphics.Shader/TextureFormat.cs'
        nvn_enum = runtime / 'third_party/nvn/nvn.h'
        names = re.search(r'public enum TextureFormat\s*\{([^}]+)\}', shader_enum.read_text()).group(1)
        enum = {name.strip(): i for i, name in enumerate(names.split(',')) if name.strip()}
        formats = []
        for nvn, shader in [('R','R'),('RG','R{}G{}'),('RGBA','R{}G{}B{}A{}')]:
            for bits, kinds in [(8, [('','Unorm'),('SN','Snorm'),('UI','Uint'),('I','Sint')]),
                                (16, [('F','Float'),('','Unorm'),('SN','Snorm'),('UI','Uint'),('I','Sint')]),
                                (32, [('F','Float'),('UI','Uint'),('I','Sint')])]:
                prefix = shader+str(bits) if nvn=='R' else shader.format(*([bits]*len(nvn)))
                formats += [('NVN_FORMAT_'+nvn+str(bits)+suffix, prefix+kind) for suffix,kind in kinds]
        formats += [('NVN_FORMAT_RGB10A2','R10G10B10A2Unorm'),('NVN_FORMAT_RGB10A2UI','R10G10B10A2Uint'),('NVN_FORMAT_R11G11B10F','R11G11B10Float')]
        if len(formats)!=39 or {s for _,s in formats}!=set(enum)-{'Unknown'}:
            raise ValueError('translator scalar color format enum changed')
        declarations = dict(re.findall(r'\b(NVN_FORMAT_\w+)\s*=\s*(0x[0-9A-Fa-f]+)', nvn_enum.read_text()))
        if any(n not in declarations for n,_ in formats):raise ValueError('NVN source format declaration missing')
        expected = out / 'image-format-expected.h'
        expected.write_text('/* HOST only: actual independent translator enum and public NVN names. */\nstatic const struct { NVNformat nvn; uint32_t shader; } nn_image_formats[]={'+','.join('{'+n+','+str(enum[s])+'}'for n,s in formats)+'};\n')
        inputs[str(shader_enum)]=sha(shader_enum);inputs[str(nvn_enum)]=sha(nvn_enum)
        proof['image_format_correspondence']=[{'nvn':n,'nvn_value':int(declarations[n],16),'shader':s,'shader_value':enum[s]}for n,s in formats]
        fixture = native / 'tests/d2r_bridge/pipeline_resource_queries.c'
        resource = native / 'src/pipeline_resource_queries.c'
        core = runtime / 'src/nvn.c'
        backend = runtime / 'tests/d2r_bridge/memory_backend.c'
        modules = [runtime / 'src' / (name + '.c') for name in (
            'compute', 'graphics', 'external_image', 'order', 'render_state',
            'sampler_state', 'texture_source', 'texture_bridge', 'counter',
            'offline_graphics', 'pipeline_program', 'pipeline_shader', 'pipeline_vertex',
            'pipeline', 'pipeline_archive', 'pipeline_source', 'pipeline_frontend')]
        for folder in (native / 'src', native / 'include', runtime / 'src', runtime / 'include',
                       runtime / 'third_party/nvn', runtime / 'third_party/kgsl'):
            for path in folder.rglob('*'):
                if path.is_file() and path.suffix in ('.h', '.c', '.inc'):
                    inputs[str(path.resolve())] = sha(path)
        for path in (Path(__file__).resolve(), fixture, backend,
                     runtime / 'tests/d2r_bridge/window_session_backend.inc'):
            inputs[str(path)] = sha(path)
        common = ['-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
                  '-I' + str(native / 'include'), '-I' + str(runtime / 'include'),
                  '-I' + str(runtime / 'third_party/nvn'), '-I' + str(runtime / 'src'),
                  '-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',
                  '-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',
                  '-DNN_TEST_NVN_SOURCE=' + json.dumps(str(core)),
                  '-DNN_MEMORY_BACKEND_SOURCE=' + json.dumps(str(backend))]
        common += ['-DNN_IMAGE_FORMAT_EXPECTED_HEADER='+json.dumps(str(expected))]
        observations=[]
        for compiler, sanitizer in (
            ('clang', ['-fsanitize=function', '-fsanitize-trap=function']),
            ('gcc', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                     '-fno-sanitize-recover=all'])):
            tool=Path(shutil.which(compiler)).resolve();inputs[str(tool)]=sha(tool)
            binary = out / ('resource-queries-' + compiler)
            run([compiler, *common, *sanitizer, fixture, resource, *modules, '-o', binary])
            text = run([binary])
            found=re.fullmatch(r'HOST ONLY resource-query actual NVN snapshot checks=(\d+) image-format-cases=78; GPU execution=false',text.strip())
            if not found or int(found[1])<=1505:
                raise ValueError('unexpected fixture result/scope')
            observations.append(int(found[1]))
            legacy=native/'tests/d2r_bridge/pipeline_buffer_format.c';inputs[str(legacy)]=sha(legacy)
            legacy_binary=out/('buffer-formats-'+compiler)
            run([compiler,*common,*sanitizer,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections',legacy,resource,*[native/'src'/(s+'.c')for s in('pipeline_frontend','render_state')],'-o',legacy_binary])
            legacy_result=json.loads(run([legacy_binary]))
            if legacy_result['gpu_submits'] or legacy_result['snorm_policy'] is not False or legacy_result['rgba8_shader_format']!=25:raise ValueError('BUFFER metadata regression')
        ndk = args.ndk.resolve()
        compiler = ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        inputs[str(compiler.resolve())] = sha(compiler)
        run([compiler, *common, fixture, resource, *modules, '-Wl,--no-undefined',
             '-o', out / 'resource-queries-api29'])
        run([compiler, *common, '-fPIC', '-c', resource, '-o', out / 'resource-queries-api29.o'])
        if any(sha(path) != expected for path, expected in inputs.items()):
            raise ValueError('source changed during qualification')
        if observations[0]!=observations[1]:raise ValueError('HOST query results differ')
        proof.update(passed=True, checks_per_host_run=observations[0],image_formats=39,image_format_cases=78,
                     android_execution=False, arm64_link_only=True,
                     actual_texture_draw=False, ownership_adapter='HOST fixture only')
        proof['artifacts'] = {str(path): sha(path) for path in out.iterdir() if path.is_file()}
    except Exception as error:
        proof['error'] = str(error)
    proof['inputs'] = inputs
    (out / 'check.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': proof['passed'], 'proof': str(out / 'check.json'), 'gpu_submits': 0}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
