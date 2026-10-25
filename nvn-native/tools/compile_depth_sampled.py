#!/usr/bin/env python3
"""Normal authored native noncompare depth SAMPLE/FETCH/SIZE/Gather recipes. No GPU."""
import argparse,json,os,re,struct,subprocess
from pathlib import Path
from package_graphics_pipelines import compiler_record,texture_types,checked,sha

def build(root,compiler,tools,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,n,inputs)
 files=[n/'shaders/depth_sampled.vert',*[n/('shaders/depth_sampled_'+r+'.frag')for r in ('sample','fetch','size','gather')],n/'tools/package_graphics_pipelines.py',n/'tools/package_graphics_stages.py',n/'include/nvn_native/pipeline_texture_types.h',*[tools/t for t in ('glslc','spirv-val','spirv-dis')]]
 inputs.update({str(p):sha(p)for p in files});out.mkdir(parents=True);runs=[];headers=[];rows=[]
 def run(argv):
  a=list(map(str,argv));r=subprocess.run(a,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));runs.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-8000:])
 for role,ops,sampler in [('sample',1,2),('fetch',6,0),('size',4,0),('gather',16,2)]:
  prefix=out/role;vs=out/(role+'.vert.spv');fs=out/(role+'.frag.spv')
  for src,dest in [(n/'shaders/depth_sampled.vert',vs),(n/('shaders/depth_sampled_'+role+'.frag'),fs)]:
   run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',src,'-o',dest]);run([tools/'spirv-val','--target-env','spv1.3',dest]);run([tools/'spirv-dis',dest,'-o',dest.with_suffix('.spvasm')])
  run([cc,vs,fs,prefix,0,'0x06050002']);meta=json.loads(Path(str(prefix)+'.pipeline.json').read_text());link=json.loads(Path(str(prefix)+'.link.json').read_text());types=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),meta,link)
  if meta['schema']!=1 or types['texture_mask']!=2 or types['sampler_mask']!=sampler or types['operations']!=[0,ops]+[0]*14 or types['dimensions']!=[0,2]+[0]*14:raise ValueError('actual normal role mismatch '+role)
  gather=types.get('gather_usage',[])
  if role=='gather'and gather!=[dict(binding=1,count=1,component_mask=1,semantic_sampler_mask=2,offsets=0)]:raise ValueError('actual GatherR proof missing')
  text=Path(str(prefix)+'.pipeline.h').read_text()
  for s in ['nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe']:text=re.sub(r'\b'+s+r'\b',s+'_'+role,text)
  headers.append(text)
  for stage,path in [('vs',vs),('fs',fs)]:
   raw=path.read_bytes();v=struct.unpack('<'+'I'*(len(raw)//4),raw);headers.append('static const uint32_t depth_sampled_'+role+'_'+stage+'[]={'+','.join(str(w)+'u'for w in v)+'};')
  v='depth_sampled_'+role+'_vs';f='depth_sampled_'+role+'_fs';r='&nn_pipeline_recipe_'+role;t='depth_sampled_'+role+'_types'
  headers.append('static const struct nn_pipeline_texture_types '+t+'={.version=1,.recipe='+r+',.spirv={'+v+','+f+'},.spirv_bytes={sizeof('+v+'),sizeof('+f+')},.texture_mask=2,.sampler_mask='+str(sampler)+',.dimensions={[1]=2},.operations={[1]='+str(ops)+'}'+(',.gathers={[1]={1,1,2,0}}'if role=='gather'else '')+'};')
  headers.append('static const struct nn_pipeline_native_artifact depth_sampled_'+role+'_artifact={.version=1,.recipe='+r+',.spirv={'+v+','+f+'},.spirv_bytes={sizeof('+v+'),sizeof('+f+')},.textures=&'+t+'};');rows.append(dict(role=role,types=types,recipe=meta,link=link,prefix=str(prefix)))
 run([platform,'0x06050002',1179648,out/'device']);header=out/'depth-sampled.native.h';header.write_text('/* Authored native roles; no Maxwell/game envelope. */\n#pragma once\n#include "nvn_native/pipeline_native_program.h"\n'+'\n'.join(headers)+'\n'+(out/'device.platform.h').read_text())
 checked(inputs,{})
 result=dict(schema=1,kind='authored-native-depth-sampled-recipes-v1',passed=True,gpu_execution=False,runtime_compiler=False,compiler_record=str(compiler/'build-record.json'),header=str(header),roles=rows,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','spirv-tools','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(build(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
