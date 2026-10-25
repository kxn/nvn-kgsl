#!/usr/bin/env python3
"""HOST ONLY public-NVN qualification caller/oracle and real Android shared link; no GPU execution."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('root','check','ndk','out'):p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--python',type=Path,default=Path(sys.executable))
 a=p.parse_args();root=a.root.resolve();native=Path(__file__).resolve().parents[1];dependency=root/'nvn-native';out=a.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,scope=__doc__,passed=False,gpu_submits=0,runs=[]);inputs={}
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise RuntimeError(r.stderr[-5000:]+r.stdout[-2000:])
  return r.stdout
 try:
  sys.path.insert(0,str(native/'tools'));sys.path.insert(1,str(dependency/'tools'));from nvn_pipeline_qualification import build
  h,manifest,locked=build(a.check.resolve(),out,root=root);inputs.update(locked);proof['qualification']=manifest
  for base in (native,dependency):
   for folder in ('src','include','android','third_party/nvn','third_party/kgsl','tests'):
    for f in (base/folder).rglob('*'):
     if f.is_file() and f.suffix in ('.c','.h','.inc','.py'):inputs[str(f.resolve())]=sha(f)
  for f in native.glob('tools/*pipeline*.py'):inputs[str(f.resolve())]=sha(f)
  inputs[str((native/'CMakeLists.txt').resolve())]=sha(native/'CMakeLists.txt')
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNVN_NVN_PIPELINE_CALL=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-I'+str(native/'include'),'-I'+str(dependency/'include'),'-I'+str(dependency/'third_party/nvn'),'-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(h))]
  modules=[(native if name=='nvn' else dependency)/'src'/f'{name}.c' for name in ('nvn','compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries')]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   binary=out/cc;run([cc,*common,*flags,native/'tests/d2r_bridge/nvn_pipeline_caller.c',native/'tests/nvn_pipeline_probe.c',dependency/'tests/d2r_bridge/memory_backend.c',*modules,'-lm','-o',binary])
   text=run([binary,out])
   if 'GPU backend ENOTSUP preserved'not in text:raise RuntimeError('caller refusal scope changed')
  # Existing NumPy tool only; synthetic image generation is an offline test oracle.
  python=a.python.absolute();inputs[str(python.resolve())]=sha(python)
  text=run([python,native/'tests/test_nvn_pipeline_validation.py'])
  if json.loads(text).get('checks')!=16:raise RuntimeError('oracle negative inventory changed')
  ndk=a.ndk.resolve();toolchain=ndk/'build/cmake/android.toolchain.cmake';inputs[str(toolchain)]=sha(toolchain)
  clang=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang';inputs[str(clang.resolve())]=sha(clang.resolve())
  run(['cmake','-S',native,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_NVN_PIPELINE_CALL=ON','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(h)])
  run(['cmake','--build',out/'cmake','--parallel','4'])
  if any(sha(k)!=v for k,v in inputs.items()):raise RuntimeError('source/artifact changed during check')
  proof['passed']=True;proof['artifacts']={str(out/'cmake/libnvn_kgsl_probe.so'):sha(out/'cmake/libnvn_kgsl_probe.so')}
 except Exception as e:proof['error']=str(e)
 proof['inputs']=inputs;(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'),gpu_submits=0)));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
