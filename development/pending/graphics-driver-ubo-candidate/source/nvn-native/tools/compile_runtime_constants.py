#!/usr/bin/env python3
"""Actual authored VS driver parameters/dynamic native UBOs, no fake guest source."""
import argparse,json,os,subprocess
from pathlib import Path
from package_graphics_pipelines import compiler_record,checked,sha,texture_types

def build(root,compiler,tools,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,n,inputs)
 for p in [n/'shaders/runtime_constants.vert',n/'shaders/runtime_constants.frag',n/'tools/package_graphics_pipelines.py',n/'tools/package_graphics_stages.py',*[tools/x for x in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];cases=[]
 def run(a):
  a=list(map(str,a));r=subprocess.run(a,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr)
 for case in range(6):
  for st in ('vert','frag'):
   src=n/f'shaders/runtime_constants.{st}';p=out/f'case{case}.{st}.spv';run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DCASE='+str(case),src,'-o',p]);run([tools/'spirv-val','--target-env','spv1.3',p]);run([tools/'spirv-dis',p,'-o',p.with_suffix('.spvasm')])
  prefix=out/f'case{case}';run([cc,'--runtime-constants',out/f'case{case}.vert.spv',out/f'case{case}.frag.spv',prefix,0,'0x06050002'])
  actual=json.loads(Path(str(prefix)+'.link.json').read_text());recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text());texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),recipe,actual);runtime=json.loads(Path(str(prefix)+'.pipeline.runtime.json').read_text())
  expected_mask=[4,2,1,7,0,7][case]
  if runtime['stages'][0]['driver_mask']!=expected_mask or runtime['stages'][1]['driver_mask']:raise ValueError('actual driver masks differ from authored source')
  if case>=4 and (runtime['stages'][0]['ubo_mask']!=2 or runtime['stages'][1]['ubo_mask']!=1<<15):raise ValueError('actual residual native UBO mask differs')
  lines=['/* Authored GLSL actual runtime constants. No Maxwell/control/C1 envelope. */','#pragma once','#include "nvn_native/pipeline_native_program.h"',Path(str(prefix)+'.pipeline.h').read_text()]
  for st in ('vert','frag'):
   raw=(out/f'case{case}.{st}.spv').read_bytes();lines+=['static const uint32_t nn_runtime_'+st+'[]={'+','.join(str(int.from_bytes(raw[i:i+4],'little'))for i in range(0,len(raw),4))+'};']
  lines+=['static const struct nn_pipeline_texture_types nn_runtime_textures={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_runtime_vert,nn_runtime_frag},.spirv_bytes={sizeof(nn_runtime_vert),sizeof(nn_runtime_frag)}};',
  'static const struct nn_pipeline_runtime_constants nn_runtime_constants={.version=1,.layout=&nn_pipeline_runtime,.spirv={nn_runtime_vert,nn_runtime_frag},.spirv_bytes={sizeof(nn_runtime_vert),sizeof(nn_runtime_frag)}};',
  'static const struct nn_pipeline_native_artifact NN_RUNTIME_ARTIFACT={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_runtime_vert,nn_runtime_frag},.spirv_bytes={sizeof(nn_runtime_vert),sizeof(nn_runtime_frag)},.textures=&nn_runtime_textures};']
  header=out/f'case{case}.native.h';header.write_text('\n'.join(lines)+'\n');cases.append(dict(case=case,header=str(header),runtime=runtime))
 run([platform,'0x06050002',1179648,out/'device']);checked(inputs,{})
 result=dict(schema=1,kind='native-authored-runtime-constants-v1',passed=True,gpu_execution=False,cases=cases,commands=commands,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},compiler_record=str(compiler/'build-record.json'),scope='Actual standalone NIR/IR3 source compilation only; no GPU/publicNVN/game execution')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','spirv-tools','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(build(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
