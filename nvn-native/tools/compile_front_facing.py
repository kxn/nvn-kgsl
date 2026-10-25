#!/usr/bin/env python3
"""Normal authored native FrontFacing artifact for subsequent GPU qualification.
No Maxwell/source-control/C1 identity or public NVN/game claim.
"""
import argparse,json,os,subprocess,sys
from pathlib import Path

def build(root,compiler,tools,gmem,out):
 sys.path.insert(0,str(root/'nvn-native/tools'))
 from package_graphics_pipelines import compiler_record,texture_types,checked,sha
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local')or gmem<=0 or gmem%4096:raise ValueError('fresh ignored output and measured GMEM required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,n,inputs)
 for path in[n/'tests/shaders/front_facing.vert',n/'tests/shaders/front_facing.frag',n/'tools/package_graphics_pipelines.py',n/'tools/package_graphics_stages.py',n/'include/nvn_native/pipeline_native_program.h',n/'include/nvn_native/pipeline_texture_types.h',*[tools/x for x in('glslc','spirv-val','spirv-dis')]]:inputs[str(path)]=sha(path)
 out.mkdir(parents=True);commands=[]
 def run(a):
  a=list(map(str,a));p=subprocess.run(a,capture_output=True,text=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stderr)
 for role,suffix in(('vs','vert'),('fs','frag')):
  source=n/'tests/shaders'/('front_facing.'+suffix);dest=out/(role+'.spv');run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DCASE=0',source,'-o',dest]);run([tools/'spirv-val','--target-env','spv1.3',dest]);run([tools/'spirv-dis',dest,'-o',out/(role+'.spvasm')])
 prefix=out/'front';run([cc,out/'vs.spv',out/'fs.spv',prefix,0,'0x06050002']);run([platform,'0x06050002',gmem,out/'device'])
 link=json.loads((out/'front.link.json').read_text());recipe=json.loads((out/'front.pipeline.json').read_text());types=texture_types(json.loads((out/'front.pipeline.types.json').read_text()),recipe,link);face=recipe.get('front_facing')
 if not face or not face['live']or not face['half_scalar']or not face['merged_registers']or face['max_separate_half_register']!=-1 or not face['need_size']or any(types['dimensions']+types['operations'])or types['texture_mask']or types['sampler_mask']or recipe['fragcoord_mask']:raise ValueError('actual pure FRONT_FACE facts differ')
 lines=['/* Authored native FACE only; never a fabricated Maxwell Program. */','#pragma once','#include "nvn_native/pipeline_native_program.h"',(out/'front.pipeline.h').read_text(),(out/'device.platform.h').read_text()]
 for role in('vs','fs'):
  raw=(out/(role+'.spv')).read_bytes();words=[int.from_bytes(raw[i:i+4],'little')for i in range(0,len(raw),4)];lines.append('static const uint32_t nn_front_'+role+'_spv[]={'+','.join(map(str,words))+'};')
 lines+=['static const struct nn_pipeline_texture_types nn_front_types={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_front_vs_spv,nn_front_fs_spv},.spirv_bytes={sizeof(nn_front_vs_spv),sizeof(nn_front_fs_spv)}};','static const struct nn_pipeline_native_artifact NN_FRONT_FACING_ARTIFACT={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_front_vs_spv,nn_front_fs_spv},.spirv_bytes={sizeof(nn_front_vs_spv),sizeof(nn_front_fs_spv)},.textures=&nn_front_types};']
 header=out/'front_facing.native.h';header.write_text('\n'.join(lines)+'\n');checked(inputs,{})
 c=dict(schema=1,kind='native-authored-front-facing-v1',passed=True,gpu_execution=False,runtime_compiler=False,public_NVNs_or_game_qualified=False,header=str(header),native_texture_types=types,front_facing=face,compiler_record=str(compiler/'build-record.json'),gmem_bytes=gmem,source_identity='Authored GLSL/SPIR-V, actual common NIR/IR3; no source/control/C1 envelope',inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in('root','compiler','spirv-tools','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--gmem-bytes',type=lambda x:int(x,0),required=True);a=p.parse_args();print(build(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.gmem_bytes,a.out.resolve()))
