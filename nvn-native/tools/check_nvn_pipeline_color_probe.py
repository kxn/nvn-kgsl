#!/usr/bin/env python3
"""HOST real public GOB22 failure ownership + offline oracle; API29 normal CMake link."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
from nvn_pipeline_color_qualification import build,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--python',type=Path,default=Path(sys.executable))
 a=p.parse_args();root=a.root.resolve();native=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh worktree/.local output required')
 out.mkdir(parents=True);runs=[];proof=dict(schema=1,scope=__doc__,passed=False,gpu_execution=False,android_execution=False);inputs={}
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-6000:])
  return r.stdout
 try:
  h,c,inputs=build(a.check.resolve(),out,root)
  for folder in('src','include','third_party','android','tests'):
   for q in(native/folder).rglob('*'):
    if q.is_file()and q.suffix in('.c','.h','.inc','.cc','.py'):inputs[str(q)]=sha(q)
  for q in [Path(__file__).resolve(),native/'CMakeLists.txt',native/'tools/build_android_probe.py',native/'tools/run_android_probe.py']:inputs[str(q)]=sha(q)
  oracle=json.loads(run([a.python.absolute(),native/'tests/test_nvn_pipeline_color_validation.py',a.check.resolve()]));
  if oracle['negative_cases']<25 or oracle['gpu_execution']is not False:raise ValueError('oracle negative scope')
  defs=['-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_PIPELINE_RUNTIME_VIEWS=1','-DNN_PIPELINE_COLOR_TARGETS=1','-DNN_NVN_COLOR_TARGETS=1','-DNN_NVN_BLOCK_COPY=1','-DNVN_NVN_PIPELINE_COLOR=1','-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=1','-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(h)),'-DNN_MEMORY_BACKEND='+json.dumps(str(native/'tests/d2r_bridge/memory_backend.c'))]
  inc=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
  modules=[native/'src'/(s+'.c')for s in('nvn','compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','color_target','color_target_shadow','pipeline_views','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_buffer_sample')]
  fdl_commands=json.loads((Path(c['runtime_fdl_check']).parent/'commands.json').read_text());results=[]
  for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
   objects=[]
   for i,q in enumerate([native/'tests/d2r_bridge/public_color_probe_failure.c',native/'tests/nvn_pipeline_probe.c',*modules]):
    o=out/(cc+str(i)+'.o');run([cc,*inc,*defs,*san,'-fno-pie','-c',q,'-o',o]);objects.append(o)
   template=next(v for v in fdl_commands if v['argv'][0]==cpp and str(root/'nvn-native/tests/runtime_fdl.cc')in v['argv']);argv=[]
   for word in template['argv']:
    if word==str(root/'nvn-native/tests/runtime_fdl.cc'):argv.extend(map(str,objects))
    else:argv.append(word)
   binary=out/('failure-'+cc);argv[argv.index('-o')+1]=str(binary);run(argv,template['cwd'])
   for mode in('refused','unknown'):
    result=json.loads(run([binary,out,mode]));results.append(result)
    if result['completed_cases']or not result['resources_retained']or result['cleanup_errno']!=16:raise ValueError('public owner failure')
  toolchain=a.ndk.resolve()/'build/cmake/android.toolchain.cmake';inputs[str(toolchain)]=sha(toolchain)
  run(['cmake','-S',native,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_NVN_PIPELINE_COLOR=ON','-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=ON','-DNVN_NVN_PIPELINE_RUNTIME_FDL_LIBRARY='+c['runtime_fdl_library'],'-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(h)])
  run(['cmake','--build',out/'cmake','--parallel','4'])
  # Common command-line admission only; no SDK/ADB or Android launch occurs.
  text=run([sys.executable,native/'tools/build_android_probe.py','--help']);
  if '--nvn-color-target-check'not in text:raise ValueError('normal CLI option absent')
  for q,hsh in inputs.items():
   if sha(q)!=hsh:raise ValueError('input changed '+q)
  proof.update(passed=True,oracle=oracle,actual_public_failure_results=results,actual_ndk_normal_cmake_link=True)
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,artifacts={str(q):sha(q)for q in out.rglob('*')if q.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'),error=proof.get('error'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
