#!/usr/bin/env python3
"""Normal original source/API/Call runtime constants HOST owner test, not GPU/game."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('graphics_storage_normal_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,pipeline_check,fdl_root,fdl_check,depth_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 n=root/'nvn-native';fdl_tool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';_,fdl=module(fdl_tool).verify(fdl_root,fdl_check);inputs=dict(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check)
 def checked(path,kind):
  c=json.loads(path.read_text());inputs[str(path)]=sha(path)
  if c.get('kind')!=kind or c.get('gpu_execution')is not False:raise ValueError('wrong normal record')
  for group in ('inputs','artifacts','outputs'):
   for f,h in c.get(group,{}).items():
    if sha(f)!=h:raise ValueError('source/product changed '+f)
    inputs[f]=h
  return c
 pipeline=checked(pipeline_check,'native-graphics-pipeline-recipes-v1');depth=checked(depth_check,'native-depth-fdl-foundation-v1')
 if not pipeline['publish_runtime_constants']or not pipeline['publish_storage_recipes']or len(pipeline['pairs'])!=25:raise ValueError('need actual 25-original-source runtime-constant catalogue')
 modules=[n/'src'/(s+'.c')for s in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_bc_shadow','sampled_image','texture_buffer_sample','pipeline_storage','pipeline_runtime_constants')]
 fixture=n/'tests/d2r_bridge/pipeline_runtime_constants.c';include=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'third_party/kgsl')]
 defs=['-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_NVN_GRAPHICS_STORAGE=1','-DNN_PIPELINE_GRAPHICS_STORAGE=1','-DNN_NVN_GRAPHICS_RUNTIME_CONSTANTS=1','-DNN_PIPELINE_RUNTIME_CONSTANTS=1']
 for macro,path in [('NN_TEST_NVN_SOURCE',n/'src/nvn.c'),('NN_MEMORY_BACKEND_SOURCE',n/'tests/d2r_bridge/pipeline_draw_memory.c'),('NN_REAL_MEMORY_BACKEND',n/'tests/d2r_bridge/memory_backend.c'),('NN_SOURCE_FIXTURE',n/'tests/d2r_bridge/pipeline_nvn_source.c'),('NN_PIPELINE_ARCHIVE',pipeline_check.parent/'offline_graphics_pipelines.h')]:defs.append('-D'+macro+'='+json.dumps(str(path)))
 for folder in ('src','include','third_party','tests/d2r_bridge'):
  for f in (n/folder).rglob('*'):
   if f.is_file()and f.suffix in ('.c','.cc','.h','.inc'):inputs[str(f)]=sha(f)
 inputs[str(Path(__file__).resolve())]=sha(__file__)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 normal=json.loads((fdl_check.parent/'commands.json').read_text());results=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  objs=[]
  for i,f in enumerate([fixture,*modules]):
   o=out/(cc+'-'+str(i)+'.o');run([cc,*include,*defs,*san,'-fno-pie','-c',f,'-o',o]);objs.append(o)
  template=next(x for x in normal if x['argv'][0]==cpp);a=template['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start]+['-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')];libs=a[end:-2]
  binary=out/cc;run([cpp,*flags,*san,'-fno-pie','-no-pie',n/'src/runtime_fdl.cc',n/'src/depth_fdl.cc',n/'src/depth_native.cc',*objs,*libs,'-Wl,--gc-sections','-o',binary],template['cwd']);result=json.loads(run([binary]).strip().splitlines()[-1]);results.append(result)
  for tool in (cc,cpp):f=Path(shutil.which(tool)).resolve();inputs[str(f)]=sha(f)
 if results[0]!=results[1]or results[0]['original_pairs']!=25 or results[0]['mock_submits']!=150 or results[0]['mock_failed_submits']!=50 or results[0]['negative_cases']!=10:raise ValueError('incomplete original storage family')
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++';recipe=json.loads((fdl_check.parent/'source-closure/compile-recipes.json').read_text())[3];o=out/'depth-native.o';run([cpp,*recipe['flags'],'-I'+str(n/'include'),'-Werror','-c',n/'src/depth_native.cc','-o',o],fdl_check.parent/'android-build')
 real=[n/'src/nvn.c',n/'src/kgsl.c',*modules,*[n/'android'/(s+'.c')for s in ('window_session','window_android','present','present_pacer','window_display','ahb')]]
 run([cc,*include,*defs,'-fPIC','-shared',*real,o,depth['static_library'],fdl['static_library'],'-Wl,--no-undefined','-Wl,--gc-sections','-landroid','-ldl','-lm','-o',out/'libgraphics-storage-real-api29.so'])
 run([cc,*include,*defs,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*modules,o,depth['static_library'],fdl['static_library'],'-Wl,--no-undefined','-Wl,--gc-sections','-lm','-o',out/'graphics-storage-api29-not-executed'])
 for tool in (cc,cpp,tools/'ld.lld'):inputs[str(tool.resolve())]=sha(tool)
 for f,h in inputs.items():
  if sha(f)!=h:raise ValueError('source/product changed during checks '+f)
 record=dict(schema=1,kind='public-original-source-runtime-constants-host-v1',passed=True,host_only=True,gpu_execution=False,android_execution=False,results=results,commands=commands,inputs=inputs,artifacts={str(f):sha(f)for f in out.iterdir()if f.is_file()},scope='25 unchanged original VS/fullcontrol/C1 and fixed original diagnostic FS, explicit authored specialization; Actual driver BaseInstance17/2instances, native residual C1/API full-range bindings + storage API4/5 mapping, unused unqualified bindings, Call inherited state and retirement only, not observed game Program pairing or GPU qualification');(out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','pipeline-check','fdl-root','fdl-check','depth-check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k).resolve()for k in ('root','pipeline_check','fdl_root','fdl_check','depth_check','ndk','out')]))
