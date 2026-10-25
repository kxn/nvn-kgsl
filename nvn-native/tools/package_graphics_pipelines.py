#!/usr/bin/env python3
"""Build immutable shader recipes/typed specialization requirements, no Draw template.

Normal common IR3 compiler and actual source archive are required. All outputs
stay ignored. This package does not resolve actual NVN state or register a Draw
pipeline automatically. Unknown runtime query/resource semantics remain errors.
"""
import argparse,hashlib,json,math,os,re,struct,subprocess
from pathlib import Path
from workspace_paths import shader_frontend_root
from package_graphics_stages import build as stage_archive,gather_bindings

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def checked(mapping,inputs):
 for path,value in mapping.items():
  if sha(path)!=value:raise ValueError('changed dependency '+path)
  inputs[str(Path(path).resolve())]=value

def compiler_record(directory,native,inputs):
 p=directory/'build-record.json';d=json.loads(p.read_text());inputs[str(p)]=sha(p)
 policy=Path(__file__).with_name('workspace_paths.py');inputs[str(policy)]=sha(policy)
 manifest=directory/'source-hashes.json'
 if sha(manifest)!=d['source_manifest_sha256'] or sha(native/'tools/build_ir3.py')!=d['builder_sha256']:raise ValueError('compiler manifest/builder mismatch')
 inputs[str(manifest)]=sha(manifest);inputs[str(native/'tools/build_ir3.py')]=sha(native/'tools/build_ir3.py')
 checked(d['tools'],inputs);checked(d['artifacts'],inputs)
 for rel,value in d['source_rules'].items():checked({str(native/rel):value},inputs)
 inventory=json.loads(manifest.read_text());source=directory/'source/mesa-26.2.5'
 for rel,value in inventory.items():
  part=Path(rel)
  if part.is_absolute() or '..' in part.parts:raise ValueError('compiler source outside manifest root')
  f=source/part
  actual=hashlib.sha256(('symlink:'+os.readlink(f)).encode()).hexdigest() if f.is_symlink() else sha(f)
  if actual!=value:raise ValueError('changed compiler source '+rel)
 for rel,value in d['overlay'].items():
  if inventory.get(rel)!=value:raise ValueError('compiler overlay manifest mismatch')
  if rel.startswith('src/freedreno/ir3/nvn_'):
   name=Path(rel).name.removeprefix('nvn_')
   origin=native/('shader/'+name if name=='nir_bridge.h' or not name.endswith('.h') else 'include/nvn_native/'+name)
   checked({str(origin):value},inputs)
 if not (source/'src/freedreno/fdl/meson.build').read_bytes().endswith(b'\n'+(native/'shader/mesa-tool.meson').read_bytes()):raise ValueError('compiler target definitions changed')
 inputs[str(native/'shader/mesa-tool.meson')]=sha(native/'shader/mesa-tool.meson')
 required=directory/'build/src/freedreno/fdl/nvn-pipeline-ir3';platform=directory/'build/src/freedreno/fdl/nvn-pipeline-platform'
 if str(required) not in d['artifacts'] or str(platform) not in d['artifacts']:raise ValueError('normal pipeline compiler targets missing')
 return required,platform,d

def queries(reflection,semantic_usage=None):
 result=[];identities={}
 for q in reflection['queries']:
  if 'value' not in q:
   if q['name'] not in ('CreateTextureBinding','RegisterTexture','binding'):raise ValueError('unhandled non-query reflection event')
   continue
  if set(q)-{'name','value','handle','cbufSlot','slot','index'}:raise ValueError('unknown query arguments')
  if not isinstance(q['name'],str) or not q['name'] or len(q['name'])>128:raise ValueError('invalid query name')
  argnames=('handle','cbufSlot','slot','index');args=[q.get(n,0) for n in argnames];mask=sum(1<<i for i,n in enumerate(argnames)if n in q)
  if any(type(x)is not int or not -(1<<31)<=x<(1<<31) for x in args):raise ValueError('unknown query argument representation')
  values=q['value'] if isinstance(q['value'],list) else [q['value']]
  if not values or len(values)>32:raise ValueError('unknown query value shape')
  if q['name'] in ('graphics/point_size','graphics/alpha_test_reference'):
   kind=2
   if len(values)!=1 or type(values[0]) not in (int,float) or not math.isfinite(values[0]):raise ValueError('invalid float32 query')
   values=[struct.unpack('<I',struct.pack('<f',values[0]))[0]]
  elif all(type(x)is bool for x in values):kind=0;values=list(map(int,values))
  elif all(type(x)is int and -(1<<31)<=x<(1<<32) for x in values):kind=1;values=[x&0xffffffff for x in values]
  else:raise ValueError('unsupported query result representation')
  key=(q['name'],mask,tuple(args));value=(kind,tuple(values))
  if key in identities:
   if identities[key]!=value:raise ValueError('inconsistent repeated compiler query')
   continue
  identities[key]=value;result.append({'name':q['name'],'arguments':mask,'args':args,'type':kind,'values':values})
 reductions=[]
 coverage=next((x for x in result if x['name']=='graphics/alpha_to_coverage_enable'),None)
 dither=next((x for x in result if x['name']=='graphics/alpha_to_coverage_dither_enable'),None)
 if coverage and dither and coverage['type']==0 and coverage['values']==[0]:
  result.remove(dither);reductions.append({'raw_query':dither,'required_condition':coverage,
   'rule':'ShaderDefinitions.AlphaToCoverageDitherEnable = AlphaToCoverageEnable && rawDither; false coverage retained, raw dither not a shader dependency'})

 # This removes an unconsumed *requirement*, never supplies an invented runtime
 # coordinate-normalization answer. Only actual common-NIR instruction facts
 # for every matching source identity may justify the bounded reduction.
 resources=(semantic_usage or {}).get('resource_semantics',{})
 if resources.get('schema')==1:
  native={}
  for entry in resources.get('bindings',[]):
   b=entry.get('binding')
   if type(b)is not int or not 0<=b<16 or b in native:raise ValueError('invalid NIR resource binding facts')
   for name in ('fetches','size_queries','samples','offsets','other'):
    if type(entry.get(name))is not int or entry[name]<0:raise ValueError('invalid NIR resource operation facts')
   native[b]=entry
  for raw in result[:]:
   if raw['name']!='QueryTextureCoordNormalized' or raw['arguments']!=3 or raw['type']!=0 or len(raw['values'])!=1:continue
   entries=[x for x in reflection['textures']if x['HandleIndex']==raw['args'][0] and x['CbufSlot']==raw['args'][1]]
   evidence=[]
   for entry in entries:
    fact=native.get(entry['Binding'])
    if entry['Set']!=0 or entry['ArrayLength']!=1 or entry['Separate'] or not fact or fact['samples'] or fact['other'] or not fact['fetches']+fact['size_queries']:break
    # TXF/TXS are the existing IR operations with integral/no coordinates.
    # Integer TXF offsets do not change the early IntCoords return. Mixed
    # sampling or other consumers remain real unknown requirements.
    evidence.append(fact)
   else:
    if entries:
     result.remove(raw);reductions.append({'raw_query':raw,'resource_semantics':evidence,
      'rule':'TexturePass.InsertCoordNormalization returns before using normalized value for IntCoords; actual common NIR proves only TXF/TXS (including validated integer constant offsets) for every matching source binding'})
 unit=(semantic_usage or {}).get('unit_render_scale',{})
 if unit.get('requested')is True and unit.get('replaced_scale_loads',0)>0:
  requirement={'name':'backend/unit_render_scale','arguments':0,'args':[0,0,0,0],'type':0,'values':[1]}
  result.append(requirement)
  reductions.append({'explicit_backend_requirement':requirement,'unit_render_scale':unit,
   'rule':'source/native pixel extents must match for target and every consumed texture; original CB0 is not provided or removed'})
 if not result or len(result)>256:raise ValueError('invalid specialization requirement count')
 return result,reductions

def texture_types(facts,recipe,actual):
 # Exact sidecar from this normal compiler invocation, corroborated by actual
 # lowered common NIR operation facts. Never derive native dimensions from
 # source reflection, source SPIR-V declarations or specialization expectations.
 keys={'schema','basis','texture_mask','sampler_mask','code_bytes','dimensions','operations'}
 if not isinstance(facts,dict) or set(facts)!=keys or type(facts['schema'])is not int or facts['schema']!=1 or facts['basis']!='actual common NIR texture instructions':raise ValueError('missing actual native texture type facts')
 for k in ('texture_mask','sampler_mask'):
  if type(facts[k])is not int or not 0<=facts[k]<=65535:raise ValueError('native texture mask representation')
 if facts['texture_mask']!=recipe['texture_mask'] or facts['sampler_mask']!=recipe['semantic_sampler_mask'] or facts['code_bytes']!=[s['code_bytes']for s in recipe['stages']]:raise ValueError('native types/recipe identity mismatch')
 for k in ('dimensions','operations'):
  if not isinstance(facts[k],list)or len(facts[k])!=16 or any(type(v)is not int for v in facts[k]):raise ValueError('native texture fact table shape')
 for stage in ('vs','fs'):
  semantics=actual[stage]['nir_usage']['resource_semantics']
  if semantics.get('schema')!=1 or semantics.get('basis')!='actual common NIR texture instructions':raise ValueError('unknown lowered NIR facts')
  if stage=='vs' and semantics['bindings']:raise ValueError('native type table currently fragment-only')
 gathers=gather_bindings(actual['fs']['nir_usage'])
 rows=actual['fs']['nir_usage']['resource_semantics']['bindings'];seen=set()
 for row in rows:
  b=row['binding']
  if type(b)is not int or not 0<=b<16 or b in seen:raise ValueError('duplicate lowered NIR binding')
  seen.add(b)
  if any(type(row.get(k))is not int or row[k]<0 for k in ('fetches','size_queries','samples','offsets','other')):raise ValueError('invalid lowered NIR operation count')
  g=gathers.get(b)
  if row['other']and not g:raise ValueError('unconsumed actual NIR other operation')
  operations=(1 if row['samples']else 0)|(2 if row['fetches']else 0)|(4 if row['size_queries']else 0)|(16 if g else 0)
  if facts['operations'][b]!=operations:raise ValueError('native operation table differs from actual lowered NIR')
 if facts['texture_mask']!=sum(1<<b for b in seen):raise ValueError('native binding table incomplete')
 for b,(dim,op)in enumerate(zip(facts['dimensions'],facts['operations'])):
  if b not in seen:
   if dim or op:raise ValueError('unused native binding has facts')
  elif dim not in (1,2,3)or not op or op&~23 or (dim==3 and op&~6) or (dim!=2 and op&16):raise ValueError('unsupported actual native texture operation/dimension')
  if b in gathers and (dim!=2 or not facts['sampler_mask']&(1<<b)):raise ValueError('gather dimension/sampler conflict')
 # Returned metadata adds corroboration; compiler sidecar remains unchanged.
 return dict(facts,gather_usage=[gathers[b]for b in sorted(gathers)])if gathers else facts

def storage_types(facts,actual):
 if not isinstance(facts,dict)or set(facts)!={'schema','basis','hardware_mapping','masks','operations'}or facts['schema']!=1 or facts['basis']!='actual common NIR storage instructions' or facts['hardware_mapping']!='VS binding; FS binding+16':raise ValueError('unknown storage proof')
 masks=facts['masks'];ops=facts['operations']
 if not isinstance(masks,list)or len(masks)!=2 or any(type(v)is not int or not 0<=v<=65535 for v in masks)or not any(masks)or not isinstance(ops,list)or len(ops)!=2:raise ValueError('invalid storage masks')
 for stage,label in enumerate(('vs','fs')):
  row=ops[stage]
  if not isinstance(row,list)or len(row)!=16 or any(type(v)is not int or not 0<=v<=7 or bool(v)!=bool(masks[stage]&(1<<i))for i,v in enumerate(row)):raise ValueError('invalid storage operations')
  expected={'schema':1,'basis':'actual common NIR storage instructions','mask':masks[stage],'operations':row}
  common=actual[label].get('nir_usage',{}).get('storage_usage')
  if common!=(expected if masks[stage]else None):raise ValueError('storage differs from actual common NIR')
  if actual[label]['num_uavs']>((16 if stage else 0)+masks[stage].bit_length()if masks[stage]else 0):raise ValueError('native storage slots exceed proven logical table')
 return facts

def build(root,stages,compiler,gmem,out,verify=False,*,shader_frontend=None,defer_depth_recipes=False,publish_depth_recipes=False,publish_storage_recipes=False):
 root=root.resolve();native=Path(__file__).resolve().parents[1];out=out.resolve();compiler=compiler.resolve()
 if type(defer_depth_recipes)is not bool or type(publish_depth_recipes)is not bool or (defer_depth_recipes and publish_depth_recipes):raise ValueError('invalid/mutually exclusive depth consumer options')
 if type(publish_storage_recipes)is not bool:raise ValueError('storage consumer option must be boolean')
 if not out.is_relative_to(root/'.local'):raise ValueError('ignored output required')
 if verify:
  p=out/'check.json';d=json.loads(p.read_text())
  if d.get('kind')!='native-graphics-pipeline-recipes-v1' or d.get('gpu_execution')is not False or d.get('nvn_runtime_registered')is not False:raise ValueError('unknown package profile')
  if d.get('stage_check')!=str(Path(stages).resolve()) or d.get('compiler_record')!=str(compiler/'build-record.json') or d.get('gmem_bytes')!=gmem:raise ValueError('cached pipeline options changed')
  if d.get('defer_depth_recipes',False)!=defer_depth_recipes or d.get('publish_depth_recipes',False)!=publish_depth_recipes:raise ValueError('cached depth consumer options changed')
  if type(d.get('publish_storage_recipes',False))is not bool or d.get('publish_storage_recipes',False)!=publish_storage_recipes:raise ValueError('cached storage consumer option changed')
  if shader_frontend is not None and d.get('frontend_root')!=str(shader_frontend_root(shader_frontend)):raise ValueError('cached frontend source root changed')
  checked(d['inputs'],{});checked(d['outputs'],{})
  compiler_record(compiler,native,{})
  if d['inputs'].get(str(Path(__file__).resolve()))!=sha(__file__):raise ValueError('package generator changed')
  return out/'offline_graphics_pipelines.h'
 if out.exists():raise ValueError('fresh ignored output required')
 if gmem<=0 or gmem>0xffffffff or gmem%4096:raise ValueError('invalid measured GMEM size')
 out.mkdir(parents=True);inputs={str(Path(__file__).resolve()):sha(__file__)};commands=[]
 tools,platform,record=compiler_record(compiler,native,inputs)
 stage_header,manifest,_=stage_archive(stages,out/'stages',root=root)
 checked(manifest['inputs'],inputs);checked(manifest['outputs'],inputs)
 for name in ('pipeline_archive.h','pipeline_recipe.h','pipeline_platform.h','pipeline_texture_types.h'):
  p=native/'include/nvn_native'/name;inputs[str(p)]=sha(p)
 header=['/* Generated immutable generic shader pipeline catalogue; game bytes remain ignored. */','#pragma once','#include "nvn_native/pipeline_archive.h"',stage_header.read_text()]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'),capture_output=True,text=True)
  commands.append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('normal compiler failed: '+r.stderr[-1500:])
 run([platform,'0x06050002',gmem,out/'device'])
 header.append((out/'device.platform.h').read_text());pair_init=[];pair_metadata=[];types_init=[];depth_init=[];storage_init=[]
 if publish_depth_recipes:
  header.append('#include "nvn_native/pipeline_depth_archive.h"');path=native/'include/nvn_native/pipeline_depth_archive.h';inputs[str(path)]=sha(path)
 if publish_storage_recipes:
  header.append('#include "nvn_native/pipeline_storage.h"');path=native/'include/nvn_native/pipeline_storage.h';inputs[str(path)]=sha(path)
 compile_check=json.loads(Path(stages).read_text());decls={s['request_sha256']:s for s in manifest['stages']}
 accepted=[p for p in compile_check['pairs']if p['passed'] and p['vs']in decls and p['fs']in decls]
 # The stage packager's bounded common-NIR interface decision is authoritative.
 rejected={(p['vs'],p['fs']) for p in manifest['linked_pair_rejections']}
 native_policy=compile_check.get('native_backend_policy',{'unit_render_scale':False})
 if native_policy not in ({'unit_render_scale':False},{'unit_render_scale':True}):raise ValueError('unknown native backend policy')
 semantic_usage={};deferred=[]
 for compile_index,p in enumerate(accepted):
  if(p['vs'],p['fs'])in rejected:continue
  v=compile_check['stages'][p['vs']];f=compile_check['stages'][p['fs']];i=len(pair_init);prefix=out/('pair-'+str(compile_index))
  run([tools,*(['--unit-render-scale']if native_policy['unit_render_scale']else []),v['prefix']+'.spv',f['prefix']+'.spv',prefix,'0','0x06050002'])
  for stage in ('vs','fs'):
   a=Path(p['prefix']+'.'+stage+'.bin');b=Path(str(prefix)+'.'+stage+'.bin')
   if a.read_bytes()!=b.read_bytes():raise ValueError('recipe compiler native code differs from exact stage archive')
  actual=json.loads(Path(str(prefix)+'.link.json').read_text())
  compiled_recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text())
  storage=None;storage_path=Path(str(prefix)+'.pipeline.storage.json')
  if storage_path.exists():
   storage=storage_types(json.loads(storage_path.read_text()),actual)
   if not publish_storage_recipes:raise ValueError('graphics storage recipe needs explicit typed storage consumer')

  for label,key in (('vs',p['vs']),('fs',p['fs'])):
   facts=actual[label]['nir_usage']
   selected={name:facts[name]for name in ('resource_semantics','unit_render_scale')}
   if key in semantic_usage and semantic_usage[key]!=selected:raise ValueError('inconsistent stage semantic resource proof across pairs')
   semantic_usage[key]=selected
  if compiled_recipe.get('depth_export') is not None and not publish_depth_recipes:
   if not defer_depth_recipes:raise ValueError('depth recipe version2 requires an explicit typed depth consumer; color archive cannot admit it')
   # ProgramSetShaders registers exact source identity. It does not choose a
   # shader specialization or promise that an eventual Draw is implemented.
   # Retain the already compiled source in the stage archive, but publish no
   # v2 recipe or typed Draw pair to this color-only consumer. Runtime Draw
   # must still resolve actual queries and find an admitted recipe.
   facts=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),compiled_recipe,actual)
   deferred.append({'vs_index':decls[p['vs']]['index'],'fs_index':decls[p['fs']]['index'],
    'reason':'depth recipe requires explicit typed depth Draw consumer; full source identity only',
    'draw_recipe_published':False,'native_code_identity':True,
    'native_code_sha256':{s:sha(str(prefix)+'.'+s+'.bin')for s in ('vs','fs')},
    'native_texture_types':facts,'metadata':compiled_recipe})
   continue
  code=(Path(str(prefix)+'.pipeline.h')).read_text()
  for name in ('nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe',*(['nn_pipeline_depth']if publish_depth_recipes else [])):
   code=re.sub(r'\b'+name+r'\b',name+'_pair_'+str(i),code)
  header.append(code);vi=decls[p['vs']]['index'];fi=decls[p['fs']]['index']
  facts=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),compiled_recipe,actual)
  typename='nn_pipeline_types_pair_'+str(i)
  gather_init=''
  if facts.get('gather_usage'):
   gather_init=',.gathers={'+','.join('['+str(g['binding'])+']={'+','.join(str(g[k])for k in ('count','component_mask','semantic_sampler_mask','offsets'))+'}'for g in facts['gather_usage'])+'}'
  header.append('static const struct nn_pipeline_texture_types '+typename+'={.version=1,.recipe=&nn_pipeline_recipe_pair_'+str(i)+',.spirv={(const uint32_t*)nn_ogs_'+str(vi)+'_spv,(const uint32_t*)nn_ogs_'+str(fi)+'_spv},.spirv_bytes={sizeof(nn_ogs_'+str(vi)+'_spv),sizeof(nn_ogs_'+str(fi)+'_spv)},.texture_mask='+str(facts['texture_mask'])+',.sampler_mask='+str(facts['sampler_mask'])+',.dimensions={'+','.join(map(str,facts['dimensions']))+'},.operations={'+','.join(map(str,facts['operations']))+'}'+gather_init+'};')
  if publish_storage_recipes:
   if storage:
    storage_name='nn_pipeline_storage_pair_'+str(i)
    header.append('static const struct nn_pipeline_storage_types '+storage_name+'={.version=1,.recipe=&nn_pipeline_recipe_pair_'+str(i)+',.spirv={(const uint32_t*)nn_ogs_'+str(vi)+'_spv,(const uint32_t*)nn_ogs_'+str(fi)+'_spv},.spirv_bytes={sizeof(nn_ogs_'+str(vi)+'_spv),sizeof(nn_ogs_'+str(fi)+'_spv)},.mask={'+','.join(map(str,storage['masks']))+'},.operations={'+','.join('{'+','.join(map(str,row))+'}'for row in storage['operations'])+'}};')
    storage_init.append('&'+storage_name)
   else:storage_init.append('NULL')
  if publish_depth_recipes:
   export=compiled_recipe.get('depth_export')
   if export is not None:
    if export.get('schema')!=1 or export.get('recipe_version')!=2 or export.get('writes_depth')is not True or type(export.get('regid'))is not int or not 0<=export['regid']<192 or export.get('color_outputs')not in (0,1) or export.get('late_z_required')is not True or export.get('lrz_disabled_required')is not True:raise ValueError('unknown actual depth compiler export contract')
    if actual['fs'].get('depth_export',{}).get('regid')!=export['regid']:raise ValueError('IR3 and recipe depth register disagree')
   else:
    # Normal compiler allows only DATA0 for a color-v1 FS. Use that existing
    # lowered IR3 declaration, then runtime corroborates all six PM4 output
    # registers. This sidecar describes ordinary raster depth, not a v2
    # recipe rewrite or an expected shader/resource query answer.
    outputs=actual['fs'].get('outputs')
    if actual['fs'].get('depth_export') is not None or not isinstance(outputs,list) or len(outputs)!=1 or outputs[0].get('slot')!=4 or type(outputs[0].get('regid'))is not int or not 0<=outputs[0]['regid']<192:raise ValueError('ordinary depth attachment requires actual sole FRAG_RESULT_DATA0/no depth export')
    header.append('static const struct nn_pipeline_depth_export nn_pipeline_depth_pair_'+str(i)+'={1,&nn_pipeline_recipe_pair_'+str(i)+',0,252,1,1,1};')
   depth_init.append('&nn_pipeline_depth_pair_'+str(i))
  types_init.append('&'+typename)
  pair_init.append('{'+f'{vi},{fi},&nn_pipeline_recipe_pair_{i}'+'}');pair_metadata.append({'vs_index':vi,'fs_index':fi,'native_code_identity':True,'native_texture_types':facts,'native_texture_types_identity':{'vs_specialization_sha256':decls[p['vs']]['specialization_sha256'],'fs_specialization_sha256':decls[p['fs']]['specialization_sha256'],'vs_spirv_sha256':sha(v['prefix']+'.spv'),'fs_spirv_sha256':sha(f['prefix']+'.spv')},'metadata':json.loads(Path(str(prefix)+'.pipeline.json').read_text())})
 if not pair_init:raise ValueError('no bounded native pipeline pair')
 frontend=shader_frontend_root(shader_frontend)
 dependencies={frontend/'src/Ryujinx.Graphics.Shader/Translation/ShaderDefinitions.cs':
  b'public bool AlphaToCoverageDitherEnable => _graphicsState.AlphaToCoverageEnable && _graphicsState.AlphaToCoverageDitherEnable;',
  frontend/'src/Ryujinx.Graphics.Shader/Translation/EmitterContext.cs':
  b'if (!TranslatorContext.Definitions.AlphaToCoverageDitherEnable || (TranslatorContext.Definitions.OmapTargets & 8) == 0)'}
 for path,condition in dependencies.items():
  if condition not in path.read_bytes():raise ValueError('conditional dither compiler dependency changed')
  inputs[str(path)]=sha(path)
 path=frontend/'src/Ryujinx.Graphics.Shader/Translation/Transforms/TexturePass.cs'
 if b'bool intCoords = (texOp.Flags & TextureFlags.IntCoords) != 0;'not in path.read_bytes()or b'if (isCoordNormalized || intCoords)'not in path.read_bytes():raise ValueError('normalization compiler dependency changed')
 inputs[str(path)]=sha(path)
 requirements=[];stage_requirements=[]
 for s in manifest['stages']:
  i=s['index'];q,reductions=queries(s['reflection'],semantic_usage.get(s['request_sha256']));requirements.append({'stage_index':i,'queries':q,'dependency_reductions':reductions,'raw_queries':s['reflection']['queries']})
  header.append('static const struct nn_pipeline_query_requirement nn_stage_requirements_'+str(i)+'[]={'+','.join(
   '{{'+json.dumps(x['name'])+','+str(x['arguments'])+','+str(x['type'])+','+str(len(x['values']))+',{'+','.join(map(str,x['args']))+'}},{'+','.join(str(n)+'u'for n in x['values'])+'}}'for x in q)+'};')
  stage_requirements.append('{'+f'{i},nn_stage_requirements_{i},{len(q)}'+'}')
 header+=['static const struct nn_pipeline_stage_requirements nn_pipeline_requirements[]={'+','.join(stage_requirements)+'};',
  'static const struct nn_pipeline_pair_recipe nn_pipeline_pairs[]={'+','.join(pair_init)+'};',
  'static const struct nn_pipeline_archive NN_GRAPHICS_PIPELINE_ARCHIVE={&nn_offline_graphics,nn_pipeline_requirements,'+str(len(requirements))+',nn_pipeline_pairs,'+str(len(pair_init))+',&nn_pipeline_device};',
  'static const struct nn_pipeline_texture_types *const nn_pipeline_pair_types[]={'+','.join(types_init)+'};',
  'static const struct nn_pipeline_typed_archive NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE={1,&NN_GRAPHICS_PIPELINE_ARCHIVE,nn_pipeline_pair_types,'+str(len(types_init))+'};',
  '#define NN_GRAPHICS_PIPELINE_HAS_TEXTURE_TYPES 1']
 if publish_depth_recipes:
  header+=['static const struct nn_pipeline_depth_export *const nn_pipeline_depth_pairs[]={'+','.join(depth_init)+'};','static const struct nn_pipeline_depth_archive NN_GRAPHICS_PIPELINE_DEPTH_ARCHIVE={1,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,nn_pipeline_depth_pairs,'+str(len(depth_init))+'};','#define NN_GRAPHICS_PIPELINE_HAS_DEPTH_ARCHIVE 1']
 if publish_storage_recipes:
  header.append('static const struct nn_pipeline_storage_types *const nn_pipeline_storage_pairs[]={'+','.join(storage_init)+'};')
  header.append('static const struct nn_pipeline_storage_archive NN_GRAPHICS_PIPELINE_STORAGE_ARCHIVE={1,&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE,nn_pipeline_storage_pairs,'+str(len(pair_init))+'};')
 h=out/'offline_graphics_pipelines.h';h.write_text('\n'.join(header)+'\n')
 outputs={str(p):sha(p)for p in out.rglob('*')if p.is_file()}
 checked(inputs,{})
 result={'schema':1,'kind':'native-graphics-pipeline-recipes-v1','gpu_execution':False,'nvn_runtime_registered':False,
  'runtime_compiler':False,'draw_template':False,'chip':0x06050002,'gmem_bytes':gmem,'frontend_root':str(frontend),'stage_check':str(Path(stages).resolve()),'compiler_record':str(compiler/'build-record.json'),'inputs':inputs,'outputs':outputs,
  'commands':commands,'native_backend_policy':native_policy,'defer_depth_recipes':defer_depth_recipes,'publish_depth_recipes':publish_depth_recipes,'publish_storage_recipes':publish_storage_recipes,'deferred_pairs':deferred,'pairs':pair_metadata,'requirements':requirements,
  'scope':'original full source/control/C1 matching plus explicit actual runtime queries required; native recipe generation and HOST only'}
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return h
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--stages',type=Path,required=True);p.add_argument('--compiler',type=Path,required=True);p.add_argument('--gmem-bytes',type=lambda x:int(x,0),required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--verify',action='store_true');p.add_argument('--defer-depth-recipes',action='store_true',help='retain normally compiled full shader sources but publish no depth Draw recipe');p.add_argument('--publish-depth-recipes',action='store_true',help='publish real v2 exports and verified ordinary raster-depth sidecars for explicit native depth consumer');p.add_argument('--publish-storage-recipes',action='store_true',help='explicit real typed graphics-storage consumer and source/API mapping required');p.add_argument('--shader-frontend-root',type=Path,help='Public shader frontend checkout; or NVN_SHADER_FRONTEND_ROOT');a=p.parse_args()
 print(build(a.root,a.stages,a.compiler,a.gmem_bytes,a.out,a.verify,shader_frontend=a.shader_frontend_root,defer_depth_recipes=a.defer_depth_recipes,publish_depth_recipes=a.publish_depth_recipes,publish_storage_recipes=a.publish_storage_recipes))
