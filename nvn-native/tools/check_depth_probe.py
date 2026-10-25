#!/usr/bin/env python3
"""Normal depth qualifier HOST faults/oracle + real KGSL API29 strict links. No GPU."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
from depth_probe_qualification import build,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='raw-native-depth-probe-host-v2',passed=False,gpu_execution=False,android_execution=False,runs=runs)
 def run(argv,cwd=None):
  argv=list(map(str,argv));x=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));(out/'progress.json').write_text(json.dumps(runs,indent=2)+'\n')
  if x.returncode:raise ValueError(x.stdout[-1500:]+x.stderr[-5500:])
  return x.stdout
 try:
  import numpy
  header,c,inputs=build(a.check.resolve(),out,root=root)
  for folder in ('include','src','third_party','android'):
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in('.c','.h','.cc','.inc'):inputs[str(f.resolve())]=sha(f)
  fixture=n/'tests/d2r_bridge/depth_probe_host.c';validator=n/'tests/test_depth_probe_validation.py'
  for f in (Path(__file__).resolve(),n/'tools/build_android_probe.py',n/'tools/run_android_probe.py',n/'CMakeLists.txt',fixture,validator,n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',Path(numpy.__file__).resolve(),Path(sys.executable).resolve()):inputs[str(f)]=sha(f)
  modules=[n/'src'/(s+'.c')for s in ('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','pipeline','depth_attachment','depth_transfer','order','pipeline_views')]
  cppmodules=[n/'src'/(s+'.cc')for s in ('depth_native','depth_a2d','depth_fdl','runtime_fdl')]
  common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
  defs=[]
  for k,v in {'NN_DEPTH_MEMORY_BACKEND':n/'tests/d2r_bridge/memory_backend.c','NN_DEPTH_PROBE_SOURCE':n/'tests/depth_probe.c','NVN_PIPELINE_QUALIFICATION_HEADER':header}.items():defs.append('-D'+k+'='+json.dumps(str(v)))
  fdl_check=Path(c['fdl_check']);fdl_root=Path(c['fdl_root']);original=json.loads((fdl_check.parent/'commands.json').read_text());results=[]
  for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-fno-pie']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function','-fno-pie'])]:
   template=next(x for x in original if x['argv'][0]==cpp);args=template['argv'];start=args.index('-I'+str(fdl_root/'nvn-native/include'));end=args.index('-Wl,--as-needed');cppflags=args[1:start];libs=args[end:-2]
   for name in (cc,cpp):f=Path(shutil.which(name)).resolve();inputs[str(f)]=sha(f)
   for enabled in (0,1):
    objs=[]
    for i,src in enumerate([fixture,*modules]):
     obj=out/(cc+str(enabled)+'-'+str(i)+'.o');run([cc,*common,*defs,*san,'-DNN_PIPELINE_DEPTH_NATIVE='+str(enabled),'-c',src,'-o',obj]);objs.append(obj)
    exe=out/('probe-'+cc+'-'+str(enabled));run([cpp,*cppflags,*san,'-no-pie','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),*cppmodules,*objs,*libs,'-Wl,--wrap=nn_pipeline_encode_depth','-Wl,--gc-sections','-o',exe],template['cwd']);raw=out/(cc+'-'+str(enabled)+'-raw');raw.mkdir();text=run([exe,raw]);result=out/(cc+'-'+str(enabled)+'-result.json');result.write_text(text)
    if not enabled:
     if 'defaultOFF closed'not in text:raise ValueError('default OFF changed')
     continue
    data=json.loads(text)
    if data.get('passed')is not True or data.get('submissions')!=432:raise ValueError('HOST depth matrix incomplete')
    answer=json.loads(run([sys.executable,validator,'--check',a.check.resolve(),'--host-result',result,'--host-raw',raw]));results.append(answer)
    if answer.get('negative_cases')!=25 or answer.get('nonexecuting_HOST_pixels_rejected')is not True:raise ValueError('HOST depth no-op pixels/negative controls')
  if results[0]!=results[1]:raise ValueError('HOST sanitizer protocol mismatch')
  ndk=a.ndk.resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
  for f in (cc.resolve(),readelf):inputs[str(f)]=sha(f)
  for enabled in(0,1):
   lib=out/('libdepth-probe-'+str(enabled)+'-api29.so')
   run([cc,*common,'-DNN_PIPELINE_DEPTH_NATIVE='+str(enabled),'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'tests/depth_probe.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,*c['native_libraries'],'-landroid','-ldl','-lm','-o',lib]);elf=run([readelf,'-d',lib])
   if any(v in elf.lower()for v in('libvulkan','libegl','libgles','metal')):raise ValueError('renderer dependency')
  run([cc,*common,'-fPIC','-c',n/'android/pipeline_probe_activity.c','-o',out/'activity-api29.o'])
  for extra in ('--nvn-api','--device-inspect','--native-pipeline-check'):
   argv=[sys.executable,n/'tools/build_android_probe.py','--sdk',ndk.parent.parent,'--ndk',ndk,'--out',out/'MUST_NOT_BUILD','--native-depth-check',a.check.resolve(),extra]
   if extra=='--native-pipeline-check':argv.append(str(a.check.resolve()))
   x=subprocess.run(list(map(str,argv)),capture_output=True,text=True)
   if x.returncode==0 or 'independent'not in x.stderr or(out/'MUST_NOT_BUILD').exists():raise ValueError('CLI isolation')
  if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('source/normal input changed')
  proof.update(passed=True,HOST_nonexecuting_submits_per_sanitizer=432,actual_GPU_submits=0,ambiguous_owners_per_sanitizer=2,constructor_faults_per_sanitizer=5,negative_cases=25,cli_mode_negative_cases=3,default_OFF_closed=True,oracle=results[0])
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
