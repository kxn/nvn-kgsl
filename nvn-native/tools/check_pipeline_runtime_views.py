#!/usr/bin/env python3
"""HOST actual-FDL dynamic target cache/typed NVN ownership; API29 link, no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,runtime,fdl_path,pipeline,bridge,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=root/'nvn-native';normal=load(native/'tools/build_runtime_fdl.py','runtime_views_fdl')
 library,fdl=normal.verify(root,fdl_path)
 import sys
 sys.path.insert(0,str(native/'tools'))
 from graphics_pipeline_cache import build as pipeline_build
 from texture_bridge_bundle import build as view_build
 ph,pm=pipeline_build(root,pipeline);bh,bm=view_build(root,bridge)
 inputs={str(fdl_path):sha(fdl_path),str(pipeline):sha(pipeline),str(bridge):sha(bridge),**fdl['inputs'],**fdl['artifacts'],**pm['inputs'],**bm['inputs'],**bm['artifacts']}
 for folder in ('src','include','android','third_party','tests/d2r_bridge'):
  for p in (runtime/folder).rglob('*'):
   if p.is_file()and p.suffix in ('.c','.h','.inc','.cc'):inputs[str(p)]=sha(p)
 for p in [Path(__file__).resolve(),runtime/'tests/pipeline_views.c']:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-6000:])
  return r.stdout
 host=Path(fdl['normal_ir3'])/'build';entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));_,hf=normal.flags(entry)
 ninja=Path(shutil.which('ninja')).resolve();cmd=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in cmd[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 modules=[runtime/'src'/(n+'.c')for n in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','pipeline_views')]
 include=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(runtime/'third_party/nvn'),'-I'+str(runtime/'include'),'-I'+str(runtime/'include/nvn_native'),'-I'+str(runtime/'tests/d2r_bridge')]
 defines=['-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_PIPELINE_RUNTIME_VIEWS=1']
 for macro,path in [('NN_TEST_NVN_SOURCE',runtime/'src/nvn.c'),('NN_MEMORY_BACKEND_SOURCE',runtime/'tests/d2r_bridge/pipeline_draw_memory.c'),('NN_REAL_MEMORY_BACKEND',runtime/'tests/d2r_bridge/memory_backend.c'),('NN_SOURCE_FIXTURE',runtime/'tests/d2r_bridge/pipeline_nvn_source.c'),('NN_PIPELINE_ARCHIVE',ph),('NN_PIPELINE_NATIVE_VIEWS',Path(bh).parent/'texture_native_views.h'),('NN_PIPELINE_TEXTURE_BRIDGE',bh)]:defines.append('-D'+macro+'='+json.dumps(str(path)))
 fixture=runtime/'tests/d2r_bridge/pipeline_runtime_views.c';facade=runtime/'src/runtime_fdl.cc'
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/cc;folder.mkdir();objects=[]
  for tool in (cc,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  fdl_obj=folder/'fdl.o';run([cpp,*ff,'-Werror','-I'+str(runtime/'include'),*san,'-fno-pie','-c',facade,'-o',fdl_obj],host)
  cache=folder/'cache.o';run([cc,*include,*san,'-fno-pie','-DNN_PIPELINE_VIEWS_SOURCE='+json.dumps(str(runtime/'src/pipeline_views.c')),'-c',runtime/'tests/pipeline_views.c','-o',cache]);binary=folder/'cache';run([cpp,'-no-pie',*san,cache,fdl_obj,*libs,'-o',binary],host);cache_text=run([binary])
  if 'entries=201'not in cache_text or 'allocator_failures=2'not in cache_text:raise ValueError('cache inventory changed')
  for i,source in enumerate([fixture,*modules]):
   obj=folder/(str(i)+'.o');run([cc,*include,*defines,*san,'-fno-pie','-c',source,'-o',obj]);objects.append(obj)
  binary=folder/'core';run([cpp,'-no-pie',*san,*objects,fdl_obj,*libs,'-o',binary],host);core_text=run([binary])
  if core_text.count('mock_submits=3')!=2 or 'Draw73x41 source=0'not in core_text or 'Draw73x41 source=1'not in core_text:raise ValueError('core ownership scope changed')
  results.append(dict(cache=cache_text,core=core_text))
 if results[0]!=results[1]:raise ValueError('different sanitizer results')
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc.resolve())]=sha(cc)
 run([cc,*include,*defines,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*modules,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'core-api29-not-run'])
 actual=[runtime/'src/nvn.c',runtime/'src/kgsl.c',*modules,*[runtime/'android'/(n+'.c')for n in ('window_session','window_android','present','present_pacer','window_display','ahb')]]
 run([cc,*include,*defines,'-I'+str(runtime/'third_party/kgsl'),'-fPIC','-shared',*actual,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'core-api29.so'])
 # Explicit OFF path compiles without the new module or FDL library.
 off=[p for p in actual if p.name!='pipeline_views.c']
 run([cc,*include,'-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_PIPELINE_RUNTIME_VIEWS=0','-I'+str(runtime/'third_party/kgsl'),'-fPIC','-shared',*off,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'core-off-api29.so'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 proof=dict(schema=1,kind='actual-runtime-fdl-pipeline-views-host-v1',passed=True,android_execution=False,gpu_execution=False,scope=__doc__,results=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime','fdl-check','pipeline-check','bridge-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime.resolve(),a.fdl_check.resolve(),a.pipeline_check.resolve(),a.bridge_check.resolve(),a.ndk.resolve(),a.out.resolve()))
