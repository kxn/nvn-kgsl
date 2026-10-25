#!/usr/bin/env python3
"""Normal authored color-only SAMPLE native artifact; no game/source envelope or GPU execution."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def build(root,compiler,tools,out):
 sys.path.insert(0,str(root/'nvn-native/tools'))
 from package_graphics_pipelines import compiler_record,texture_types,checked
 native=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,root/'nvn-native',inputs)
 for f in [native/'shaders/sampler_mip.vert',native/'shaders/sampler_mip.frag',root/'nvn-native/tools/package_graphics_pipelines.py',root/'nvn-native/tools/package_graphics_stages.py',root/'nvn-native/include/nvn_native/pipeline_texture_types.h',*[tools/n for n in ('glslc','spirv-val','spirv-dis')]]:inputs[str(f)]=sha(f)
 out.mkdir(parents=True);runs=[]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));runs.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError('normal compiler failed: '+r.stderr[-3000:])
 for suffix in ('vert','frag'):
  source=native/'shaders'/('sampler_mip.'+suffix);dest=out/(suffix+'.spv')
  run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',source,'-o',dest]);run([tools/'spirv-val','--target-env','spv1.3',dest]);run([tools/'spirv-dis',dest,'-o',out/(suffix+'.spvasm')])
 prefix=out/'sampler_mip';run([cc,out/'vert.spv',out/'frag.spv',prefix,0,'0x06050002']);run([platform,'0x06050002',1179648,out/'device'])
 actual=json.loads(Path(str(prefix)+'.link.json').read_text());recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text());types=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),recipe,actual)
 if types['operations']!=[0,1]+[0]*14 or types['dimensions']!=[0,2]+[0]*14 or types['texture_mask']!=2 or types['sampler_mask']!=2:raise ValueError('authored SAMPLE actual typed profile changed')
 if actual['fs']['nir_usage']['resource_semantics']['bindings']!=[{'binding':1,'fetches':0,'size_queries':0,'samples':1,'offsets':0,'other':0}]:raise ValueError('missing actual implicit SAMPLE NIR facts')
 lines=['/* Authored normal native color artifact: no Maxwell or NVN source identity. */','#pragma once','#include "nvn_native/pipeline_native_program.h"',(out/'sampler_mip.pipeline.h').read_text(),(out/'device.platform.h').read_text()]
 for name,suffix in (('nn_sampler_mip_color_vs','vert'),('nn_sampler_mip_color_fs','frag')):
  raw=(out/(suffix+'.spv')).read_bytes();words=[int.from_bytes(raw[i:i+4],'little')for i in range(0,len(raw),4)];lines.append('static const uint32_t '+name+'[]={'+','.join(map(str,words))+'};')
 lines+=['static const struct nn_pipeline_texture_types nn_sampler_mip_color_types={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_sampler_mip_color_vs,nn_sampler_mip_color_fs},.spirv_bytes={sizeof(nn_sampler_mip_color_vs),sizeof(nn_sampler_mip_color_fs)},.texture_mask=2,.sampler_mask=2,.dimensions={0,2},.operations={0,NN_PIPELINE_TEX_SAMPLE}};',
  'static const struct nn_pipeline_native_artifact NN_SAMPLER_MIP_ARTIFACT={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_sampler_mip_color_vs,nn_sampler_mip_color_fs},.spirv_bytes={sizeof(nn_sampler_mip_color_vs),sizeof(nn_sampler_mip_color_fs)},.textures=&nn_sampler_mip_color_types};']
 header=out/'sampler_mip.native.h';header.write_text('\n'.join(lines)+'\n');checked(inputs,{})
 result=dict(schema=1,kind='native-authored-color-sampler_mip-v1',gpu_execution=False,runtime_compiler=False,source_identity='authored GLSL only; no Maxwell/source-control/C1 matching',inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},compiler_record=str(compiler/'build-record.json'),native_texture_types=types,header=str(header))
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','compiler','spirv-tools','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
