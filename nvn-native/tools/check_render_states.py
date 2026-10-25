#!/usr/bin/env python3
"""HOST ONLY: native value-state API and encoding; no GPU/game execution."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,required=True)
    ap.add_argument('--candidate',type=Path,help='ignored candidate overlay; omitted = tracked core')
    ap.add_argument('--ndk',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--color-header',type=Path,required=True,help='ignored schema3 original color qualifier header')
    ap.add_argument('--texture-header',type=Path,required=True,help='ignored schema3 original texture qualifier header')
    a=ap.parse_args();root=a.root.resolve();out=a.out.resolve();candidate=a.candidate.resolve() if a.candidate else root/'nvn-native'
    if out.exists() or not out.is_relative_to(root/'.local'):ap.error('choose fresh out below root/.local')
    out.mkdir(parents=True);proof={'scope':'HOST ONLY: RAM backend, no GPU or original module execution','passed':False,'runs':[]}
    tests=candidate/'tests' if a.candidate else root/'nvn-native/tests/d2r_bridge'
    sources=[candidate/'src/nvn.c',candidate/'src/render_state.c',candidate/'src/nvn_render_state.inc',candidate/'src/render_state_entries.inc',candidate/'src/nvn_entries.inc',tests/'render_state.c',tests/'draw_snapshots.c']
    sources+=list((candidate/'include/nvn_native').glob('*.h'))
    sources+=[root/'nvn-native/src'/f for f in ['graphics.c','compute.c','external_image.c','order.c','resource_copy.inc','resource_layout.inc','nvn_draw_dispatch.inc']]
    sources+=[root/'nvn-native/tests/d2r_bridge/memory_backend.c',root/'nvn-native/tests/d2r_bridge/window_session_backend.inc',Path(__file__).resolve(),a.color_header.resolve(),a.texture_header.resolve()]
    sources += list((root/'nvn-native/src').glob('*.c'))
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(label,cmd):
        r=subprocess.run([str(v) for v in cmd],cwd=root,env=env,text=True,capture_output=True)
        proof['runs'].append({'label':label,'command':[str(v) for v in cmd],'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise RuntimeError(label+' failed')
        return r.stdout
    try:
        before={str(p):sha(p) for p in sources};proof['sources']=before
        include=['-I'+str(candidate/'include'),'-I'+str(root/'nvn-native/include'),'-I'+str(root/'nvn-native/third_party/nvn'),'-iquote',str(candidate/'src'),'-iquote',str(root/'nvn-native/src')]
        flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror',*include]
        native='-DNN_TEST_NVN_SOURCE="'+str(candidate/'src/nvn.c')+'"'
        libs=[root/'nvn-native/src/sampler_state.c', root/'nvn-native/src/texture_source.c', root/'nvn-native/src/texture_bridge.c', root/'nvn-native/src/counter.c',candidate/'src/render_state.c',root/'nvn-native/src/order.c',root/'nvn-native/tests/d2r_bridge/memory_backend.c',root/'nvn-native/src/compute.c',root/'nvn-native/src/graphics.c',root/'nvn-native/src/external_image.c']
        for cc,sanitizer in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
            for name,source,header in [('api',tests/'render_state.c',None),('color',tests/'draw_snapshots.c',a.color_header.resolve()),('texture',tests/'draw_snapshots.c',a.texture_header.resolve())]:
                binary=out/(name+'-'+cc);defines=[native]
                if header:defines+=['-DNN_MODE_IMAGE_HEADER="'+str(header)+'"']
                run(name+' '+cc+' compile',[cc,*flags,*sanitizer,*defines,source,*libs,'-o',binary])
                result=json.loads(run(name+' '+cc+' run',[binary]));proof['runs'][-1]['result']=result
                expected={'api':450,'color':97,'texture':126}[name]
                if result['checks']!=expected or result['gpu_submits']!=0:raise ValueError('unexpected '+name+' result '+str(result))
        arm=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
        for file in ['nvn.c','render_state.c']:
            run('NDK '+file+' compile',[arm,*flags,'-fPIC','-c',candidate/'src'/file,'-o',out/(file+'.o')])
        proof['source_unchanged']=all(sha(Path(p))==v for p,v in before.items())
        if not proof['source_unchanged']:raise ValueError('source changed while testing')
        proof['passed']=True
    except Exception as e:proof['error']=str(e)
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'passed':proof['passed'],'proof':str(out/'proof.json'),'host_only':True}))
    return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
