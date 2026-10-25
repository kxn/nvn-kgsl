#!/usr/bin/env python3
"""Fresh normal graphics storage hooks: HOST RAM/fault checks, real API29 links. No GPU."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('native_depth_runtime_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,shader_check,fdl_root,fdl_check,depth_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=root/'nvn-native';fdl_tool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';_,fdl=module(fdl_tool).verify(fdl_root,fdl_check);inputs=dict(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(fdl_tool)]=sha(fdl_tool)
 def checked(path,kind):
  c=json.loads(path.read_text());inputs[str(path)]=sha(path)
  if c.get('kind')!=kind or c.get('passed')is not True or c.get('gpu_execution')is not False:raise ValueError('invalid input record '+str(path))
  for mapping in (c['inputs'],c['artifacts']):
   for p,h in mapping.items():
    if sha(p)!=h:raise ValueError('source/product changed '+p)
    inputs[p]=h
  return c
 shaders=checked(shader_check,'native-authored-graphics-storage-v1');depth=checked(depth_check,'native-depth-fdl-foundation-v1')
 if Path(fdl['ndk'])!=ndk:raise ValueError('wrong shader/NDK profile')
 sources=['src/pipeline.c','src/pipeline_storage.c','src/pipeline_archive.c','src/pipeline_program.c','src/pipeline_shader.c','src/pipeline_vertex.c','src/depth_attachment.c','src/render_state.c','src/sampler_state.c','src/offline_graphics.c','tests/d2r_bridge/graphics_storage.c'];cpp_sources=['src/depth_fdl.cc','src/depth_native.cc','src/runtime_fdl.cc'];all_sources=sources+cpp_sources+['src/kgsl.c','tools/check_graphics_storage.py','tests/d2r_bridge/memory_backend.c','tests/d2r_bridge/window_session_backend.inc']
 for p in [*[own/s for s in all_sources],*list((own/'include').rglob('*.h')),own/'third_party/nvn/nvn.h',own/'third_party/kgsl/msm_kgsl.h']:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 cbase=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-I'+str(own/'third_party/kgsl'),'-DNN_MEMORY_BACKEND="'+str(own/'tests/d2r_bridge/memory_backend.c')+'"','-DNN_STORAGE_NATIVE_ARTIFACT="'+shaders['cases'][0]['header']+'"','-DNN_STORAGE_PLATFORM="'+str(shader_check.parent/'device.platform.h')+'"']
 normal_commands=json.loads((fdl_check.parent/'commands.json').read_text());results=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  template=next(c for c in normal_commands if c['argv'][0]==cpp);a=template['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');cpp_flags=a[1:start]+['-I'+str(own/'include'),'-I'+str(own/'third_party/nvn')];libs=a[end:-2]
  for case in shaders['cases']:
   cbase=[a for a in cbase if not a.startswith('-DNN_STORAGE_NATIVE_ARTIFACT=')]+['-DNN_STORAGE_NATIVE_ARTIFACT="'+case['header']+'"']
   for enabled in [0,1]:
    definitions=['-DNN_PIPELINE_GRAPHICS_STORAGE='+str(enabled)]+([]if enabled else['-DNN_TEST_STORAGE_OFF=1']);objects=[]
    for i,src in enumerate(sources):
     obj=out/(cc+str(case['case'])+str(enabled)+'-'+str(i)+'.o');run([cc,*cbase,*definitions,*san,'-fno-pie','-c',own/src,'-o',obj]);objects.append(obj)
    # C++ metadata and packet helper, actual original FDL libraries.
    binary=out/(cc+'-'+str(case['case'])+'-'+str(enabled));run([cpp,*cpp_flags,*san,'-fno-pie','-no-pie',*[own/s for s in cpp_sources],*objects,*libs,'-Wl,--gc-sections','-o',binary],template['cwd']);result=json.loads(run([binary],template['cwd']).strip().splitlines()[-1]);result['case']=case['case'];result['enabled']=enabled;result['compiler']=cc;results.append(result)
  for t in (cc,cpp):p=Path(shutil.which(t)).resolve();inputs[str(p)]=sha(p)
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++';recipe=json.loads((fdl_check.parent/'source-closure/compile-recipes.json').read_text())[3];fs=recipe['flags'];native=out/'depth-native-api29.o';run([cpp,*fs,'-I'+str(own/'include'),'-Werror','-c',own/'src/depth_native.cc','-o',native],fdl_check.parent/'android-build')
 for case in shaders['cases']:
  cbase=[a for a in cbase if not a.startswith('-DNN_STORAGE_NATIVE_ARTIFACT=')]+['-DNN_STORAGE_NATIVE_ARTIFACT="'+case['header']+'"']
  for enabled in [0,1]:
   definitions=['-DNN_PIPELINE_GRAPHICS_STORAGE='+str(enabled)]+([]if enabled else['-DNN_TEST_STORAGE_OFF=1']);objects=[]
   for i,src in enumerate(sources):
    obj=out/('ndk'+str(case['case'])+str(enabled)+'-'+str(i)+'.o');run([cc,*cbase,*definitions,'-c',own/src,'-o',obj]);objects.append(obj)
   run([cc,*objects,native,depth['static_library'],fdl['static_library'],'-Wl,--gc-sections','-Wl,--no-undefined','-lm','-o',out/('api29-'+str(case['case'])+'-'+str(enabled)+'-not-executed')])
 # Also link the same actual runtime C modules against the real KGSL backend.
 platform=out/'actual-kgsl.o';run([cc,*cbase,'-fPIC','-c',own/'src/kgsl.c','-o',platform]);real=[]
 for i,src in enumerate(sources[:-1]):
  obj=out/('actual'+str(i)+'.o');run([cc,*cbase,'-fPIC','-DNN_PIPELINE_GRAPHICS_STORAGE=1','-c',own/src,'-o',obj]);real.append(obj)
 run([cc,'-shared',*real,platform,native,depth['static_library'],fdl['static_library'],'-Wl,--gc-sections','-Wl,--no-undefined','-lm','-o',out/'libdepth-runtime-real-kgsl.so'])
 for t in [cc,cpp,tools/'ld.lld']:inputs[str(t)]=sha(t)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed during checks '+p)
 active=[r for r in results if r['enabled']];inactive=[r for r in results if not r['enabled']]
 if len(active)!=8 or len(inactive)!=8 or any(r['jobs']!=2 for r in active)or any(r['jobs']for r in inactive)or any(a['checks']!=b['checks']for a,b in zip(active[:4],active[4:])):raise ValueError('HOST result mismatch')
 result=dict(schema=1,kind='native-graphics-storage-host-v1',passed=True,host_only=True,android_execution=False,gpu_execution=False,host=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Actual authored VS/FS storage read/write/size/sparse32table roles + FDL/native encoder; HOST RAM/owner/fault/transaction checks and nonexecuted API29 links only, not GPU/publicNVN/game')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','shader-check','fdl-root','fdl-check','depth-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k).resolve()for k in ['root','shader_check','fdl_root','fdl_check','depth_check','ndk','out']]))
