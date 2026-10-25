#!/usr/bin/env python3
"""HOST canonical GOB source-resource ownership + real NDK backend strict link.
No GPU/game execution, CPU image conversion, or Vulkan/Metal implementation.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    p.add_argument('--candidate',type=Path,help='ignored overlay containing nvn-native/src and tests; default tracked')
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--ndk-clang',type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';overlay=a.candidate.resolve() if a.candidate else None
    core=overlay/'nvn-native/src/nvn.c' if overlay else n/'src/nvn.c'
    fixture=overlay/'tests/source_resource.c' if overlay else n/'tests/d2r_bridge/source_resource.c'
    out=a.out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
    out.mkdir(parents=True);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    proof={'schema':1,'passed':False,'scope':'HOST ONLY canonical source BO/lifetime/boundaries; NDK strict link only; no GPU or game','commands':[]}
    def run(label,args):
        r=subprocess.run(list(map(str,args)),cwd=root,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
        entry={'label':label,'argv':list(map(str,args)),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr};proof['commands'].append(entry)
        if r.returncode:raise ValueError(label+' failed: '+r.stderr[-3000:])
        return r.stdout
    try:
        modules=[n/'src'/f'{x}.c' for x in ['compute','graphics','external_image','order','render_state','sampler_state','counter','texture_source', 'texture_bridge']]
        sources=[f for d in ['include','third_party/nvn','third_party/kgsl','src','android'] for f in (n/d).rglob('*') if f.is_file()]
        sources += [Path(__file__).resolve(),fixture,n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',n/'tests/d2r_bridge/resource_copy.c',n/'tests/d2r_bridge/target_clear.c',n/'tests/d2r_bridge/resource_packet_boundary.c']
        if overlay:sources += list((overlay/'nvn-native/src').glob('*'))
        proof['sources']={str(f):sha(f) for f in sources};proof['ndk_clang_sha256']=sha(a.ndk_clang.resolve())
        common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'third_party/kgsl'),'-I'+str(n/'src'),'-DNN_RESOURCE_CORE="'+str(core)+'"','-DNN_RESOURCE_MEMORY_BACKEND="'+str(n/'tests/d2r_bridge/memory_backend.c')+'"']
        for compiler,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
            for label,test,rows in [('source',fixture,1),('legacy_resources',n/'tests/d2r_bridge/target_clear.c',2),('legacy_boundaries',n/'tests/d2r_bridge/resource_packet_boundary.c',1)]:
                binary=out/(label+'-'+compiler);run(label+' '+compiler+' compile',[compiler,*common,*san,test,*modules,'-o',binary])
                result=[json.loads(line) for line in run(label+' '+compiler+' execute',[binary]).splitlines() if line.startswith('{')]
                if len(result)!=rows or any(not q.get('passed') for q in result):raise ValueError(label+' result contract')
                proof['commands'][-1]['results']=result
        binary=out/'libsource-resource-api29.so'
        run('NDK29 real backend shared strict link',[a.ndk_clang.resolve(),*common,'-fPIC','-shared','-Wl,--no-undefined',core,n/'src/kgsl.c',*modules,*[n/'android'/f'{x}.c' for x in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid', '-ldl','-o',binary])
        proof['artifacts']={str(binary):sha(binary)}
        if any(sha(f)!=h for f,h in proof['sources'].items()):raise ValueError('source closure changed while checking')
        proof['passed']=True
    except Exception as e:proof['error']=str(e)
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'passed':proof['passed'],'proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
