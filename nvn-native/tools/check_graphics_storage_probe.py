#!/usr/bin/env python3
"""Full HOST ownership/fault matrix + independent oracle + API29 real link; no GPU."""
import argparse,importlib.util,json,os,subprocess,sys,shutil
from pathlib import Path
from graphics_storage_probe_qualification import build,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='raw-native-graphics-storage-probe-host-v1',passed=False,gpu_execution=False,android_execution=False)
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-7000:])
  return r.stdout
 try:
  header,c,inputs=build(a.check.resolve(),out/'compiled',root=root)
  for folder in('include','src','third_party','android'):
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in('.c','.h','.cc','.inc'):inputs[str(f)]=sha(f)
  fixture=n/'tests/d2r_bridge/graphics_storage_probe_host.c';validator=n/'tests/test_graphics_storage_probe_validation.py'
  for f in(Path(__file__).resolve(),fixture,validator,n/'tools/build_android_probe.py',n/'tools/run_android_probe.py',n/'tools/graphics_storage_probe_runner.py',n/'CMakeLists.txt',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc'):inputs[str(f)]=sha(f)
  fdlcheck=Path(json.loads(a.check.read_text())['runtime_fdl_check']);fdl=json.loads(fdlcheck.read_text());commands=json.loads((fdlcheck.parent/'commands.json').read_text());template=next(x for x in commands if x['argv'][0]=='g++');v=template['argv'];start=next(i for i,x in enumerate(v)if x.startswith('-I')and x.endswith('/nvn-native/include'));end=v.index('-Wl,--as-needed');cppflags=v[1:start];libs=v[end:-2]
  modules=[n/'src'/(s+'.c')for s in('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','pipeline','pipeline_storage','pipeline_archive')]
  base=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'tests')];defs=['-DNN_STORAGE_GPU_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_STORAGE_GPU_PROBE_SOURCE='+json.dumps(str(n/'tests/graphics_storage_probe.c')),'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header))];results=[]
  for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
   for enabled in(0,1):
    folder=out/(cc+str(enabled));folder.mkdir();objects=[]
    for i,s in enumerate([fixture,*modules]):
     obj=folder/(str(i)+'.o');run([cc,*base,*defs,'-DNN_PIPELINE_GRAPHICS_STORAGE='+str(enabled),*san,'-fno-pie','-c',s,'-o',obj]);objects.append(obj)
    flags=cppflags if cpp=='g++'else[x for x in cppflags if x not in('-flifetime-dse=1','-Wno-error=maybe-uninitialized')];binary=folder/'host';run([cpp,*flags,'-I'+str(n/'include'),'-Werror',*san,'-fno-pie','-no-pie',n/'src/runtime_fdl.cc',*objects,*libs,'-Wl,--gc-sections','-lm','-o',binary],template['cwd']);raw=folder/'raw';raw.mkdir();text=run([binary,raw]);
    if enabled:
     result=json.loads(text.strip().splitlines()[-1]);assert result['submissions']==192 and result['files']==576 and result['passed']is True;results.append(result)
    elif 'defaultOFF closed'not in text:raise ValueError('default OFF changed')
   for tool in(cc,cpp):f=Path(shutil.which(tool)).resolve();inputs[str(f)]=sha(f)
  if results[0]!=results[1]:raise ValueError('HOST full matrix differs')
  oracle=json.loads(run([sys.executable,validator,'--check',out/'compiled/check.json']));assert oracle['negative_cases']==29 and oracle['positive']['submissions']==192
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc.resolve())]=sha(cc.resolve());run([cc,*base,'-DNN_PIPELINE_GRAPHICS_STORAGE=1','-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'tests/graphics_storage_probe.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,c['runtime_fdl_library'],'-landroid','-ldl','-lm','-o',out/'graphics-storage-real-api29.so']);run([cc,*base,'-fPIC','-c',n/'android/pipeline_probe_activity.c','-o',out/'activity-api29.o'])
  for extra in('--nvn-api','--device-inspect','--native-gather-check','--native-sampler-mip-check'):
   argv=[sys.executable,n/'tools/build_android_probe.py','--sdk',a.ndk.parent.parent,'--ndk',a.ndk,'--out',out/'MUST_NOT_BUILD','--native-graphics-storage-check',a.check.resolve(),extra]
   if extra.endswith('-check'):argv.append(a.check.resolve())
   q=subprocess.run(list(map(str,argv)),text=True,capture_output=True)
   if not q.returncode or'independent'not in q.stderr or(out/'MUST_NOT_BUILD').exists():raise ValueError('CLI mixed mode protection')
  if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('source/product changed during checks')
  proof.update(passed=True,HOST_nonexecuting_submits_per_sanitizer=192,actual_GPU_submits=0,ambiguous_owners_per_sanitizer=2,constructor_faults_per_sanitizer=5,default_OFF_closed=True,cli_negative_cases=4,oracle=oracle,results=results)
 except Exception as error:proof['error']=str(error)
 proof.update(inputs=inputs,runs=runs,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'),error=proof.get('error'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
