#!/usr/bin/env python3
"""HOST actual public depth-source144 failure ownership, independent oracle, API29 CMake link. No GPU."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
from public_depth_sampled_qualification import build,sha
from public_depth_sampled_validation import state_ledger

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--python',type=Path,default=Path(sys.executable));a=p.parse_args();root=a.root.resolve();native=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh worktree/.local output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='public-depth-sampled-host-v1',passed=False,gpu_execution=False,android_execution=False)
 def run(argv,cwd=None,allowleaks=False):
  argv=list(map(str,argv));env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0'if allowleaks else'1')+':abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1');r=subprocess.run(argv,cwd=cwd,env=env,text=True,capture_output=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-10000:])
  return r.stdout
 try:
  h,c,inputs=build(a.check.resolve(),out,root)
  for folder in('src','include','third_party','android','tests'):
   for path in(native/folder).rglob('*'):
    if path.is_file()and path.suffix in('.c','.cc','.h','.inc','.py'):inputs[str(path)]=sha(path)
  for path in[Path(__file__).resolve(),native/'CMakeLists.txt',native/'tools/build_android_probe.py',native/'tools/run_android_probe.py',*[Path(shutil.which(t)).resolve()for t in('gcc','g++','clang','clang++','cmake','ninja')]]:inputs[str(path)]=sha(path)
  oracle=json.loads(run([a.python.absolute(),native/'tests/test_public_depth_sampled_validation.py',a.check.resolve()]))
  if oracle['negative_cases']<30 or oracle['gpu_execution']is not False:raise ValueError('independent oracle negatives missing')
  macros=('NN_NVN_COLOR_TARGETS','NN_PIPELINE_COLOR_TARGETS','NN_NVN_GRAPHICS_SOURCE_CACHE','NN_NVN_BLOCK_COPY','NN_PIPELINE_RUNTIME_VIEWS','NN_NVN_DEPTH_TARGETS','NN_PIPELINE_DEPTH_NATIVE','NN_PIPELINE_SAMPLED_IMAGES','NN_NVN_SAMPLED_IMAGES','NN_PIPELINE_GATHER','NN_NVN_DEPTH_SAMPLED','NN_PIPELINE_DEPTH_SAMPLED')
  definitions=['-DNN_PUBLIC_DEPTH_SAMPLED_HOST_NEGATIVES=1']+['-D'+s+'=1'for s in macros]+['-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=1','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(h)),'-DNN_MEMORY_BACKEND='+json.dumps(str(native/'tests/d2r_bridge/memory_backend.c'))]
  cbase=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
  names=('nvn','compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_buffer_sample','texture_bc_shadow','sampled_image','pipeline_views','color_target','color_target_shadow','depth_attachment','depth_transfer','depth_shadow','pipeline_depth_archive','depth_sampled','depth_sampled_native')
  sources=[native/'tests/d2r_bridge/public_depth_sampled_failure.c',native/'tests/public_depth_sampled_probe.c',*[native/'src'/(s+'.c')for s in names]];cppsources=[native/'src'/(s+'.cc')for s in('runtime_fdl','depth_fdl','depth_a2d','depth_native')];normal=json.loads((Path(c['runtime_fdl_check']).parent/'commands.json').read_text());results=[]
  for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
   folder=out/cc;folder.mkdir();objects=[]
   for i,s in enumerate(sources):
    obj=folder/(str(i)+'.o');run([cc,*cbase,*definitions,*san,'-fno-pie','-c',s,'-o',obj]);objects.append(obj)
   template=next(t for t in normal if t['argv'][0]==cpp);argv=template['argv'];start=next(i for i,x in enumerate(argv)if x.startswith('-I')and x.endswith('/nvn-native/include'));end=argv.index('-Wl,--as-needed');flags=argv[1:start];libs=argv[end:-2];binary=folder/'public-depth-sampled-failure'
   run([cpp,*flags,'-I'+str(native/'include'),'-I'+str(native/'third_party/nvn'),*san,'-fno-pie','-no-pie',*cppsources,*objects,*libs,'-Wl,--gc-sections','-o',binary],template['cwd'])
   for mode in('refused','unknown','finish','metadata'):
    destination=folder/'unexecuted-RAM-targets'if mode=='metadata'else out
    if mode=='metadata':destination.mkdir()
    row=json.loads(run([binary,destination,mode],allowleaks=mode!='metadata'));results.append(dict(compiler=cc,mode=mode,result=row,gpu_execution=False))
    if mode=='metadata':
     if row['completed_cases']!=144 or row['submissions']!=144 or row['finishes']!=144 or row['cleanup_errno'] or row['resources_retained']is not False:raise ValueError('incomplete public144 HOST state/lifetime paths')
     state_ledger(row,c)
    elif row['completed_cases']or row['cleanup_errno']!=16 or row['resources_retained']is not True:raise ValueError('uncertain owner not retained')
  tc=a.ndk.resolve()/'build/cmake/android.toolchain.cmake';inputs[str(tc)]=sha(tc)
  run(['cmake','-S',native,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(tc),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_PUBLIC_DEPTH_SAMPLED_PROBE=ON','-DNVN_CASC_GATHER_DEPTH_LIBRARIES='+';'.join(c['native_libraries']),'-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=ON','-DNVN_NVN_PIPELINE_RUNTIME_FDL_LIBRARY='+c['runtime_fdl_library'],'-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(h)])
  run(['cmake','--build',out/'cmake','--parallel','4'])
  commands=json.loads((out/'cmake/compile_commands.json').read_text());actual=[x for x in commands if Path(x['file'])==native/'tests/public_depth_sampled_probe.c']
  if len(actual)!=1 or any(Path(x['file'])==native/'tests/nvn_pipeline_probe.c'for x in commands):raise ValueError('actual CMake probe source substitution failed')
  for macro in macros:
   core=[x['command']for x in commands if Path(x['file'])==native/'src/nvn.c']
   if len(core)!=1 or '-D'+macro+'=1'not in core[0]:raise ValueError('actual common source feature missing '+macro)
  helptext=run([a.python.absolute(),native/'tools/build_android_probe.py','--help'])
  if '--public-depth-sampled-check'not in helptext:raise ValueError('normal CLI option absent')
  for conflicting in('--nvn-api','--native-depth-check','--nvn-pipeline-call'):
   args=[a.python.absolute(),native/'tools/build_android_probe.py','--sdk','${LOCAL_VALIDATION_ROOT}/.local/android-sdk','--ndk',a.ndk.resolve(),'--out',out/('rejected-'+conflicting[2:]),'--public-depth-sampled-check',a.check.resolve(),conflicting]
   if conflicting=='--native-depth-check':args.append(a.check.resolve())
   r=subprocess.run(list(map(str,args)),capture_output=True,text=True);runs.append(dict(argv=list(map(str,args)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
   if r.returncode!=2 or 'independent public depth-source'not in r.stderr:raise ValueError('normal CLI conflict was not rejected before any build/device')
  for path,digest in inputs.items():
   if sha(path)!=digest:raise ValueError('source changed during normal check '+path)
  proof.update(passed=True,oracle=oracle,host_failures=results,actual_ndk_cmake_link=True,cli_conflict_negatives=3,host_actual_submitted_ib_rectangles_per_sanitizer=144,host_out_of_bounds_scissor_rejections_per_sanitizer=432,unknown_owner_scope='HOST failure seams intentionally retain entire owner graph; no fabricated GPU execution/completion')
 except Exception as e:proof['error']=str(e)
 (out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n');proof.update(inputs=inputs,artifacts={str(path):sha(path)for path in out.rglob('*')if path.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
