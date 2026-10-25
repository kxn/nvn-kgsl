#!/usr/bin/env python3
"""HOST-only common NIR/IR3 image-query/fetch and IO-reflection checks."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(f):return hashlib.sha256(Path(f).read_bytes()).hexdigest()
def load(f):
 s=importlib.util.spec_from_file_location('closure',f);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--root',type=Path,required=True);a.add_argument('--ir3',type=Path,required=True);a.add_argument('--spirv-tools',type=Path,required=True);a.add_argument('--original-check',type=Path,required=True);a.add_argument('--baseline-check',type=Path,required=True);a.add_argument('--frontend',type=Path);a.add_argument('--c1-fixture',type=Path,action='append',default=[]);a.add_argument('--packager',type=Path);a.add_argument('--out',type=Path,required=True);p=a.parse_args()
 root=p.root.resolve();build=p.ir3.resolve();tools=p.spirv_tools.resolve();out=p.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):a.error('fresh ignored output required')
 out.mkdir(parents=True);record,inputs,unused,count=load(root/'nvn-native/tools/compute_driverparams.py').compiler_inputs(root,build)
 inputs[str(Path(__file__).resolve())]=sha(__file__);inputs[str(root/'nvn-native/tools/compute_driverparams.py')]=sha(root/'nvn-native/tools/compute_driverparams.py')
 for t in ['glslc','spirv-val','spirv-as','spirv-dis']:inputs[str(tools/t)]=sha(tools/t)
 compiler=build/'build/src/freedreno/fdl/nvn-graphics-ir3';env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip');commands=[];results=[]
 def run(args):
  x=subprocess.run(list(map(str,args)),text=True,capture_output=True,env=env);commands.append({'argv':list(map(str,args)),'returncode':x.returncode,'stdout':x.stdout,'stderr':x.stderr});return x
 def checked(args):
  x=run(args)
  if x.returncode:raise ValueError(x.stderr[-2000:])
  return x
 # Author qualification shaders only; original Maxwell code/control/C1 untouched.
 vertex='#version 450\nlayout(location=0)in vec2 p;layout(location=1)out vec4 v;void main(){gl_Position=vec4(p,0,1);v=vec4(p,0,1);}\n'
 def glsl(name,text,stage):
  f=out/(name+'.'+stage);f.write_text(text);spv=out/(name+'.spv');checked([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O0',f,'-o',spv]);checked([tools/'spirv-val','--target-env','spv1.3',spv]);return spv
 vs=glsl('own',vertex,'vert')
 head='#version 450\nlayout(location=1)in vec4 v;layout(location=0)out vec4 o;'
 positive={
  'sample':('layout(set=0,binding=1)uniform sampler2D t;','o=texture(t,v.xy);',2,2,1,0,0),
  'query15':('layout(set=0,binding=15)uniform sampler2D t;','o=vec4(textureSize(t,0),0,1);',32768,0,0,0,1),
  'fetch15':('layout(set=0,binding=15)uniform sampler2D t;','o=texelFetch(t,ivec2(v.xy),0);',32768,0,0,1,0),
  'offset15':('layout(set=0,binding=15)uniform sampler2D t;','o=texelFetchOffset(t,ivec2(v.xy),0,ivec2(1,-1));',32768,0,0,1,0),
  'mixed15':('layout(set=0,binding=15)uniform sampler2D t;','o=texture(t,v.xy)+texelFetch(t,ivec2(v.xy),0)+vec4(textureSize(t,0),0,1);',32768,32768,1,1,1)}
 for name,(decl,body,tm,sm,samples,fetches,queries) in positive.items():
  fs=glsl(name,head+decl+'void main(){'+body+'}\n','frag');prefix=out/name;checked([compiler,vs,fs,prefix,0,'0x06050002']);meta=json.loads(Path(str(prefix)+'.link.json').read_text());u=meta['fs']['nir_usage']['texture_usage']
  expected={'texture_mask':tm,'semantic_sampler_mask':sm,'implicit_samples':samples,'texel_fetches':fetches,'size_lod_queries':queries,'constant_offset_fetches':1 if name=='offset15' else 0}
  if u!=expected:raise ValueError(name+' usage differs: '+str(u))
  if meta['fs']['num_samplers']!=(16 if tm==32768 else 2):raise ValueError('sparse native texture table capacity lost')
  results.append({'name':name,'accepted':True,'usage':u})
 negative={
  'set1':('layout(set=1,binding=1)uniform sampler2D t;','o=texelFetch(t,ivec2(v.xy),0);'),
  'binding16':('layout(set=0,binding=16)uniform sampler2D t;','o=texture(t,v.xy);'),
  'array_const':('layout(set=0,binding=1)uniform sampler2D t[2];','o=texelFetch(t[1],ivec2(v.xy),0);'),
  'array_dynamic':('layout(set=0,binding=1)uniform sampler2D t[2];','o=texture(t[int(v.x)&1],v.xy);'),
  'sampler_set1':('layout(set=0,binding=1)uniform texture2D t;layout(set=1,binding=1)uniform sampler s;','o=texture(sampler2D(t,s),v.xy);'),
  'dim3':('layout(set=0,binding=1)uniform sampler3D t;','o=texelFetch(t,ivec3(v.xyz),0);'),
  'image_array':('layout(set=0,binding=1)uniform sampler2DArray t;','o=texelFetch(t,ivec3(v.xyz),0);'),
  'shadow':('layout(set=0,binding=1)uniform sampler2DShadow t;','o=vec4(texture(t,v.xyz));'),
  'integer':('layout(set=0,binding=1)uniform isampler2D t;','o=vec4(texelFetch(t,ivec2(v.xy),0));'),
  'storage_image':('layout(set=0,binding=1,rgba32f)uniform readonly image2D t;','o=imageLoad(t,ivec2(v.xy));'),
  'lod_sampling':('layout(set=0,binding=1)uniform sampler2D t;','o=textureLod(t,v.xy,0);'),
  'query_levels':('layout(set=0,binding=1)uniform sampler2D t;','o=vec4(textureQueryLevels(t));')}
 for name,(decl,body) in negative.items():
  fs=glsl(name,head+decl+'void main(){'+body+'}\n','frag');x=run([compiler,vs,fs,out/name,0,'0x06050002'])
  if x.returncode!=4 or 'unqualified SPIR-V profile:' not in x.stderr:raise ValueError(name+' not strictly rejected: '+x.stderr)
  results.append({'name':name,'accepted':False,'reason':x.stderr.strip()})
 # Raw SPIR-V is assembled/validated by existing SPIRV-Tools only.
 partial=vertex.replace('v=vec4(p,0,1);','v.xy=p;');pv=glsl('partial',partial,'vert');full=glsl('full_consumer',head+'void main(){o=v;}','frag')
 x=run([compiler,pv,full,out/'partial',0,'0x06050002'])
 if x.returncode!=4:raise ValueError('partial output unexpectedly covers vec4')
 u=json.loads((out/'partial.vs.io.json').read_text());slot=next(e for e in u['outputs'] if e['slot']==33)
 if slot!={'slot':33,'known':True,'may_write_mask':3,'store_mask':15,'direct_undef_mask':12} or u['all_paths_initialized_proven']is not False:raise ValueError('partial output undef reflection')
 results.append({'name':'partial_output','accepted':False,'usage':slot,'all_paths_initialized_proven':False})
 conditional=vertex.replace('v=vec4(p,0,1);','if(p.x>0)v=vec4(p,0,1);');cv=glsl('conditional',conditional,'vert');checked([compiler,cv,full,out/'conditional',0,'0x06050002']);u=json.loads((out/'conditional.vs.io.json').read_text())
 if u['all_paths_initialized_proven']is not False:raise ValueError('may-write union falsely claims all-path proof')
 results.append({'name':'conditional_output','accepted':True,'all_paths_initialized_proven':False,'usage':next(e for e in u['outputs'] if e['slot']==33)})
 # Verify untouched originals and exact native baseline pairs.
 original=json.loads(p.original_check.read_text());baseline=json.loads(p.baseline_check.read_text())
 for check in [p.original_check,p.baseline_check]:inputs[str(check.resolve())]=sha(check)
 for c in [original,baseline]:
  for mapping in [c['inputs'],c['artifacts']]:
   for f,h in mapping.items():
    if sha(f)!=h:raise ValueError('original compilation dependency changed: '+f)
    inputs[f]=h
 old={(q['vs'],q['fs']):q for q in baseline['pairs'] if q['passed']};identical=[];c1=[]
 for q in original['pairs']:
  if not q['passed']:continue
  key=(q['vs'],q['fs'])
  if key in old:
   for stage in ['vs','fs']:
    f=q['prefix']+'.'+stage+'.bin';b=old[key]['prefix']+'.'+stage+'.bin'
    if Path(f).read_bytes()!=Path(b).read_bytes():raise ValueError('existing native binary changed: '+f)
   identical.append({'vs':q['vs'],'fs':q['fs']})
  fixture=json.loads(Path(original['stages'][q['fs']]['fixture']).read_text())
  if fixture['blobs']['c1']['size']:
   meta=q['program'];usage=meta['fs']['nir_usage']['texture_usage']
   if not any(e['block']==4 for e in meta['fs']['ubo_push_ranges']) or usage['semantic_sampler_mask']!=0 or usage['texture_mask']!=6:raise ValueError('C1/table resource contract lost')
   c1.append({'vs':q['vs'],'fs':q['fs'],'code_bytes':fixture['blobs']['code']['size'],'c1_bytes':fixture['blobs']['c1']['size'],'control_bytes':fixture['blobs']['control']['size'],'blobs':fixture['blobs'],'program':meta})
 if len(identical)!=2 or len(c1)!=1 or c1[0]['c1_bytes']!=256:raise ValueError('unexpected original shader inventory')
 variants=[]
 if p.c1_fixture:
  if not p.frontend:raise ValueError('--c1-fixture requires --frontend')
  frontend=json.loads((p.frontend.resolve()/'build-record.json').read_text());inputs[str((p.frontend.resolve()/'build-record.json'))]=sha(p.frontend.resolve()/'build-record.json')
  validator=load(root/'nvn-native/tools/shader_fixture.py');inputs[str(root/'nvn-native/tools/shader_fixture.py')]=sha(root/'nvn-native/tools/shader_fixture.py')
  vertex_prefix=next(s['prefix'] for s in original['stages'].values() if s['stage']=='vs' and s['frontend_passed'])
  for i,fixture in enumerate(p.c1_fixture):
   fixture=fixture.resolve();doc=json.loads(fixture.read_text());envelope=validator.validate(doc,fixture.parent);inputs[str(fixture)]=sha(fixture)
   for blob in doc['blobs'].values():inputs[str((fixture.parent/blob['path']).resolve())]=sha(fixture.parent/blob['path'])
   if envelope['inputs']['c1']['size']!=256:raise ValueError('C1 variant is not full original data')
   prefix=out/('original-c1-variant'+str(i));checked([frontend['commands'][0][0],frontend['entrypoint'],fixture,prefix]);checked([tools/'spirv-val',str(prefix)+'.spv']);checked([compiler,vertex_prefix+'.spv',str(prefix)+'.spv',prefix,0,'0x06050002'])
   meta=json.loads(Path(str(prefix)+'.link.json').read_text());resources=json.loads(Path(str(prefix)+'.resources.json').read_text())
   if resources['code_sha256']!=envelope['inputs']['code']['sha256'] or not any(x['block']==4 for x in meta['fs']['ubo_push_ranges']):raise ValueError('C1 resource identity lost')
   variants.append({'fixture':str(fixture),'request_sha256':envelope['request_sha256'],'inputs':envelope['inputs'],'resources':resources,'program':meta})
 interface_rejections=[]
 if p.packager:
  packager=p.packager.resolve();inputs[str(packager)]=sha(packager);m=load(packager)
  import copy
  sample=next(q['program'] for q in original['pairs'] if q['passed']);m.interface_check(sample)
  for name in ['missing_nir','unknown_mask','missing_component','type_mismatch','false_allpaths_claim']:
   value=copy.deepcopy(sample)
   if name=='missing_nir':del value['vs']['nir_usage']
   elif name=='false_allpaths_claim':value['vs']['nir_usage']['all_paths_initialized_proven']=True
   else:
    slot=next(e['slot'] for e in value['fs']['inputs'] if e['bary'] and e['compmask'])
    if name=='type_mismatch':next(e for e in value['vs']['nir_usage']['interfaces']['outputs'] if e['slot']==slot)['base_type']+=1
    else:
     target=next(e for e in value['vs']['nir_usage']['outputs'] if e['slot']==slot)
     if name=='unknown_mask':target['known']=False
     else:target['may_write_mask']=0
   try:m.interface_check(value)
   except ValueError as e:interface_rejections.append({'case':name,'rejection':str(e)})
   else:raise ValueError('NIR archive interface corruption accepted: '+name)
 artifacts={str(f):sha(f) for f in out.iterdir() if f.is_file()}
 if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('dependency changed during checks')
 proof={'schema':1,'passed':True,'scope':'HOST compiler/asmroundtrip/reflection and original stage archive only; no GPU or game frame','gpu_execution':False,'compiler_build':record,'compiler_source_files':count,'environment':{'IR3_SHADER_DEBUG':'asmroundtrip'},'results':results,'original_frontend_stages':len(original['stages']),'original_native_pairs':sum(q['passed'] for q in original['pairs']),'baseline_machine_code_identical_pairs':identical,'original_c1_native':c1,'original_c1_variants':variants,'interface_metadata_rejections':interface_rejections,'commands':commands,'inputs':inputs,'artifacts':artifacts}
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
if __name__=='__main__':main()
