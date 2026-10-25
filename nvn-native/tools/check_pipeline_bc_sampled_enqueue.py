#!/usr/bin/env python3
"""HOST ONLY public NVN original sampled BC enqueue metadata/owners; actual Android link, no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,runtime,sampled_check,bridge_check,fdl_check,kernel_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=root/'nvn-native';normal=module(native/'tools/build_runtime_fdl.py','bc_enqueue_fdl')
 library,fdl=normal.verify(runtime.parent,fdl_check)
 block=module(native/'tools/compile_block_copy.py','bc_enqueue_kernel').verify(root,kernel_check)
 inputs={str(fdl_check):sha(fdl_check),str(kernel_check):sha(kernel_check),**fdl['inputs'],**fdl['artifacts'],**block['inputs'],**block['artifacts']}
 for record in (sampled_check,bridge_check):
  d=json.loads(record.read_text());inputs[str(record)]=sha(record)
  inputs.update(d['inputs']);inputs.update(d.get('outputs',d.get('artifacts',{})))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal source/product changed '+p)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv,cwd=None):
  p=subprocess.run(list(map(str,argv)),cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError((p.stdout+p.stderr)[-6000:])
  return p.stdout
 d=json.loads(sampled_check.read_text())
 run([shutil.which('python3'),native/'tools/package_graphics_pipelines.py','--root',root,'--stages',d['stage_check'],'--compiler',Path(d['compiler_record']).parent,'--gmem-bytes',d['gmem_bytes'],'--out',sampled_check.parent,'--verify'])
 views=bridge_check.parent/'texture_native_views.h';bundle=bridge_check.parent/'texture_bridge_bundle.h'
 modules=[runtime/'src'/(s+'.c')for s in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_bc_shadow')]
 fixture=runtime/'tests/d2r_bridge/pipeline_enqueue_bc_sampled.c';facade=runtime/'src/runtime_fdl.cc'
 host=Path(fdl['normal_ir3'])/'build';cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';ninja=Path(shutil.which('ninja')).resolve()
 for folder in ('include','src','third_party','android','tests/d2r_bridge'):
  for p in (runtime/folder).rglob('*'):
   if p.is_file()and p.suffix in ('.c','.h','.inc','.cc'):inputs[str(p)]=sha(p)
 for p in (Path(__file__).resolve(),cc.resolve(),ninja,host/'compile_commands.json',views,bundle):inputs[str(p)]=sha(p)
 entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));_,hf=normal.flags(entry)
 command=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in command[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 include=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(runtime/'third_party/nvn'),'-I'+str(runtime/'include'),'-I'+str(runtime/'include/nvn_native'),'-I'+str(runtime/'tests/d2r_bridge')]
 defines=['-DNN_SAMPLED_PAIR=0','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_NVN_BLOCK_COPY=1','-DNN_PIPELINE_BC_SAMPLED=1']
 for macro,path in [('NN_TEST_NVN_SOURCE',runtime/'tests/d2r_bridge/pipeline_enqueue_instrumented.h'),('NN_ENQUEUE_CORE_SOURCE',runtime/'src/nvn.c'),('NN_MEMORY_BACKEND_SOURCE',runtime/'tests/d2r_bridge/pipeline_bc_memory.c'),('NN_REAL_MEMORY_BACKEND',runtime/'tests/d2r_bridge/memory_backend.c'),('NN_SOURCE_FIXTURE',runtime/'tests/d2r_bridge/pipeline_nvn_source.c'),('NN_PIPELINE_ARCHIVE',sampled_check.parent/'offline_graphics_pipelines.h'),('NN_PIPELINE_NATIVE_VIEWS',views),('NN_PIPELINE_TEXTURE_BRIDGE',bundle),('NN_BLOCK_IMAGE',Path(block['header']))]:defines.append('-D'+macro+'='+json.dumps(str(path)))
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objs=[]
  for tool in (compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate([fixture,*modules]):
   o=folder/(str(i)+'.o');run([compiler,*include,*defines,*san,'-fno-pie','-c',s,'-o',o]);objs.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(runtime/'include'),'-Werror',*san,'-fno-pie','-c',facade,'-o',o],host);objs.append(o)
  binary=folder/'bc-enqueue';run([cpp,'-no-pie',*san,*objs,*libs,'-o',binary],host);text=run([binary]);results.append(text)
  if text.count('GPU execution=false')!=16 or text.count('mock_submits=2')!=16:raise ValueError('incomplete sixteen format/pool owner fixture')
 if results[0]!=results[1]:raise ValueError('sanitizer results differ')
 # Actual Android/KGSL module link, not executing the HOST RAM fixture as GPU.
 actual=[runtime/'src/kgsl.c',runtime/'src/nvn.c',*modules,*[runtime/'android'/(s+'.c')for s in ('window_session','window_android','present','present_pacer','window_display','ahb')]]
 run([cc,*include,*defines,'-I'+str(runtime/'third_party/kgsl'),'-fPIC','-shared',*actual,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'bc-enqueue-real-api29.so'])
 run([cc,*include,*defines,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*modules,library,'-Wl,--no-undefined','-landroid','-lm','-o',out/'bc-enqueue-fixture-api29'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during check '+p)
 proof=dict(schema=1,kind='public-nvn-sampled-bc-enqueue-host-v1',scope=__doc__,passed=True,gpu_execution=False,android_execution=False,sampled_normalization_known=True,normalization_policy="independent-public-RE-ordinary-2D-v1",sdk53_314_descriptor_observed=False,results=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime','sampled-check','bridge-check','runtime-fdl-check','kernel-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime.resolve(),a.sampled_check.resolve(),a.bridge_check.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))
