#!/usr/bin/env python3
"""Actual authored native storage family, no Maxwell/NVN/game source envelope."""
import argparse,json,os,subprocess
from pathlib import Path
from package_graphics_pipelines import compiler_record,texture_types,checked,sha

def build(root,compiler,tools,out):
 native=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,native,inputs)
 for p in [native/'shaders/storage_gpu.vert',native/'shaders/storage_gpu.frag',native/'tools/package_graphics_pipelines.py',native/'tools/package_graphics_stages.py',*[tools/n for n in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];cases=[]
 def run(a):
  a=list(map(str,a));r=subprocess.run(a,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr)
 for case in range(4):
  for stage in ('vert','frag'):
   source=native/'shaders'/('storage_gpu.'+stage);dest=out/(f'case{case}.{stage}.spv')
   run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DCASE='+str(case),source,'-o',dest]);run([tools/'spirv-val','--target-env','spv1.3',dest]);run([tools/'spirv-dis',dest,'-o',dest.with_suffix('.spvasm')])
  prefix=out/('case'+str(case));run([cc,out/f'case{case}.vert.spv',out/f'case{case}.frag.spv',prefix,0,'0x06050002'])
  actual=json.loads(Path(str(prefix)+'.link.json').read_text());recipe=json.loads(Path(str(prefix)+'.pipeline.json').read_text());types=texture_types(json.loads(Path(str(prefix)+'.pipeline.types.json').read_text()),recipe,actual);storage=json.loads(Path(str(prefix)+'.pipeline.storage.json').read_text())
  b=15 if case==3 else 1;op=3 if case in (1,3)else 5 if case==2 else 1
  expected=[[op if i==b else 0 for i in range(16)]]*2
  if storage!={'schema':1,'basis':'actual common NIR storage instructions','hardware_mapping':'VS binding; FS binding+16','masks':[1<<b]*2,'operations':expected}:raise ValueError('wrong actual storage facts')
  for stage in ('vs','fs'):
   facts=actual[stage]['nir_usage']['storage_usage']
   if facts!={'schema':1,'basis':'actual common NIR storage instructions','mask':1<<b,'operations':expected[0]}:raise ValueError('storage sidecar differs from actual NIR')
  lines=['/* Authored GLSL storage artifact; no original game envelope. */','#pragma once','#include "nvn_native/pipeline_native_program.h"',Path(str(prefix)+'.pipeline.h').read_text()]
  for stage in ('vert','frag'):
   raw=(out/f'case{case}.{stage}.spv').read_bytes();words=[int.from_bytes(raw[i:i+4],'little')for i in range(0,len(raw),4)];lines+=['static const uint32_t nn_storage_'+stage+'[]={'+','.join(map(str,words))+'};']
  lines+=['static const struct nn_pipeline_texture_types nn_storage_textures={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_storage_vert,nn_storage_frag},.spirv_bytes={sizeof(nn_storage_vert),sizeof(nn_storage_frag)}};',
   'static const struct nn_pipeline_storage_types nn_storage_types={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_storage_vert,nn_storage_frag},.spirv_bytes={sizeof(nn_storage_vert),sizeof(nn_storage_frag)},.mask={'+','.join(map(str,storage['masks']))+'},.operations={'+','.join('{'+','.join(map(str,row))+'}'for row in expected)+'}};',
   'static const struct nn_pipeline_native_artifact NN_STORAGE_ARTIFACT={.version=1,.recipe=&nn_pipeline_recipe,.spirv={nn_storage_vert,nn_storage_frag},.spirv_bytes={sizeof(nn_storage_vert),sizeof(nn_storage_frag)},.textures=&nn_storage_textures};']
  header=out/f'case{case}.native.h';header.write_text('\n'.join(lines)+'\n');cases.append(dict(case=case,header=str(header),binding=b,operations=op,metadata=storage))
 run([platform,'0x06050002',1179648,out/'device']);checked(inputs,{})
 result=dict(schema=1,kind='native-authored-graphics-storage-gpu-v1',passed=True,source_identity='authored GLSL only, never original Maxwell/NVN source',gpu_execution=False,cases=cases,commands=commands,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},compiler_record=str(compiler/'build-record.json'));(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','compiler','spirv-tools','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.root.resolve(),a.compiler.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
