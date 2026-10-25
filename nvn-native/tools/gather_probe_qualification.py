#!/usr/bin/env python3
"""Normal authored sparse float2D TG4 component/wrap/replay qualifier; no NVN/game source identity."""
import argparse,json,os,subprocess
from pathlib import Path
from package_graphics_pipelines import compiler_record,texture_types,checked,sha
from texture_bridge_bundle import native_views

def produce(root,compiler,tools,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,n,inputs)
 for rel in ('shaders/gather_probe.vert','shaders/gather_probe.frag','tools/package_graphics_pipelines.py','tools/package_graphics_stages.py','tools/texture_bridge_bundle.py','tools/gather_probe_validation.py','tools/gather_probe_runner.py','tests/gather_probe.c','tests/gather_probe_cases.h','tests/pipeline_probe.c','src/pipeline.c','src/pipeline_program.c'):
  f=n/rel;inputs[str(f)]=sha(f)
 for name in ('glslc','spirv-val','spirv-dis'):inputs[str(tools/name)]=sha(tools/name)
 out.mkdir(parents=True);runs=[]
 def run(argv):
  argv=list(map(str,argv));x=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));runs.append(dict(argv=argv,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if x.returncode:raise ValueError(x.stderr[-5000:])
 run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',n/'shaders/gather_probe.vert','-o',out/'vs.spv']);run([tools/'spirv-val','--target-env','spv1.3',out/'vs.spv']);run([platform,'0x06050002',1179648,out/'device'])
 lines=['/* Authored normal TG4 native artifacts; no Maxwell/control/C1. */','#pragma once','#include "nvn_native/pipeline_native_program.h"',(out/'device.platform.h').read_text()];entries=[];metadata=[]
 def array(name,path):
  raw=path.read_bytes();lines.append('static const uint32_t '+name+'[]={'+','.join(str(int.from_bytes(raw[i:i+4],'little'))for i in range(0,len(raw),4))+'};')
 array('nn_gather_probe_vs',out/'vs.spv')
 for component in range(4):
  for binding in (1,13):
   i=len(entries);spv=out/('fs-'+str(i)+'.spv');prefix=out/('pair-'+str(i));run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DGATHER_COMPONENT='+str(component),'-DGATHER_BINDING='+str(binding),n/'shaders/gather_probe.frag','-o',spv]);run([tools/'spirv-val','--target-env','spv1.3',spv]);run([tools/'spirv-dis',spv,'-o',out/('fs-'+str(i)+'.spvasm')]);run([cc,out/'vs.spv',spv,prefix,0,'0x06050002'])
   actual=json.loads(Path(str(prefix)+'.link.json').read_text());recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text());facts=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),recipe,actual)
   expected={'binding':binding,'count':1,'component_mask':1<<component,'semantic_sampler_mask':1<<binding,'offsets':0}
   if facts.get('gather_usage')!=[expected]or facts['texture_mask']!=1<<binding or facts['sampler_mask']!=1<<binding or facts['operations'][binding]!=16 or recipe['vertex_inputs'] or recipe['has_kill']or recipe.get('depth_export')or recipe['fragcoord_mask']:raise ValueError('actual authored Gather profile mismatch')
   if ('gather4'+'rgba'[component])not in Path(str(prefix)+'.fs.asm').read_text().lower():raise ValueError('actual native Gather component opcode mismatch')
   if actual['vs']['ubo_push_ranges']or actual['fs']['ubo_push_ranges']!=[{'block':5,'range_offset_bytes':0,'start_bytes':0,'end_bytes':64}]or any(not 0<st['code_bytes']<=4096 for st in recipe['stages']):raise ValueError('actual coordinate/code owner contract changed')
   code=Path(str(prefix)+'.pipeline.h').read_text()
   for symbol in ('nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe'):code=code.replace(symbol,symbol+'_'+str(i))
   lines.append(code);array('nn_gather_probe_fs_'+str(i),spv)
   lines.append('static const struct nn_pipeline_texture_types nn_gather_probe_types_'+str(i)+'={.version=1,.recipe=&nn_pipeline_recipe_'+str(i)+',.spirv={nn_gather_probe_vs,nn_gather_probe_fs_'+str(i)+'},.spirv_bytes={sizeof(nn_gather_probe_vs),sizeof(nn_gather_probe_fs_'+str(i)+')},.texture_mask='+str(1<<binding)+',.sampler_mask='+str(1<<binding)+',.dimensions={['+str(binding)+']=2},.operations={['+str(binding)+']=16},.gathers={['+str(binding)+']={1,'+str(1<<component)+','+str(1<<binding)+',0}}};')
   entries.append('{.version=1,.recipe=&nn_pipeline_recipe_'+str(i)+',.spirv={nn_gather_probe_vs,nn_gather_probe_fs_'+str(i)+'},.spirv_bytes={sizeof(nn_gather_probe_vs),sizeof(nn_gather_probe_fs_'+str(i)+')},.textures=&nn_gather_probe_types_'+str(i)+'}')
   metadata.append(dict(index=i,component=component,binding=binding,types=facts,recipe=recipe,actual_program=actual))
 lines.append('static const struct nn_pipeline_native_artifact NN_GATHER_PROBE_ARTIFACTS[]={'+','.join(entries)+'};')
 views,extra,_=native_views({'compiler_build':record},compiler,out,[(32,16),(8,4)]);inputs.update(extra)
 for name,v in zip(('NN_GATHER_TARGET','NN_GATHER_TEXTURE'),views):lines.append('static const struct nn_tex_native_view '+name+'={'+','.join('.'+k+'='+('{' + ','.join(map(str,x)) + '}'if isinstance(x,list)else str(int(x)))for k,x in v.items())+'};')
 header=out/'gather_probe_qualification.h';header.write_text('\n'.join(lines)+'\n');checked(inputs,{})
 c=dict(schema=1,kind='raw-native-gather-qualification-v1',producer_sha256=sha(__file__),gpu_execution=False,nvn_api=False,game_modules_executed=False,compiler_record=str(compiler/'build-record.json'),header=str(header),programs=metadata,target=views[0],texture=views[1],components=4,bindings=[1,13],wraps=[1,6,7],origins=2,repeats=2,jobs=48,submissions=96,files=96,sequence_offset=45056,sequence_bytes_exempt=4,coordinate_ubo_binding=5,coordinate_ubo_bytes=64,max_channel_error=1,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 c=json.loads(check.read_text());root=(root or Path(__file__).resolve().parents[2]).resolve()
 if c.get('kind')!='raw-native-gather-qualification-v1'or c.get('producer_sha256')!=sha(__file__)or c.get('gpu_execution')is not False or c.get('submissions')!=96:raise ValueError('unknown/stale Gather qualifier')
 checked(c['inputs'],{});checked(c['artifacts'],{});compiler_record(Path(c['compiler_record']).parent,Path(__file__).resolve().parents[1],{})
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','spirv-tools','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
