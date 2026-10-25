#!/usr/bin/env python3
"""HOST compiler/producer contract only. No GPU, runtime registration or game frame."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess

from package_graphics_pipelines import build, compiler_record, queries, checked

AUTHOR_FS = '''OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %main "main" %out
OpExecutionMode %main OriginUpperLeft
OpName %support "support_buffer"
OpDecorate %support DescriptorSet 0
OpDecorate %support Binding 0
OpDecorate %out Location 0
OpDecorate %a8 ArrayStride 16
OpDecorate %a73 ArrayStride 16
OpDecorate %block Block
OpMemberDecorate %block 0 Offset 0
OpMemberDecorate %block 1 Offset 16
OpMemberDecorate %block 2 Offset 144
OpMemberDecorate %block 3 Offset 160
OpMemberDecorate %block 4 Offset 176
OpMemberDecorate %block 5 Offset 192
OpMemberDecorate %block 6 Offset 1360
OpMemberDecorate %block 7 Offset 1376
%void = OpTypeVoid
%fn = OpTypeFunction %void
%f = OpTypeFloat 32
%u = OpTypeInt 32 0
%i = OpTypeInt 32 1
%v4f = OpTypeVector %f 4
%v4i = OpTypeVector %i 4
%u8 = OpConstant %u 8
%u73 = OpConstant %u 73
%i0 = OpConstant %i 0
%i2 = OpConstant %i 2
%i5 = OpConstant %i 5
%a8 = OpTypeArray %u %u8
%a73 = OpTypeArray %f %u73
%block = OpTypeStruct %u %a8 %v4f %v4f %i %a73 %v4i %i
%pblock = OpTypePointer Uniform %block
%pf = OpTypePointer Uniform %f
%pu = OpTypePointer Uniform %u
%pout = OpTypePointer Output %v4f
%support = OpVariable %pblock Uniform
%out = OpVariable %pout Output
%main = OpFunction %void None %fn
%entry = OpLabel
%ptr = OpAccessChain %pf %support %i5 %i2
%value = OpLoad %f %ptr
%color = OpCompositeConstruct %v4f %value %value %value %value
OpStore %out %color
OpReturn
OpFunctionEnd
'''


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('root', 'compiler', 'unit-stages', 'default-stages', 'spirv-tools', 'ndk', 'out'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    root, compiler, out = args.root.resolve(), args.compiler.resolve(), args.out.resolve()
    native = Path(__file__).resolve().parents[1]
    if out.exists() or not out.is_relative_to(root / '.local'):
        parser.error('fresh ignored output required')
    out.mkdir(parents=True)
    proof = {'schema': 1, 'passed': False, 'scope': __doc__, 'gpu_submits': 0, 'runs': []}
    inputs = {str(Path(__file__).resolve()): sha(__file__)}
    count = 0

    def test(value):
        nonlocal count
        count += 1
        if not value:
            raise ValueError('assertion ' + str(count))

    def run(argv, good=True):
        argv = list(map(str, argv))
        result = subprocess.run(argv, text=True, capture_output=True,
                                env=dict(os.environ, IR3_SHADER_DEBUG='asmroundtrip'))
        proof['runs'].append({'argv': argv, 'returncode': result.returncode,
                              'stdout': result.stdout, 'stderr': result.stderr})
        test((result.returncode == 0) == good)
        return result

    try:
        tool, _, record = compiler_record(compiler, native, inputs)
        unit = json.loads(args.unit_stages.read_text())
        default = json.loads(args.default_stages.read_text())
        for path, stage in ((args.unit_stages, unit), (args.default_stages, default)):
            inputs[str(path.resolve())] = sha(path)
            checked(stage['inputs'], inputs)
            checked(stage['artifacts'], inputs)
            test(stage['gpu_execution'] is False)
        test(unit['native_backend_policy'] == {'unit_render_scale': True})
        test(default['native_backend_policy'] == {'unit_render_scale': False})
        test(set(unit['stages']) == set(default['stages']))
        for key, value in unit['stages'].items():
            other = default['stages'][key]
            test(value['envelope']['inputs'] == other['envelope']['inputs'])
            test(value['frontend_passed'] and other['frontend_passed'])
            test(Path(value['prefix'] + '.spv').read_bytes() == Path(other['prefix'] + '.spv').read_bytes())
            if value['stage'] == 'fs':
                test(value['envelope']['inputs']['c1']['size'] == 256)
        test(len(unit['pairs']) == 3 and all(pair['passed'] for pair in unit['pairs']))
        for pair in unit['pairs']:
            fs = pair['program']['fs']
            usage = fs['nir_usage']
            test(usage['unit_render_scale']['replaced_scale_loads'] > 0)
            test(not usage['unit_render_scale']['remaining_support_loads'])
            test(all(item['block'] != 0 for item in fs['ubo_push_ranges']))
            fs_entry = unit['stages'][pair['fs']]
            requirements, reductions = queries(fs_entry['resources'], usage)
            test(not any(item['name'] == 'QueryTextureCoordNormalized' for item in requirements))
            test(any(item['name'] == 'backend/unit_render_scale' for item in requirements))
            test(len([item for item in reductions if item.get('raw_query', {}).get('name') == 'QueryTextureCoordNormalized']) == 2)
            for field in ('samples', 'other'):
                changed = copy.deepcopy(usage)
                changed['resource_semantics']['bindings'][0][field] += 1
                required, _ = queries(fs_entry['resources'], changed)
                test(any(item['name'] == 'QueryTextureCoordNormalized' and item['args'][0] == 110 for item in required))
            changed = copy.deepcopy(usage)
            changed['resource_semantics']['bindings'].pop(0)
            required, _ = queries(fs_entry['resources'], changed)
            test(any(item['name'] == 'QueryTextureCoordNormalized' and item['args'][0] == 110 for item in required))
        unit_header = build(root, args.unit_stages.resolve(), compiler, 1179648, out / 'unit-package')
        build(root, args.unit_stages.resolve(), compiler, 1179648, out / 'unit-package', True)
        test(unit_header.is_file())
        fixture=native/'tests/d2r_bridge/unit_render_scale.c'
        inputs[str(fixture)]=sha(fixture)
        for path in (native/'include').rglob('*.h'):inputs[str(path)]=sha(path)
        common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-DNN_UNIT_SCALE_ARCHIVE='+json.dumps(str(unit_header))]
        for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
            binary=out/('recipe-'+cc)
            run([cc,*common,*flags,fixture,'-o',binary])
            result=run([binary]);stats=json.loads(result.stdout)
            test(stats['ubo0_patches']==0 and stats['ubo_patches']==9 and stats['pairs']==3 and stats['gpu_submits']==0)
        cc=args.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        inputs[str(cc.resolve())]=sha(cc)
        run([cc,*common,fixture,'-Wl,--no-undefined','-o',out/'recipe-api29'])
        graphics_tool=compiler/'build/src/freedreno/fdl/nvn-graphics-ir3'
        test(str(graphics_tool)in record['artifacts'])
        vertex = next(value for value in unit['stages'].values() if value['stage'] == 'vs')['prefix'] + '.spv'
        tools = args.spirv_tools.resolve()
        for name in ('spirv-as', 'spirv-val'):
            inputs[str(tools / name)] = sha(tools / name)
        dynamic = AUTHOR_FS.replace('%main "main" %out', '%main "main" %out %coord').replace('OpDecorate %out Location 0', 'OpDecorate %out Location 0\nOpDecorate %coord BuiltIn FragCoord').replace('%pout = OpTypePointer Output %v4f', '%pout = OpTypePointer Output %v4f\n%pin = OpTypePointer Input %v4f').replace('%out = OpVariable %pout Output', '%out = OpVariable %pout Output\n%coord = OpVariable %pin Input').replace('%ptr = OpAccessChain %pf %support %i5 %i2', '%position = OpLoad %v4f %coord\n%x = OpCompositeExtract %f %position 0\n%index = OpConvertFToS %i %x\n%ptr = OpAccessChain %pf %support %i5 %index')
        cases = {
            'scale': (AUTHOR_FS, True, True),
            'dynamic-index': (dynamic, True, False),
            'out-of-range-index': (AUTHOR_FS.replace('%i2 = OpConstant %i 2', '%i2 = OpConstant %i 73'), True, False),
            'viewport-field': (AUTHOR_FS.replace('%pout = OpTypePointer Output %v4f', '%pout = OpTypePointer Output %v4f\n%pv4f = OpTypePointer Uniform %v4f').replace('%ptr = OpAccessChain %pf %support %i5 %i2\n%value = OpLoad %f %ptr', '%ptr = OpAccessChain %pv4f %support %i2\n%vector = OpLoad %v4f %ptr\n%value = OpCompositeExtract %f %vector 0'), True, False),
            'tfe-field': (AUTHOR_FS.replace('%i5 = OpConstant %i 5', '%i5 = OpConstant %i 5\n%i7 = OpConstant %i 7').replace('%pf = OpTypePointer Uniform %f', '%pf = OpTypePointer Uniform %f\n%pi = OpTypePointer Uniform %i').replace('%ptr = OpAccessChain %pf %support %i5 %i2\n%value = OpLoad %f %ptr', '%ptr = OpAccessChain %pi %support %i7\n%integer = OpLoad %i %ptr\n%value = OpBitcast %f %integer'), True, False),
            'other-field': (AUTHOR_FS.replace('%ptr = OpAccessChain %pf %support %i5 %i2\n%value = OpLoad %f %ptr', '%ptr = OpAccessChain %pu %support %i0\n%uint = OpLoad %u %ptr\n%value = OpBitcast %f %uint'), True, False),
            'renamed': (AUTHOR_FS.replace('"support_buffer"', '"guest_cb0"'), False, False),
            'wrong-offset': (AUTHOR_FS.replace('5 Offset 192', '5 Offset 208').replace('6 Offset 1360', '6 Offset 1376').replace('7 Offset 1376', '7 Offset 1392'), False, False),
            'other-binding': (AUTHOR_FS.replace('%support Binding 0', '%support Binding 1'), True, False),
        }
        for name, (source, good, folded) in cases.items():
            asm, spv, prefix = out / (name + '.spvasm'), out / (name + '.spv'), out / name
            asm.write_text(source)
            run([tools / 'spirv-as', '--target-env', 'spv1.3', asm, '-o', spv])
            run([tools / 'spirv-val', spv])
            run([graphics_tool, '--unit-render-scale', vertex, spv, prefix, 0, '0x06050002'], good)
            if not good:
                test(not Path(str(prefix) + '.fs.bin').exists())
                continue
            meta = json.loads(Path(str(prefix) + '.link.json').read_text())['fs']
            stats = meta['nir_usage']['unit_render_scale']
            test(bool(stats['replaced_scale_loads']) == folded)
            if name in ('other-field', 'viewport-field', 'tfe-field', 'dynamic-index', 'out-of-range-index'):
                test(stats['remaining_support_loads'] > 0)
                test(any(item['block'] == 0 for item in meta['ubo_push_ranges']))
        test(all(sha(path) == value for path, value in inputs.items()))
        proof.update(passed=True, checks=count, inputs=inputs,
                     unit_package=str(out / 'unit-package/check.json'),
                     actual_runtime_support=False, original_code_control_c1_unchanged=True)
        proof['artifacts'] = {str(path): sha(path) for path in out.rglob('*') if path.is_file()}
    except Exception as error:
        proof.update(error=str(error), inputs=inputs, checks=count)
    (out / 'check.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps({'passed': proof['passed'], 'checks': count, 'proof': str(out / 'check.json')}))
    return 0 if proof['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
