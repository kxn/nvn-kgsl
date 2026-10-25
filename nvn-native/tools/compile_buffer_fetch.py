#!/usr/bin/env python3
"""Normal authored BUFFER TXF/TXS SPIR-V/NIR/IR3 recipes, no game/runtime execution."""
import argparse,hashlib,importlib.util,json,os,struct,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(root,ir3,tools,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local output required')
 n=root/'nvn-native';p=n/'tools/compute_driverparams.py';s=importlib.util.spec_from_file_location('buffer_fetch_compiler',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);record,inputs,_,count=m.compiler_inputs(root,ir3)
 compilers=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-ir3'];platform=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-platform']
 if len(compilers)!=1 or len(platform)!=1:raise ValueError('normal pipeline tools missing')
 source=n/'tests/shaders';out.mkdir(parents=True);runs=[]
 for p in [Path(__file__).resolve(),n/'tools/compute_driverparams.py',n/'include/nvn_native/pipeline_native_program.h',n/'include/nvn_native/pipeline_texture_types.h',n/'include/nvn_native/texture_buffer_sample.h',*[source/x for x in ('buffer_fetch.vert','buffer_fetch.frag','buffer_size.frag')],*[tools/x for x in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 def run(argv,good=True):
  argv=list(map(str,argv));p=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble'));runs.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if (p.returncode==0)!=good:raise ValueError(p.stdout+p.stderr)
  return p
 for src in ('buffer_fetch.vert','buffer_size.frag'):
  target=out/(src+'.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',source/src,'-o',target]);run([tools/'spirv-val','--target-env','spv1.3',target]);run([tools/'spirv-dis',target,'-o',target.with_suffix('.spvasm')])
 for kind,value in [('float',0),('uint',1),('sint',2)]:
  target=out/('buffer_'+kind+'.frag.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DTYPE='+str(value),source/'buffer_fetch.frag','-o',target]);run([tools/'spirv-val','--target-env','spv1.3',target]);run([tools/'spirv-dis',target,'-o',target.with_suffix('.spvasm')])
 headers=[];recipes=[]
 for index,mode in enumerate(('float','size')):
  prefix=out/('buffer_'+mode);vs=out/'buffer_fetch.vert.spv';fs=out/('buffer_'+mode+'.frag.spv');run([compilers[0],vs,fs,prefix,'0','0x06050002'])
  recipe=json.loads(prefix.with_suffix('.pipeline.json').read_text());types=json.loads(prefix.with_suffix('.pipeline.types.json').read_text());use=json.loads(prefix.with_suffix('.fs.io.json').read_text())['resource_semantics']['bindings']
  op=4 if mode=='size'else 2
  if recipe['texture_mask']!=2 or recipe['semantic_sampler_mask']!=0 or recipe['has_kill']or types!={'schema':1,'basis':'actual common NIR texture instructions','texture_mask':2,'sampler_mask':0,'code_bytes':[recipe['stages'][i]['code_bytes']for i in range(2)],'dimensions':[0,1]+[0]*14,'operations':[0,op]+[0]*14}:raise ValueError('not actual scalarBUFFER TXF/TXS proof')
  expected=[dict(binding=1,fetches=int(mode!='size'),size_queries=int(mode=='size'),samples=0,offsets=0,other=0)]
  if use!=expected:raise ValueError('BUFFER operation metadata changed')
  # Existing normal compiler emits dimensions1 exclusively for actual NIR
  # GLSL_SAMPLER_DIM_BUF; scalar1D is lowered to2D or rejected before this.
  text=prefix.with_suffix('.pipeline.h').read_text()
  for name in ('nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe'):text=text.replace(name,name+'_'+mode)
  headers.append(text)
  for stage,file in (('vs',vs),('fs',fs)):
   data=file.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data);headers.append('static const uint32_t buffer_'+mode+'_'+stage+'_spv[]={'+','.join(str(x)+'u'for x in words)+'};')
  pointer='&nn_pipeline_recipe_'+mode;vname='buffer_'+mode+'_vs_spv';fname='buffer_'+mode+'_fs_spv';headers.append('static const struct nn_pipeline_texture_types buffer_'+mode+'_types={.version=1,.recipe='+pointer+',.spirv={'+vname+','+fname+'},.spirv_bytes={sizeof('+vname+'),sizeof('+fname+')},.texture_mask=2,.sampler_mask=0,.dimensions={0,1},.operations={0,'+str(op)+'}};')
  headers.append('static const struct nn_pipeline_native_artifact buffer_'+mode+'_artifact={.version=1,.recipe='+pointer+',.spirv={'+vname+','+fname+'},.spirv_bytes={sizeof('+vname+'),sizeof('+fname+')},.textures=&buffer_'+mode+'_types};')
  recipes.append(dict(mode=mode,recipe=recipe,types=types,prefix=str(prefix),VS_spirv_sha256=sha(vs),FS_spirv_sha256=sha(fs)))
 # Current normal compiler explicitly refuses integer sampled declarations.
 # Preserve those real errors, no forged integer recipe or source-only type tag.
 for mode in ('uint','sint'):
  p=run([compilers[0],out/'buffer_fetch.vert.spv',out/('buffer_'+mode+'.frag.spv'),out/('unsupported_'+mode),'0','0x06050002'],False)
  if 'only scalar floating'not in p.stderr:raise ValueError('unexpected integer compiler gate')
 run([platform[0],'0x06050002','1179648',out/'device']);header=out/'buffer_fetch.native.h';header.write_text('/* Authored normal standalone shader artifacts. No fake Maxwell control. */\n#pragma once\n#include "nvn_native/pipeline_native_program.h"\n'+ '\n'.join(headers)+'\n'+(out/'device.platform.h').read_text())
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal shader source changed '+p)
 result=dict(schema=1,kind='authored-native-buffer-texture-fetch-v1',passed=True,original_game_shader=False,gpu_execution=False,android_execution=False,compiler_build=record,compiler_source_files=count,inputs=inputs,commands=runs,recipes=recipes,header=str(header),artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','ir3','spirv-tools','out'):p.add_argument('--'+k,required=True,type=Path)
 a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
