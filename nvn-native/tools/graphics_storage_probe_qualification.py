#!/usr/bin/env python3
"""Actual authored storage4 + normal actual FDL; no NVN/game/GPU claim."""
import argparse,importlib.util,json
from pathlib import Path
from package_graphics_pipelines import compiler_record,checked,sha
from texture_bridge_bundle import native_views
from graphics_storage_probe_validation import catalogue
KIND='raw-native-graphics-storage-qualification-v1'
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(config,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();n=Path(__file__).resolve().parents[1];config=Path(config).resolve();out=Path(out).resolve();c=json.loads(config.read_text())
 if set(c)!={'schema','kind','shader_check','runtime_fdl_check'}or c['schema']!=1 or c['kind']!='graphics-storage-gpu-inputs-v1':raise ValueError('unknown storage qualification config')
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 sp=Path(c['shader_check']);s=json.loads(sp.read_text());fdlp=Path(c['runtime_fdl_check']);fj=json.loads(fdlp.read_text());origins=[Path(p).parents[2]for p in fj['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('ambiguous normal FDL origin')
 fdllib,fdl=module(origins[0]/'nvn-native/tools/build_runtime_fdl.py','storage_qualified_fdl').verify(origins[0],fdlp)
 if s.get('kind')!='native-authored-graphics-storage-gpu-v1'or s.get('passed')is not True or s.get('gpu_execution')is not False or len(s['cases'])!=4:raise ValueError('normal actual authored four groups required')
 inputs={str(config):sha(config),str(sp):sha(sp),str(fdlp):sha(fdlp),**s['inputs'],**s['artifacts'],**fdl['inputs'],**fdl['artifacts']};checked(inputs,{})
 compiler=Path(s['compiler_record']).parent;_,_,record=compiler_record(compiler,n,inputs)
 for rel in('shaders/storage_gpu.vert','shaders/storage_gpu.frag','tools/compile_graphics_storage_gpu.py'):
  if s['inputs'].get(str(n/rel))!=sha(n/rel):raise ValueError('different authored source '+rel)
 out.mkdir(parents=True);views,extra,_=native_views({'compiler_build':record},compiler,out,[(73,41),(127,67)]);inputs.update(extra)
 header=out/'graphics_storage_qualification.h';lines=['/* Normal authored actualIR3 storage family, not original source. */','#pragma once','#include "nvn_native/pipeline_storage.h"','#include "nvn_native/pipeline_native_program.h"','#include "graphics_storage_cases.h"']
 symbols=('nn_pipeline_code_0','nn_pipeline_code_1','nn_pipeline_words','nn_pipeline_patches','nn_pipeline_inputs','nn_pipeline_recipe','nn_storage_vert','nn_storage_frag','nn_storage_textures','nn_storage_types','NN_STORAGE_ARTIFACT')
 programs=[]
 for i,row in enumerate(s['cases']):
  if row['case']!=i or row['binding']!=(15 if i==3 else 1)or row['operations']!=([1,3,5,3][i]):raise ValueError('wrong actual storage case')
  for name in symbols:lines.append('#define '+name+' nn_ssbo'+str(i)+'_'+name)
  lines.append('#include '+json.dumps(row['header']))
  for name in symbols:lines.append('#undef '+name)
  recipe=json.loads((sp.parent/f'case{i}.pipeline.json').read_text());programs.append(dict(program=i,binding=row['binding'],operations=row['operations'],code_bytes=[(sp.parent/f'case{i}.{stage}.bin').stat().st_size for stage in('vs','fs')],storage=row['metadata']))
 lines+=['#include '+json.dumps(str(sp.parent/'device.platform.h')),'static const struct nn_pipeline_native_artifact*const nn_storage_gpu_artifacts[]={'+','.join('&nn_ssbo'+str(i)+'_NN_STORAGE_ARTIFACT'for i in range(4))+'};','static const struct nn_pipeline_storage_types*const nn_storage_gpu_types[]={'+','.join('&nn_ssbo'+str(i)+'_nn_storage_types'for i in range(4))+'};']
 lines.append('static const struct nn_tex_native_view nn_storage_gpu_targets[]={'+','.join('{'+','.join('.'+k+'='+('{'+','.join(map(str,v))+'}'if isinstance(v,list)else str(int(v)))for k,v in view.items())+'}'for view in views)+'};')
 cases=catalogue();lines.append('static const struct nn_storage_gpu_case nn_storage_gpu_cases[]={'+','.join('{'+str(row['id'])+','+str(row['program'])+',{'+','.join(map(str,row['offset']))+'},'+str(row['bytes'])+','+str(row['shape'])+'}'for row in cases)+'};');header.write_text('\n'.join(lines)+'\n')
 for rel in('tools/graphics_storage_probe_qualification.py','tools/graphics_storage_probe_validation.py','tests/graphics_storage_cases.h','tests/graphics_storage_probe.c','tests/pipeline_probe.c'):
  inputs[str(n/rel)]=sha(n/rel)
 checked(inputs,{})
 result=dict(schema=1,kind=KIND,passed=True,producer_sha256=sha(__file__),gpu_execution=False,nvn_api=False,game_modules_executed=False,header=str(header),compiler_record=str(compiler/'build-record.json'),shader_check=str(sp),runtime_fdl_library=str(fdllib),runtime_fdl_check=str(fdlp),views=views,programs=programs,cases=cases,jobs=96,submissions=192,finishes=192,replays=96,files=576,storage_BO_bytes=12288,vertex_BO_bytes=4096,unused_BO_count=2,unused_BO_bytes=4096,sequence_offset=45056,sequence_bytes_exempt=4,inputs=inputs,artifacts={str(f):sha(f)for f in out.iterdir()if f.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return header,result,{str(out/'check.json'):sha(out/'check.json'),**inputs,**result['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','config','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(build(a.config,a.out,root=a.root)[0])
