#!/usr/bin/env python3
"""HOST ONLY public depth metadata, real core ownership/fault checks, API29 link. No GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,runtime,archive,fdl_check,depth_check,a2d_check,kernel_check,bridge_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={};records={}
 def record(path):
  d=json.loads(path.read_text());inputs[str(path)]=sha(path)
  for group in ('inputs','artifacts','outputs'):
   for p,h in d.get(group,{}).items():
    if sha(p)!=h:raise ValueError('changed input '+p)
    inputs[p]=h
  return d
 for k,p in [('archive',archive),('fdl',fdl_check),('depth',depth_check),('a2d',a2d_check),('block',kernel_check),('bridge',bridge_check)]:records[k]=record(p)
 ar=records['archive'];assert ar['publish_depth_recipes']
 # Pin the complete runtime and all compiler/tool inputs before compilation.
 # The final recheck cannot bless a source first observed after it was used.
 for folder in ['src','include','tests/d2r_bridge','adapters/d2r','android','third_party']:
  for path in (runtime/folder).rglob('*'):
   if path.is_file()and path.suffix in ('.h','.c','.cc','.inc'):inputs[str(path)]=sha(path)
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin'
 for path in [Path(__file__).resolve(),fdl_check.parent/'commands.json',*[Path(shutil.which(t)).resolve()for t in ['gcc','g++','clang','clang++']],*[tools/t for t in ['aarch64-linux-android29-clang','aarch64-linux-android29-clang++','clang','ld.lld']]]:
  inputs[str(path)]=sha(path)
 block=mod(root/'nvn-native/tools/compile_block_copy.py','depth_block').verify(root,kernel_check)
 fdl=records['fdl'];host=Path(fdl['normal_ir3'])/'build'
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));p=subprocess.run(a,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError((p.stdout+p.stderr)[-10000:])
  return p.stdout
 rows=records['bridge']['views'];unique=[]
 for r in rows:
  if not any((r['width'],r['height'],r['pitch'])==(x['width'],x['height'],x['pitch'])for x in unique):unique.append(r)
 views=out/'actual-views.h';views.write_text('#include "nvn_native/texture_native_view.h"\nstatic const struct nn_tex_native_view NN_TEXTURE_NATIVE_VIEWS[]={'+','.join('{'+','.join('.'+k+'='+('{' + ','.join(map(str,v)) + '}'if isinstance(v,list)else str(int(v)))for k,v in r.items())+'}'for r in unique)+'};\n#define NN_TEXTURE_NATIVE_VIEW_COUNT (sizeof(NN_TEXTURE_NATIVE_VIEWS)/sizeof(*NN_TEXTURE_NATIVE_VIEWS))\n')
 inputs[str(views)]=sha(views)
 cbase=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(runtime/'third_party/nvn'),'-I'+str(runtime/'include'),'-I'+str(runtime/'include/nvn_native'),'-I'+str(runtime/'third_party/kgsl'),'-I'+str(runtime/'tests/d2r_bridge')]
 definitions=['-DNN_SAMPLED_PAIR=0']+['-D'+k+'=1'for k in ['NN_NVN_GRAPHICS_SOURCE_CACHE','NN_PIPELINE_DEPTH_SAMPLED','NN_NVN_DEPTH_SAMPLED','NN_NVN_BLOCK_COPY','NN_NVN_COLOR_TARGETS','NN_PIPELINE_COLOR_TARGETS','NN_NVN_DEPTH_TARGETS','NN_PIPELINE_DEPTH_NATIVE','NN_PIPELINE_SAMPLED_IMAGES','NN_NVN_SAMPLED_IMAGES','NN_PIPELINE_GATHER','NN_PIPELINE_RUNTIME_VIEWS','NN_PIPELINE_BC_SAMPLED','NN_PIPELINE_BUFFER_SAMPLED','NN_NVN_BUFFER_SAMPLED']]
 for k,p in [('NN_TEST_NVN_SOURCE',runtime/'tests/d2r_bridge/pipeline_enqueue_instrumented.h'),('NN_ENQUEUE_CORE_SOURCE',runtime/'src/nvn.c'),('NN_MEMORY_BACKEND_SOURCE',runtime/'tests/d2r_bridge/pipeline_bc_memory.c'),('NN_REAL_MEMORY_BACKEND',runtime/'tests/d2r_bridge/memory_backend.c'),('NN_SOURCE_FIXTURE',runtime/'tests/d2r_bridge/pipeline_nvn_source.c'),('NN_PIPELINE_ARCHIVE',archive.parent/'offline_graphics_pipelines.h'),('NN_PIPELINE_NATIVE_VIEWS',views),('NN_PIPELINE_TEXTURE_BRIDGE',bridge_check.parent/'texture_bridge_bundle.h'),('NN_BLOCK_IMAGE',Path(block['header']))]:definitions.append('-D'+k+'='+json.dumps(str(p)))
 names=['compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_bc_shadow','sampled_image','texture_buffer_sample','pipeline_views','color_target','color_target_shadow','depth_attachment','depth_transfer','depth_shadow','pipeline_depth_archive','depth_sampled','depth_sampled_native']
 sources=[runtime/'src'/(n+'.c')for n in names];cppsources=[runtime/'src'/(n+'.cc')for n in ['runtime_fdl','depth_fdl','depth_a2d','depth_native']];fixture=runtime/'tests/d2r_bridge/public_depth_sampled_source.c'
 normal=json.loads((fdl_check.parent/'commands.json').read_text());results=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/cc;folder.mkdir();template=next(t for t in normal if t['argv'][0]==cpp);a=template['argv'];start=next(i for i,x in enumerate(a)if x.startswith('-I')and x.endswith('/nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start];libs=a[end:-2];objects=[]
  for i,s in enumerate([fixture,*sources]):
   o=folder/(str(i)+'.o');run([cc,*cbase,*definitions,*san,'-fno-pie','-c',s,'-o',o]);objects.append(o)
  binary=folder/'public-depth';run([cpp,*flags,'-I'+str(runtime/'include'),'-I'+str(runtime/'third_party/nvn'),*san,'-fno-pie','-no-pie',*cppsources,*objects,*libs,'-Wl,--gc-sections','-o',binary],template['cwd']);sampled_result=run([binary])
  cascf=runtime/'tests/d2r_bridge/public_depth_gather_source.c';casco=folder/'casc.o';run([cc,*cbase,*definitions,*san,'-fno-pie','-c',cascf,'-o',casco]);cascbin=folder/'public-depth-casc';run([cpp,*flags,'-I'+str(runtime/'include'),'-I'+str(runtime/'third_party/nvn'),*san,'-fno-pie','-no-pie',*cppsources,casco,*objects[1:],*libs,'-Wl,--gc-sections','-o',cascbin],template['cwd']);casc_result=run([cascbin]);results.append(dict(sampled=sampled_result,casc=casc_result))
 if results[0]!=results[1]:raise ValueError('HOST sanitizer results differ')
 if any(r['sampled'].count('mock_submits=3')!=3 or r['casc'].count('mock_submits=2')!=9 for r in results):raise ValueError('incomplete 12 public depth cases')
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++';recipe=json.loads((fdl_check.parent/'source-closure/compile-recipes.json').read_text())[3];native=out/'depth-native.o';run([cpp,*recipe['flags'],'-I'+str(runtime/'include'),'-Werror','-c',runtime/'src/depth_native.cc','-o',native],fdl_check.parent/'android-build')
 libs=[native,records['a2d']['static_library'],records['depth']['static_library'],fdl['static_library']]
 actual=[runtime/'src/nvn.c',*sources,runtime/'src/kgsl.c',*[runtime/'android'/(s+'.c')for s in ['window_session','window_android','present','present_pacer','window_display','ahb']]]
 run([cc,*cbase,*definitions,'-shared',*actual,*libs,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'libdepth-sampled-enqueue-real-api29.so'])
 run([cc,*cbase,*definitions,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*sources,*libs,'-Wl,--gc-sections','-Wl,--no-undefined','-landroid','-lm','-o',out/'public-depth-fixture-api29-not-run'])
 run([cc,*cbase,*definitions,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',runtime/'tests/d2r_bridge/public_depth_gather_source.c',*sources,*libs,'-Wl,--gc-sections','-Wl,--no-undefined','-landroid','-lm','-o',out/'public-depth-casc-api29-not-run'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed '+p)
 proof=dict(schema=1,kind='public-nvn-depth-sampled-host-v1',passed=True,gpu_execution=False,android_execution=False,host_sanitizers={'gcc':['address','undefined'],'clang':['function-trap']},host_results=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','runtime','archive','fdl-check','depth-check','a2d-check','kernel-check','bridge-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k.replace('-','_')).resolve()for k in ['root','runtime','archive','fdl-check','depth-check','a2d-check','kernel-check','bridge-check','ndk','out']]))
