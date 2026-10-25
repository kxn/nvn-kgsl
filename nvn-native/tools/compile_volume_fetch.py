#!/usr/bin/env python3
"""Normal authored 3D TXF/TXS SPIR-V/NIR/IR3 recipes, no game/runtime execution."""
import argparse,hashlib,importlib.util,json,os,struct,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(root,ir3,tools,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local output required')
 n=root/'nvn-native';p=n/'tools/compute_driverparams.py';s=importlib.util.spec_from_file_location('volume_fetch_compiler',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);record,inputs,_,count=m.compiler_inputs(root,ir3)
 compilers=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-ir3'];platform=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-platform']
 if len(compilers)!=1 or len(platform)!=1:raise ValueError('normal pipeline tools missing')
 source=n/'tests/shaders';out.mkdir(parents=True);runs=[]
 for p in [Path(__file__).resolve(),n/'tools/compute_driverparams.py',n/'include/nvn_native/pipeline_native_program.h',n/'include/nvn_native/pipeline_texture_types.h',*[source/x for x in ('volume_fetch.vert','volume_fetch.frag','volume_size.frag')],*[tools/x for x in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble'));runs.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
 for src in ('volume_fetch.vert','volume_fetch.frag','volume_size.frag'):
  target=out/(src+'.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',source/src,'-o',target]);run([tools/'spirv-val','--target-env','spv1.3',target]);run([tools/'spirv-dis',target,'-o',target.with_suffix('.spvasm')])
 headers=[];recipes=[]
 for index,mode in enumerate(('fetch','size')):
  prefix=out/('volume_'+mode);vs=out/'volume_fetch.vert.spv';fs=out/('volume_'+mode+'.frag.spv');run([compilers[0],vs,fs,prefix,'0','0x06050002'])
  recipe=json.loads(prefix.with_suffix('.pipeline.json').read_text());types=json.loads(prefix.with_suffix('.pipeline.types.json').read_text());use=json.loads(prefix.with_suffix('.fs.io.json').read_text())['resource_semantics']['bindings']
  op=2 if mode=='fetch'else 4
  if recipe['texture_mask']!=2 or recipe['semantic_sampler_mask']!=0 or recipe['has_kill']or types!={'schema':1,'basis':'actual common NIR texture instructions','texture_mask':2,'sampler_mask':0,'code_bytes':[recipe['stages'][i]['code_bytes']for i in range(2)],'dimensions':[0,3]+[0]*14,'operations':[0,op]+[0]*14}:raise ValueError('not actual scalar3D TXF/TXS proof')
  expected=[dict(binding=1,fetches=int(mode=='fetch'),size_queries=int(mode=='size'),samples=0,offsets=0,other=0)]
  if use!=expected:raise ValueError('3D operation metadata changed')
  text=prefix.with_suffix('.pipeline.h').read_text()
  for name in ('nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe'):text=text.replace(name,name+'_'+mode)
  headers.append(text)
  for stage,file in (('vs',vs),('fs',fs)):
   data=file.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data);headers.append('static const uint32_t volume_'+mode+'_'+stage+'_spv[]={'+','.join(str(x)+'u'for x in words)+'};')
  pointer='&nn_pipeline_recipe_'+mode;vname='volume_'+mode+'_vs_spv';fname='volume_'+mode+'_fs_spv';headers.append('static const struct nn_pipeline_texture_types volume_'+mode+'_types={.version=1,.recipe='+pointer+',.spirv={'+vname+','+fname+'},.spirv_bytes={sizeof('+vname+'),sizeof('+fname+')},.texture_mask=2,.sampler_mask=0,.dimensions={0,3},.operations={0,'+str(op)+'}};')
  headers.append('static const struct nn_pipeline_native_artifact volume_'+mode+'_artifact={.version=1,.recipe='+pointer+',.spirv={'+vname+','+fname+'},.spirv_bytes={sizeof('+vname+'),sizeof('+fname+')},.textures=&volume_'+mode+'_types};')
  recipes.append(dict(mode=mode,recipe=recipe,types=types,prefix=str(prefix),VS_spirv_sha256=sha(vs),FS_spirv_sha256=sha(fs)))
 run([platform[0],'0x06050002','1179648',out/'device']);header=out/'volume_fetch.native.h';header.write_text('/* Authored normal standalone shader artifacts. No fake Maxwell control. */\n#pragma once\n#include "nvn_native/pipeline_native_program.h"\n'+ '\n'.join(headers)+'\n'+(out/'device.platform.h').read_text())
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal shader source changed '+p)
 result=dict(schema=1,kind='authored-native-volume-texture-fetch-v1',passed=True,original_game_shader=False,gpu_execution=False,android_execution=False,compiler_build=record,compiler_source_files=count,inputs=inputs,commands=runs,recipes=recipes,header=str(header),artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','ir3','spirv-tools','out'):p.add_argument('--'+k,required=True,type=Path)
 a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
