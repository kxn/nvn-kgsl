#!/usr/bin/env python3
"""HOST-only TG4 consumer contract/public descriptors, normal native recipe and real NDK link."""
import argparse,copy,json,os,shutil,subprocess
from pathlib import Path
from package_graphics_pipelines import compiler_record,texture_types,checked,sha
from package_graphics_stages import build as stages,gather_bindings,texture_bindings
from texture_bridge_bundle import native_views

def check(root,native_check,actual_check,legacy_check,ndk,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 out.mkdir(parents=True);inputs={str(Path(__file__).resolve()):sha(__file__)};commands=[];proof=dict(schema=1,kind='pipeline-gather-consumer-host-v1',passed=False,gpu_execution=False,android_execution=False,default_enabled=False)
 def run(argv,cwd=None):
  argv=list(map(str,argv));x=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if x.returncode:raise ValueError('HOST/NDK failed: '+x.stderr[-5000:])
  return x.stdout
 def record(path):
  d=json.loads(path.read_text());inputs[str(path)]=sha(path);checked(d['inputs'],inputs);checked(d.get('artifacts',d.get('outputs',{})),inputs);return d
 try:
  d=record(native_check)
  if d.get('kind')!='native-authored-color-gather-v1'or d.get('gpu_execution')is not False:raise ValueError('unknown native Gather fixture')
  compiler=Path(d['compiler_record']).parent;_,_,cc_record=compiler_record(compiler,n,inputs)
  actual=record(actual_check)
  if actual.get('kind')!='offline-graphics-stage-compilation-v1'or actual.get('gpu_execution')is not False or len(actual['pairs'])!=1 or not actual['pairs'][0]['passed']:raise ValueError('actual original Gather pair absent')
  pair=actual['pairs'][0];program=pair['program'];g=gather_bindings(program['fs']['nir_usage'])
  if not g or program['fs']['nir_usage']['resource_semantics']['bindings'][0]['other']!=1 or program['fs']['nir_usage']['resource_semantics']['bindings'][0]['samples']:raise ValueError('original Gather masquerades as ordinary sample')
  # Complete original source/control/C1 archive remains unchanged; its real
  # FragDepth pair is not relabeled into a color-only pipeline recipe.
  _,manifest,_=stages(actual_check,out/'actual-source-archive',root=root);checked(manifest['inputs'],inputs);checked(manifest['outputs'],inputs)
  fs=actual['stages'][pair['fs']];mapping=texture_bindings(fs['resources'])
  if len(mapping)!=1:raise ValueError('original Gather source resource count')
  mapping=mapping[0];row=program['fs']['nir_usage']['resource_semantics']['bindings'][0]
  if row['binding']!=mapping['binding']:raise ValueError('native/source binding mismatch')
  mapping.update(use_known=1,**{k:row[k]for k in ('fetches','size_queries','samples','offsets','other')})
  source_map=out/'source-map.h';source_map.write_text('#include "nvn_native/offline_graphics.h"\nstatic const struct nn_offline_texture_binding NN_GATHER_SOURCE_BINDING={'+','.join('.'+k+'='+str(mapping[k])for k in ('binding','set','type','format','array_length','flags','separate','handle_word','cbuf_slot','use_known','fetches','size_queries','samples','offsets','other'))+'};\n')
  # Existing original Mesa FDL caller and actual build flags/libraries, not a
  # handwritten native view or an unrelated stale compiler package.
  views,vinputs,_=native_views({'compiler_build':cc_record},compiler,out,[(64,64)]);checked(vinputs,inputs)
  v=views[0];view_header=out/'gather-views.h';view_header.write_text('#include "nvn_native/texture_native_view.h"\nstatic const struct nn_tex_native_view NN_TEXTURE_NATIVE_VIEWS[]={'+ '{'+','.join('.'+k+'='+('{' + ','.join(map(str,x)) + '}'if isinstance(x,list)else str(int(x)))for k,x in v.items())+'}};\n')
  prefix=Path(d['header']).parent/'gather';native_program=json.loads(Path(str(prefix)+'.link.json').read_text());recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text());facts=json.loads(Path(str(prefix)+'.pipeline.types.json').read_text())
  if texture_types(facts,recipe,native_program)!=d['native_texture_types']:raise ValueError('normal native types differ')
  negatives=[]
  def reject(name,change):
   a,b,c=copy.deepcopy(facts),copy.deepcopy(recipe),copy.deepcopy(native_program);change(a,b,c)
   try:texture_types(a,b,c)
   except (ValueError,KeyError,TypeError):negatives.append(name)
   else:raise ValueError('invalid TG4 evidence accepted: '+name)
  for field,value in [('count',0),('count',2),('count',True),('component_mask',0),('component_mask',16),('semantic_sampler_mask',8),('offsets',1)]:
   reject('gather-row-'+field+'-'+str(value),lambda a,b,c,f=field,v=value:c['fs']['nir_usage']['gather_usage']['bindings'][0].__setitem__(f,v))
  for field,value in [('schema',True),('count',2),('basis','SPIR-V declaration'),('coordinate_domain','integer'),('lod_mode','implicit_lod')]:
   reject('gather-'+field,lambda a,b,c,f=field,v=value:c['fs']['nir_usage']['gather_usage'].__setitem__(f,v))
  reject('missing-gather',lambda a,b,c:c['fs']['nir_usage'].pop('gather_usage'))
  for field,value in [('other',0),('other',2),('samples',1),('offsets',1)]:
   reject('resource-'+field,lambda a,b,c,f=field,v=value:c['fs']['nir_usage']['resource_semantics']['bindings'][0].__setitem__(f,v))
  for dim in (1,3):reject('native-dim'+str(dim),lambda a,b,c,v=dim:a['dimensions'].__setitem__(1,v))
  for op in (1,8,24):reject('operation'+str(op),lambda a,b,c,v=op:a['operations'].__setitem__(1,v))
  reject('missing-semantic-sampler',lambda a,b,c:(a.__setitem__('sampler_mask',0),b.__setitem__('semantic_sampler_mask',0)))
  reject('bool-schema',lambda a,b,c:a.__setitem__('schema',True))
  # Old0/1/2 texture artifact tables use the unextended designated initializer;
  # normal package regression is produced separately with these same tools.
  old=record(legacy_check)
  if old.get('kind')!='native-graphics-pipeline-recipes-v1'or len(old['pairs'])!=3:raise ValueError('old0/1/2 package required')
  for i,pair in enumerate(old['pairs']):
   q=legacy_check.parent/('pair-'+str(i));a=json.loads(Path(str(q)+'.link.json').read_text());b=json.loads(Path(str(q)+'.pipeline.json').read_text());c=json.loads(Path(str(q)+'.pipeline.types.json').read_text())
   if texture_types(c,b,a)!=pair['native_texture_types']or 'gather_usage'in pair['native_texture_types']:raise ValueError('legacy type semantics changed')
  modules=[n/'src'/(name+'.c')for name in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline','pipeline_archive','pipeline_source','pipeline_resource_queries','pipeline_frontend','pipeline_vertex')]
  fixture=n/'tests/d2r_bridge/pipeline_gather.c'
  for folder in ('src','include','third_party','android','tests/d2r_bridge'):
   for f in (n/folder).rglob('*'):
    if f.is_file()and f.suffix in ('.c','.h','.inc','.cc'):inputs[str(f.resolve())]=sha(f)
  for f in (n/'tools/package_graphics_pipelines.py',n/'tools/package_graphics_stages.py',n/'tools/texture_bridge_bundle.py'):inputs[str(f)]=sha(f)
  common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'src')]
  defs=['-DNN_NVN_GRAPHICS_SOURCE_CACHE=1']
  for k,v in {'NN_MEMORY_BACKEND_SOURCE':n/'tests/d2r_bridge/pipeline_bc_memory.c','NN_REAL_MEMORY_BACKEND':n/'tests/d2r_bridge/memory_backend.c','NN_TEST_NVN_SOURCE':n/'src/nvn.c','NN_GATHER_ARTIFACT':Path(d['header']),'NN_GATHER_VIEWS':view_header,'NN_GATHER_SOURCE_MAP':source_map}.items():defs.append('-D'+k+'='+json.dumps(str(v)))
  host=[]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   inputs[str(Path(shutil.which(cc)).resolve())]=sha(Path(shutil.which(cc)).resolve())
   folder=out/cc;folder.mkdir();objects=[]
   for i,source in enumerate(modules):
    obj=folder/(str(i)+'.o');run([cc,*common,*san,'-DNN_PIPELINE_GATHER=1','-c',source,'-o',obj]);objects.append(obj)
   for mode in (0,1):
    # pipeline.c contains the separate native runtime gate. Recompile it OFF
    # as well so an inactive public path cannot mask a changed native default.
    obj=folder/('pipeline-'+str(mode)+'.o');run([cc,*common,*san,*(['-DNN_PIPELINE_GATHER=1']if mode else []),'-c',n/'src/pipeline.c','-o',obj]);selected=[obj if source.name=='pipeline.c'else o for source,o in zip(modules,objects)]
    exe=folder/('gather-'+str(mode));run([cc,*common,*defs,*san,*(['-DNN_PIPELINE_GATHER=1']if mode else []),fixture,*selected,'-lm','-o',exe]);text=run([exe]);host.append(text)
    if 'actual_GPU_submits=0'not in text or 'enabled='+str(mode)not in text:raise ValueError('HOST scope differs')
  cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
  for p in (cc.resolve(),readelf):inputs[str(p)]=sha(p)
  android=[n/'android'/(s+'.c')for s in ('window_session','window_android','present','present_pacer','window_display','ahb')]
  for mode in (0,1):
   lib=out/('libgather-'+str(mode)+'-api29.so');run([cc,*common,'-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*(['-DNN_PIPELINE_GATHER=1']if mode else []),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,*android,'-landroid','-ldl','-lm','-o',lib]);elf=run([readelf,'-d',lib])
   if any(x in elf.lower()for x in ('libvulkan','libegl','libgles','metal')):raise ValueError('renderer dependency appeared')
  checked(inputs,{})
  proof.update(passed=True,producer_negative_cases=len(negatives),negatives=negatives,HOST_results=host,normal_source_archive=str(out/'actual-source-archive/archive-manifest.json'),source_other_retained=True,actual_pair_requires_depth_recipe=True,native_binding_is_API_selector=False,scope='Normalized floating2D no-offset level0 TG4; public RGBA8 singlemip only, native/metadata HOST and NDK, no GPU/D2R rendering qualification',inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 except Exception as e:proof.update(error=str(e),inputs=inputs)
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json',proof['passed']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','native-check','actual-stage-check','legacy-check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();path,ok=check(a.root.resolve(),a.native_check.resolve(),a.actual_stage_check.resolve(),a.legacy_check.resolve(),a.ndk.resolve(),a.out.resolve());print(json.dumps(dict(passed=ok,proof=str(path))));raise SystemExit(0 if ok else 1)
