#!/usr/bin/env python3
"""HOST two sanitizers/unknown owner/oracle negatives + actual Android/KGSL linkage. No GPU."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
from gather_probe_qualification import build,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='raw-native-gather-probe-host-v1',passed=False,gpu_execution=False,android_execution=False,runs=runs)
 def run(argv):
  argv=list(map(str,argv));x=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));(out/'progress.json').write_text(json.dumps(runs,indent=2)+'\n')
  if x.returncode:raise ValueError('HOST/NDK failure: '+x.stderr[-5000:])
  return x.stdout
 try:
  import numpy
  header,c,inputs=build(a.check.resolve(),out,root=root)
  for folder in ('include','src','third_party','android'):
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in('.c','.h','.cc','.inc'):inputs[str(f.resolve())]=sha(f)
  fixture=n/'tests/d2r_bridge/gather_probe_host.c';validator=n/'tests/test_gather_probe_validation.py'
  for f in (Path(__file__).resolve(),n/'tools/build_android_probe.py',n/'tools/run_android_probe.py',n/'CMakeLists.txt',fixture,validator,n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',Path(numpy.__file__).resolve(),Path(sys.executable).resolve()):inputs[str(f)]=sha(f)
  modules=[n/'src'/(s+'.c')for s in('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','pipeline')]
  common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
  defs=[]
  for k,v in {'NN_GATHER_MEMORY_BACKEND':n/'tests/d2r_bridge/memory_backend.c','NN_GATHER_PROBE_SOURCE':n/'tests/gather_probe.c','NVN_PIPELINE_QUALIFICATION_HEADER':header}.items():defs.append('-D'+k+'='+json.dumps(str(v)))
  results=[]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   inputs[str(Path(shutil.which(cc)).resolve())]=sha(Path(shutil.which(cc)).resolve());exe=out/('probe-'+cc)
   run([cc,*common,*defs,*san,'-DNN_PIPELINE_GATHER=1',fixture,*modules,'-lm','-o',exe]);raw=out/(cc+'-raw');raw.mkdir();text=run([exe,raw]);data=json.loads(text)
   if data.get('passed')is not True or data.get('submissions')!=96:raise ValueError('HOST matrix incomplete')
   result=out/(cc+'-result.json');result.write_text(text);oracle=json.loads(run([sys.executable,validator,a.check.resolve(),result,raw]));results.append(oracle)
   if oracle.get('negative_cases')!=30 or oracle.get('nonexecuting_HOST_pixels_rejected')is not True:raise ValueError('HOST oracle/negative matrix incomplete')
   off=out/('off-'+cc);run([cc,*common,*defs,*san,fixture,*modules,'-lm','-o',off]);offdir=out/(cc+'-off');offdir.mkdir();value=run([off,offdir])
   if 'defaultOFF closed'not in value:raise ValueError('defaultOFF changed')
  if results[0]!=results[1]:raise ValueError('sanitizer oracle results differ')
  ndk=a.ndk.resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
  for f in (cc.resolve(),readelf):inputs[str(f)]=sha(f)
  for mode in(0,1):
   lib=out/('libgather-probe-'+str(mode)+'-api29.so');modeflags=['-DNN_PIPELINE_GATHER=1']if mode else[]
   run([cc,*common,*modeflags,'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'tests/gather_probe.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,'-landroid','-ldl','-lm','-o',lib]);elf=run([readelf,'-d',lib])
   if any(v in elf.lower()for v in('libvulkan','libegl','libgles','metal')):raise ValueError('renderer dependency')
  run([cc,*common,'-fPIC','-c',n/'android/pipeline_probe_activity.c','-o',out/'activity-api29.o'])
  # CLI mixed modes must fail before any device/build work; normal mode and
  # consumer remain defaultOFF and have no synthetic shader transport.
  for extra in ('--nvn-api','--device-inspect','--native-pipeline-check'):
   argv=[sys.executable,n/'tools/build_android_probe.py','--sdk',ndk.parent.parent,'--ndk',ndk,'--out',out/'MUST_NOT_BUILD','--native-gather-check',a.check.resolve(),extra]
   if extra=='--native-pipeline-check':argv.append(str(a.check.resolve()))
   x=subprocess.run(list(map(str,argv)),capture_output=True,text=True)
   if x.returncode==0 or 'independent'not in x.stderr or(out/'MUST_NOT_BUILD').exists():raise ValueError('CLI mode isolation')
  if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('source/qualified input changed')
  proof.update(passed=True,HOST_nonexecuting_submits_per_sanitizer=96,actual_GPU_submits=0,ambiguous_owners_per_sanitizer=2,constructor_faults_per_sanitizer=5,negative_cases=30,cli_mode_negative_cases=3,default_OFF_closed=True,oracle=results[0])
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
