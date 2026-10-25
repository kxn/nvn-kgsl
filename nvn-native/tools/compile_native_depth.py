#!/usr/bin/env python3
"""Normal authored native depth+optionalcolor recipes. No fake Maxwell input or GPU execution."""
import argparse,hashlib,importlib.util,json,os,re,struct,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(root,ir3,tools,gmem,out):
 if out.exists()or not out.is_relative_to(root/'.local')or gmem<=0 or gmem%4096:raise ValueError('fresh ignored output and explicit measured GMEM required')
 own=root/'nvn-native';tool=own/'tools/compute_driverparams.py';s=importlib.util.spec_from_file_location('native_depth_compiler',tool);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);record,inputs,_,count=m.compiler_inputs(root,ir3)
 compiler=ir3/'build/src/freedreno/fdl/nvn-pipeline-ir3';platform=ir3/'build/src/freedreno/fdl/nvn-pipeline-platform';source=own/'tests/shaders';out.mkdir(parents=True);commands=[]
 for p in [Path(__file__).resolve(),tool,own/'include/nvn_native/pipeline_native_program.h',own/'include/nvn_native/pipeline_texture_types.h',source/'depth_native.vert',source/'depth_native.frag',*[tools/x for x in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 def run(a):
  a=list(map(str,a));p=subprocess.run(a,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
 headers=[];recipes=[]
 for mode,only in [('color',0),('only',1)]:
  vs=out/('depth-'+mode+'.vert.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DDEPTH_ONLY='+str(only),source/'depth_native.vert','-o',vs]);run([tools/'spirv-val','--target-env','spv1.3',vs])
  fs=out/('depth-'+mode+'.frag.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DDEPTH_ONLY='+str(only),source/'depth_native.frag','-o',fs]);run([tools/'spirv-val','--target-env','spv1.3',fs]);run([tools/'spirv-dis',fs,'-o',fs.with_suffix('.spvasm')]);prefix=out/('depth-'+mode);run([compiler,vs,fs,prefix,'0','0x06050002']);meta=json.loads(Path(str(prefix)+'.pipeline.json').read_text());facts=json.loads(Path(str(prefix)+'.pipeline.types.json').read_text());d=meta['depth_export']
  if d['recipe_version']!=2 or d['writes_depth']is not True or d['color_outputs']!=1-only or not 0<=d['regid']<192 or d['late_z_required']is not True or d['lrz_disabled_required']is not True or meta['texture_mask']or meta['semantic_sampler_mask']or meta['has_kill']or facts['texture_mask']or facts['sampler_mask']or any(facts['dimensions']+facts['operations']):raise ValueError('not actual authored pure depth native profile')
  text=Path(str(prefix)+'.pipeline.h').read_text();symbols=['nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe','nn_pipeline_depth']
  for symbol in symbols:text=re.sub(r'\b'+symbol+r'\b',symbol+'_'+mode,text)
  headers.append(text)
  for stage,src in [('vs',vs),('fs',fs)]:
   data=src.read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data);headers.append('static const uint32_t depth_'+mode+'_'+stage+'_spv[]={'+','.join(str(x)+'u'for x in words)+'};')
  ptr='&nn_pipeline_recipe_'+mode;v='depth_'+mode+'_vs_spv';f='depth_'+mode+'_fs_spv';headers.append('static const struct nn_pipeline_texture_types depth_'+mode+'_types={.version=1,.recipe='+ptr+',.spirv={'+v+','+f+'},.spirv_bytes={sizeof('+v+'),sizeof('+f+')}};');headers.append('static const struct nn_pipeline_native_artifact depth_'+mode+'_artifact={.version=1,.recipe='+ptr+',.spirv={'+v+','+f+'},.spirv_bytes={sizeof('+v+'),sizeof('+f+')},.textures=&depth_'+mode+'_types};');recipes.append(dict(mode=mode,recipe=meta,types=facts,prefix=str(prefix)))
 run([platform,'0x06050002',str(gmem),out/'device']);header=out/'native-depth.h';header.write_text('/* Authored normal native depth artifacts; no Maxwell/guest claim. */\n#pragma once\n#include "nvn_native/pipeline_native_program.h"\n'+'\n'.join(headers)+'\n'+(out/'device.platform.h').read_text())
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('shader/compiler source changed '+p)
 c=dict(schema=1,kind='authored-native-depth-recipes-v1',passed=True,original_game_shader=False,gpu_execution=False,android_execution=False,gmem_bytes=gmem,compiler_build=record,compiler_source_files=count,recipes=recipes,header=str(header),inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['root','ir3','spirv-tools','out']:p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--gmem-bytes',type=lambda s:int(s,0),required=True);a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.gmem_bytes,a.out.resolve()))
