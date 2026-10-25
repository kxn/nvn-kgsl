#!/usr/bin/env python3
"""HOST ONLY public NVN original FLOAT2D sampled family metadata/owners; actual Android link, no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,runtime,sampled_check,bridge_check,fdl_check,kernel_check,gather_check,actual_gather_check,ndk,out,mip_filter=False):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=root/'nvn-native';normal=module(native/'tools/build_runtime_fdl.py','bc_enqueue_fdl')
 fd=json.loads(fdl_check.read_text());origins=[Path(p).parents[2]for p in fd['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('ambiguous FDL source')
 library,fdl=normal.verify(origins[0],fdl_check)
 for rel in ('src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','include/nvn_native/texture_buffer_format.h','tests/runtime_fdl.cc','tools/build_runtime_fdl.py'):
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(runtime/rel)for p,h in fdl['inputs'].items()):raise ValueError('different FDL source '+rel)
 block=module(native/'tools/compile_block_copy.py','bc_enqueue_kernel').verify(root,kernel_check)
 inputs={str(fdl_check):sha(fdl_check),str(kernel_check):sha(kernel_check),**fdl['inputs'],**fdl['artifacts'],**block['inputs'],**block['artifacts']}
 for record in (sampled_check,bridge_check,gather_check,actual_gather_check):
  d=json.loads(record.read_text());inputs[str(record)]=sha(record)
  inputs.update(d['inputs']);inputs.update(d.get('outputs',d.get('artifacts',{})))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal source/product changed '+p)
 out.mkdir(parents=True);commands=[];results=[];family_results=[]
 def run(argv,cwd=None):
  p=subprocess.run(list(map(str,argv)),cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError((p.stdout+p.stderr)[-6000:])
  return p.stdout
 d=json.loads(sampled_check.read_text())
 run([shutil.which('python3'),native/'tools/package_graphics_pipelines.py','--root',root,'--stages',d['stage_check'],'--compiler',Path(d['compiler_record']).parent,'--gmem-bytes',d['gmem_bytes'],'--out',sampled_check.parent,'--verify'])
 bundle=bridge_check.parent/'texture_bridge_bundle.h'
 # Only distinct actual zero-address FDL views are needed by legacy fixtures.
 # The normal bridge may carry duplicate dimensions for different consumers;
 # registration intentionally rejects those duplicate lookup keys.
 rows=json.loads(bridge_check.read_text())['views'];unique=[]
 for row in rows:
  if not any((row['width'],row['height'],row['pitch'])==(r['width'],r['height'],r['pitch'])for r in unique):unique.append(row)
 views=out/'actual-distinct-views.h';views.write_text('#include "nvn_native/texture_native_view.h"\nstatic const struct nn_tex_native_view NN_TEXTURE_NATIVE_VIEWS[]={'+','.join('{'+','.join('.'+k+'='+('{' + ','.join(map(str,v)) + '}'if isinstance(v,list)else str(int(v)))for k,v in r.items())+'}'for r in unique)+'};\n#define NN_TEXTURE_NATIVE_VIEW_COUNT (sizeof(NN_TEXTURE_NATIVE_VIEWS)/sizeof(*NN_TEXTURE_NATIVE_VIEWS))\n')
 gather=json.loads(gather_check.read_text());actual=json.loads(actual_gather_check.read_text())
 if gather.get('kind')!='native-authored-color-gather-v1' or actual.get('kind')!='offline-graphics-stage-compilation-v1' or len(actual['pairs'])!=1:raise ValueError('normal actual Gather facts missing')
 pair=actual['pairs'][0];fs=actual['stages'][pair['fs']];st=module(native/'tools/package_graphics_stages.py','sampled_gather_source')
 mapping=st.texture_bindings(fs['resources'])
 if len(mapping)!=1:raise ValueError('ambiguous actual Gather resource')
 mapping=mapping[0];row=pair['program']['fs']['nir_usage']['resource_semantics']['bindings'][0]
 if row['binding']!=mapping['binding'] or row['other']!=1 or row['samples']:raise ValueError('Gather fact was changed to sample')
 mapping.update(use_known=1,**{k:row[k]for k in ('fetches','size_queries','samples','offsets','other')})
 source_map=out/'actual-gather-source-map.h';source_map.write_text('#include "nvn_native/offline_graphics.h"\nstatic const struct nn_offline_texture_binding NN_GATHER_SOURCE_BINDING={'+','.join('.'+k+'='+str(mapping[k])for k in ('binding','set','type','format','array_length','flags','separate','handle_word','cbuf_slot','use_known','fetches','size_queries','samples','offsets','other'))+'};\n')
 modules=[runtime/'src'/(s+'.c')for s in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_bc_shadow','sampled_image','texture_buffer_sample')]
 fixture=runtime/'tests/d2r_bridge/pipeline_enqueue_sampled_images.c';facade=runtime/'src/runtime_fdl.cc'
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
 defines=['-DNN_SAMPLED_PAIR=0','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_NVN_BLOCK_COPY=1','-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_NVN_SAMPLED_IMAGES=1','-DNN_NVN_BUFFER_SAMPLED=1','-DNN_PIPELINE_BUFFER_SAMPLED=1','-DNN_PIPELINE_GATHER=1']+(['-DNN_PIPELINE_SAMPLED_MIP_FILTER=1']if mip_filter else [])
 for macro,path in [('NN_TEST_NVN_SOURCE',runtime/'tests/d2r_bridge/pipeline_enqueue_instrumented.h'),('NN_ENQUEUE_CORE_SOURCE',runtime/'src/nvn.c'),('NN_MEMORY_BACKEND_SOURCE',runtime/'tests/d2r_bridge/pipeline_bc_memory.c'),('NN_REAL_MEMORY_BACKEND',runtime/'tests/d2r_bridge/memory_backend.c'),('NN_SOURCE_FIXTURE',runtime/'tests/d2r_bridge/pipeline_nvn_source.c'),('NN_PIPELINE_ARCHIVE',sampled_check.parent/'offline_graphics_pipelines.h'),('NN_PIPELINE_NATIVE_VIEWS',views),('NN_PIPELINE_TEXTURE_BRIDGE',bundle),('NN_BLOCK_IMAGE',Path(block['header'])),('NN_GATHER_ARTIFACT',Path(gather['header'])),('NN_GATHER_VIEWS',views),('NN_GATHER_SOURCE_MAP',source_map)]:defines.append('-D'+macro+'='+json.dumps(str(path)))
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objs=[]
  for tool in (compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate([fixture,*modules]):
   o=folder/(str(i)+'.o');run([compiler,*include,*defines,*san,'-fno-pie','-c',s,'-o',o]);objs.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(runtime/'include'),'-Werror',*san,'-fno-pie','-c',facade,'-o',o],host);objs.append(o)
  binary=folder/'bc-enqueue';run([cpp,'-no-pie',*san,*objs,*libs,'-o',binary],host);text=run([binary]);results.append(text)
  if text.count('GPU execution=false')!=176 or text.count('mock_submits=2')!=176:raise ValueError('incomplete 176 format/mip/view/pool owner fixture')
  fetch=folder/'fetch.o';run([compiler,*include,*defines,*san,'-fno-pie','-c',runtime/'tests/d2r_bridge/pipeline_enqueue_sampled_fetch.c','-o',fetch])
  fb=folder/'fetch';run([cpp,'-no-pie',*san,fetch,*objs[1:],*libs,'-o',fb],host);ft=run([fb])
  if ft.count('GPU execution=false')!=88 or ft.count('mock_submits=5')!=88:raise ValueError('incomplete88 original FETCH/SIZE public cases')
  (folder/'fetch-results.txt').write_text(ft)
  go=folder/'gather.o';run([compiler,*include,*defines,*san,'-fno-pie','-c',runtime/'tests/d2r_bridge/pipeline_gather_sampled_images.c','-o',go])
  gb=folder/'gather';run([cpp,'-no-pie',*san,go,*objs[1:],*libs,'-o',gb],host);gt=run([gb])
  if gt.count('actual_GPU_submits=0')!=88:raise ValueError('incomplete88 real public descriptors/raw-native Gather cases')
  (folder/'gather-results.txt').write_text(gt)
  offdefines=[d for d in defines if d!='-DNN_NVN_SAMPLED_IMAGES=1']
  off=folder/'fetch-off.o';run([compiler,*include,*offdefines,*san,'-fno-pie','-c',runtime/'tests/d2r_bridge/pipeline_enqueue_sampled_fetch.c','-o',off])
  ob=folder/'fetch-off';run([cpp,'-no-pie',*san,off,*objs[1:],*libs,'-o',ob],host);ot=run([ob])
  if ot.count('mock_submits=0')!=88:raise ValueError('public sampled defaultOFF admitted new role')
  (folder/'off-results.txt').write_text(ot)
  # Legacy BC role, including true mip-view selection, remains unchanged even
  # when a new sampled-only role exists in the common state object.
  bcdefs=[d for d in defines if d!='-DNN_NVN_SAMPLED_IMAGES=1']+['-DNN_PIPELINE_BC_SAMPLED=1']
  bp=folder/'pipeline-bc.o';run([compiler,*include,*bcdefs,*san,'-fno-pie','-c',runtime/'src/pipeline.c','-o',bp])
  bmods=[bp if module==runtime/'src/pipeline.c' else obj for module,obj in zip(modules,objs[1:-1])]+[objs[-1]]
  legacy=[]
  for fname,count in [('pipeline_enqueue_bc_sampled.c',16),('pipeline_enqueue_bc_mips.c',48)]:
   localdefs=[v for v in bcdefs if not v.startswith('-DNN_MEMORY_BACKEND_SOURCE=')]
   localdefs+=['-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(runtime/'tests/d2r_bridge'/('pipeline_bc_mip_memory.c'if fname.endswith('_mips.c')else'pipeline_bc_memory.c')))]
   o=folder/(fname+'.o');run([compiler,*include,*localdefs,*san,'-fno-pie','-c',runtime/'tests/d2r_bridge'/fname,'-o',o]);exe=folder/fname
   run([cpp,'-no-pie',*san,o,*bmods,*libs,'-o',exe],host);t=run([exe])
   if t.count('GPU execution=false')!=count or t.count('mock_submits=2')!=count:raise ValueError('legacy BC regression '+fname)
   legacy.append(t)
  family_results.append(dict(sample=text,fetch=ft,gather=gt,off=ot,legacy=legacy))
 if results[0]!=results[1]or family_results[0]!=family_results[1]:raise ValueError('sanitizer results differ')
 # Actual Android/KGSL module link, not executing the HOST RAM fixture as GPU.
 actual=[runtime/'src/kgsl.c',runtime/'src/nvn.c',*modules,*[runtime/'android'/(s+'.c')for s in ('window_session','window_android','present','present_pacer','window_display','ahb')]]
 run([cc,*include,*defines,'-I'+str(runtime/'third_party/kgsl'),'-fPIC','-shared',*actual,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'bc-enqueue-real-api29.so'])
 run([cc,*include,*defines,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*modules,library,'-Wl,--no-undefined','-landroid','-lm','-o',out/'bc-enqueue-fixture-api29'])
 for name in ('pipeline_enqueue_sampled_fetch.c','pipeline_gather_sampled_images.c'):
  run([cc,*include,*defines,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',runtime/'tests/d2r_bridge'/name,*modules,library,'-Wl,--no-undefined','-landroid','-lm','-o',out/(name+'-api29')])
 offdefines=[v for v in defines if v not in ('-DNN_NVN_SAMPLED_IMAGES=1','-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_PIPELINE_SAMPLED_MIP_FILTER=1')]
 run([cc,*include,*offdefines,'-I'+str(runtime/'third_party/kgsl'),'-fPIC','-shared',*actual,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'sampled-default-off-real-api29.so'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during check '+p)
 proof=dict(schema=1,kind='public-nvn-sampled-float2d-enqueue-host-v1',scope=__doc__,passed=True,gpu_execution=False,android_execution=False,sampled_normalization_known=True,normalization_policy="independent-public-RE-ordinary-2D-v1",sdk53_314_descriptor_observed=False,formats=22,sample_cases=176,fetch_size_cases=88,gather_descriptor_native_cases=88,legacy_BC_cases=64,public_default_off_cases=88,mip_filter=mip_filter,results=family_results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime','sampled-check','bridge-check','runtime-fdl-check','kernel-check','gather-check','actual-gather-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--mip-filter',action='store_true');a=p.parse_args();print(check(a.root.resolve(),a.runtime.resolve(),a.sampled_check.resolve(),a.bridge_check.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.gather_check.resolve(),a.actual_gather_check.resolve(),a.ndk.resolve(),a.out.resolve(),a.mip_filter))
