#!/usr/bin/env python3
"""HOST-only actual-source enqueue snapshot/preflight/owners and Android link; no GPU/game."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import shutil
import sys


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--runtime',type=Path,help='immutable native snapshot; defaults to this tool native directory')
    parser.add_argument('--color-check',type=Path,required=True)
    parser.add_argument('--c1-check',type=Path,required=True)
    parser.add_argument('--bridge-check',type=Path,required=True)
    parser.add_argument('--ndk',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();native=Path(__file__).resolve().parents[1]
    runtime=(args.runtime or native).resolve();out=args.out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):
        parser.error('fresh ignored output under ROOT/.local required')
    out.mkdir(parents=True)
    inputs={};proof={'schema':1,'scope':__doc__,'passed':False,'gpu_execution':False,'game_modules_executed':False,'runs':[]}
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    def remember(path):
        path=Path(path).resolve();value=sha(path)
        if str(path) in inputs and inputs[str(path)]!=value:raise ValueError('input changed '+str(path))
        inputs[str(path)]=value
    def run(argv):
        argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=env)
        proof['runs'].append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise ValueError('command failed: '+r.stderr[-5000:])
        return r.stdout
    def mapping(values):
        for path,value in values.items():
            expected=value if isinstance(value,str) else value['sha256']
            if sha(path)!=expected:raise ValueError('changed verified dependency '+path)
            remember(path)
    try:
        for executable in (sys.executable,shutil.which('clang'),shutil.which('gcc')):
            if not executable:raise ValueError('required host compiler missing')
            remember(executable)
        for folder in (runtime/'src',runtime/'include',runtime/'third_party',runtime/'android',native/'tests/d2r_bridge'):
            for path in folder.rglob('*'):
                if path.is_file() and path.suffix in ('.h','.c','.inc'):remember(path)
        for name in ('check_pipeline_enqueue.py','package_graphics_pipelines.py','package_graphics_stages.py','build_ir3.py'):
            remember(native/'tools'/name)
        packages=[]
        for record in (args.color_check.resolve(),args.c1_check.resolve()):
            d=json.loads(record.read_text());remember(record);mapping(d['inputs']);mapping(d['outputs'])
            run([sys.executable,native/'tools/package_graphics_pipelines.py','--root',root,'--stages',d['stage_check'],
                 '--compiler',Path(d['compiler_record']).parent,'--gmem-bytes',str(d['gmem_bytes']),
                 '--out',record.parent,'--verify'])
            packages.append(record.parent/'offline_graphics_pipelines.h')
        # Trusted raw reflection and actual common-NIR facts, never a legacy
        # handle-domain label or the archive expected query answer.
        spec=importlib.util.spec_from_file_location('enqueue_stage_metadata',native/'tools/package_graphics_stages.py')
        metadata=importlib.util.module_from_spec(spec);spec.loader.exec_module(metadata)
        c1_record=json.loads(args.c1_check.read_text());source_check=Path(c1_record['stage_check']);remember(source_check)
        source=json.loads(source_check.read_text());metadata_checks=0
        def check(value):
            nonlocal metadata_checks
            metadata_checks+=1
            if not value:raise ValueError('typed stage metadata assertion failed')
        def bad(call):
            try:call()
            except ValueError:check(True);return
            raise ValueError('malformed trusted metadata accepted')
        for key,stage in source['stages'].items():
            if not stage['frontend_passed'] or not stage['resources']['textures']:continue
            reflection=stage['resources'];rows=metadata.texture_bindings(reflection)
            check(len(rows)==2 and [r['handle_word'] for r in rows]==[110,112])
            check(all(r['cbuf_slot']==-1 and r['format']==0 for r in rows))
            known,uses=metadata.stage_texture_uses(source,key);check(known)
            check(all(not u['samples'] and not u['other'] and u['fetches']+u['size_queries'] for u in uses.values()))
            for field,value in [('Binding',True),('Set',-1),('Type',1<<32),('Format',None),('ArrayLength',0),('Flags',False),('Separate',0),('HandleIndex',1<<31),('CbufSlot',-1-(1<<31))]:
                wrong=copy.deepcopy(reflection);wrong['textures'][0][field]=value;bad(lambda:metadata.texture_bindings(wrong))
            wrong=copy.deepcopy(reflection);wrong['textures'][1]['Binding']=wrong['textures'][0]['Binding'];bad(lambda:metadata.texture_bindings(wrong))
            wrong=copy.deepcopy(reflection);wrong['textures'][0]['handle_sources']={'kind':'invalid','textureWord':777};check(metadata.texture_bindings(wrong)==rows)
            wrong=copy.deepcopy(source);wrong['pairs']=[];check(metadata.stage_texture_uses(wrong,key)==(False,{}))
            pair=next(p for p in source['pairs'] if p['passed'] and key in (p['vs'],p['fs']));label='vs' if pair['vs']==key else 'fs'
            wrong=copy.deepcopy(source);duplicate=copy.deepcopy(pair)
            duplicate['program'][label]['nir_usage']['resource_semantics']['bindings'][0]['samples']+=1
            wrong['pairs'].append(duplicate);bad(lambda:metadata.stage_texture_uses(wrong,key))
            wrong=copy.deepcopy(source);duplicate=copy.deepcopy(pair)
            del duplicate['program'][label]['nir_usage']['resource_semantics'];wrong['pairs'].append(duplicate)
            check(metadata.stage_texture_uses(wrong,key)==(False,{}))
        check(metadata_checks>0);proof['stage_metadata_checks']=metadata_checks
        record=args.bridge_check.resolve();d=json.loads(record.read_text());remember(record);mapping(d['inputs']);mapping(d['artifacts'])
        bundle=Path(d['header']);views=record.parent/'texture_native_views.h'
        if str(bundle) not in d['artifacts'] or str(views) not in d['artifacts']:raise ValueError('verified bridge/views missing')
        cc=args.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        remember(cc);remember(cc.parent/'llvm-readelf');remember(cc.parent/'llvm-nm')
        modules=[runtime/'src'/(name+'.c') for name in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries')]
        include=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(runtime/'include'),'-I'+str(runtime/'third_party/nvn'),'-I'+str(native/'tests/d2r_bridge')]
        defines=['-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_TEST_NVN_SOURCE='+json.dumps(str(native/'tests/d2r_bridge/pipeline_enqueue_instrumented.h')),
                 '-DNN_ENQUEUE_CORE_SOURCE='+json.dumps(str(runtime/'src/nvn.c')),
                 '-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(native/'tests/d2r_bridge/pipeline_enqueue_memory.c')),
                 '-DNN_REAL_MEMORY_BACKEND='+json.dumps(str(runtime/'tests/d2r_bridge/memory_backend.c')),
                 '-DNN_SOURCE_FIXTURE='+json.dumps(str(native/'tests/d2r_bridge/pipeline_nvn_source.c')),
                 '-DNN_PIPELINE_NATIVE_VIEWS='+json.dumps(str(views)),'-DNN_PIPELINE_TEXTURE_BRIDGE='+json.dumps(str(bundle))]
        for compiler,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
            for name,archive in zip(('owner','c1','callback'),(*packages,packages[0])):
                target=out/(name+'-'+compiler)
                filename='pipeline_record_callback.c' if name=='callback' else 'pipeline_enqueue_'+name+'.c'
                fixture=native/'tests/d2r_bridge'/filename
                run([compiler,*include,*defines,'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(archive)),*san,fixture,*modules,'-o',target])
                text=run([target])
                if name=='callback':
                    if text.count('GPU execution=false')!=1 or 'cases=15 callbacks=14 callback_checks=53'not in text:raise ValueError('callback fixture scope/result changed')
                elif text.count('GPU execution=false')!=2 or 'mock_submits=2'not in text:raise ValueError('fixture scope/result changed')
            for flags in (0x12,0x14):
                target=out/('gpu-policy-'+str(flags)+'-'+compiler)
                run([compiler,*include,'-DNN_UNQUALIFIED_FLAGS='+str(flags),*san,native/'tests/d2r_bridge/pool_gpu_uncached.c',*modules[:9],'-o',target])
                values=json.loads(run([target]))
                if values!={'scope':'HOST ONLY CPU-only pool and rejection tests','flags':flags,'checks':880 if flags==0x12 else 879,'policy_rejections':15,'gpu_submits':0}:raise ValueError('GPU policy result changed')
        # Actual Android/KGSL backend: no RAM fixture or fake Android includes.
        actual=[runtime/'src/kgsl.c',runtime/'src/nvn.c',*modules]
        actual += [runtime/'android'/(name+'.c') for name in ('window_session','window_android','present','present_pacer','window_display','ahb')]
        for mode in (0,1):
            target=out/('actual-api29-'+str(mode)+'.so')
            run([cc,*include,'-I'+str(runtime/'third_party/kgsl'),'-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode),'-fPIC','-shared',*actual,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',target])
            header=run([cc.parent/'llvm-readelf','-h',target])
            if 'AArch64'not in header or 'DYN'not in header:raise ValueError('wrong Android ELF ABI')
        if any(sha(path)!=value for path,value in inputs.items()):raise ValueError('inputs changed during check')
        proof['passed']=True
    except Exception as error:proof['error']=str(error)
    proof['inputs']=inputs;proof['artifacts']={str(p):sha(p) for p in out.iterdir() if p.is_file()}
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'passed':proof['passed'],'error':proof.get('error'),'proof':str(out/'proof.json'),'gpu_execution':False}))
    return 0 if proof['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
