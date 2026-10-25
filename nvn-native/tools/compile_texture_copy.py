#!/usr/bin/env python3
"""Compile the authored layout-copy kernels using the existing frozen toolchain.
No CPU image conversion, game shader, Vulkan driver or runtime compiler.
"""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def build(root,ir3,tools,source,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 spec=importlib.util.spec_from_file_location('closure',root/'nvn-native/tools/compute_driverparams.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 record,inputs,compiler,count=m.compiler_inputs(root,ir3);inputs[str(Path(__file__).resolve())]=sha(__file__);inputs[str(root/'nvn-native/tools/compute_driverparams.py')]=sha(root/'nvn-native/tools/compute_driverparams.py')
 for n in ['glslc','spirv-val','spirv-dis']:inputs[str(tools/n)]=sha(tools/n)
 out.mkdir(parents=True);commands=[];cases=[];env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble')
 def run(args):
  r=subprocess.run(list(map(str,args)),env=env,text=True,capture_output=True);commands.append({'argv':list(map(str,args)),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('layout compiler rejected '+str(args[0])+': '+r.stderr[-2000:])
 for name in ['gob_copy','linear_copy']:
  shader=source/(name+'.comp');inputs[str(shader)]=sha(shader);prefix=out/name
  run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',shader,'-o',prefix.with_suffix('.spv')]);run([tools/'spirv-val','--target-env','spv1.3',prefix.with_suffix('.spv')]);run([tools/'spirv-dis',prefix.with_suffix('.spv'),'-o',prefix.with_suffix('.spvasm')]);run([compiler,prefix.with_suffix('.spv'),prefix,'0x06050002'])
  program=json.loads(prefix.with_suffix('.json').read_text());native=json.loads(prefix.with_suffix('.native.json').read_text())
  expected={'chip_id':0x06050002,'local_size':[8,8,1],'ssbo_table_slots':2,'ubo_table_slots':3,'num_uavs':2,'num_samplers':0,'private_bytes':0,'shared_bytes':0,'early_preamble':False,'constant_data_bytes':0,'immediate_dwords':0}
  if any(program.get(k)!=v for k,v in expected.items()) or native.get('descriptor_fdl_checks')!=20 or program['ubo_push_ranges']!=[{'block':2,'range_offset_bytes':program['ubo_push_ranges'][0]['range_offset_bytes'],'start_bytes':0,'end_bytes':64,'bindless':False,'global':False}]:raise ValueError('unexpected conversion resource contract')
  cases.append({'name':name,'program':program,'native':native,'header':str(prefix.with_suffix('.native.h'))})
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('layout input changed during compilation: '+p)
 outputs={str(p):sha(p) for p in out.iterdir() if p.is_file()};check={'schema':1,'kind':'native-texture-layout-copy-v1','scope':'authored GPU conversion kernels compiled only; not GPU/NVN qualification','gpu_execution':False,'runtime_compiler':False,'compiler_build':record,'compiler_source_files':count,'environment':{'IR3_SHADER_DEBUG':'asmroundtrip,nopreamble'},'cases':cases,'commands':commands,'inputs':inputs,'artifacts':outputs};(out/'check.json').write_text(json.dumps(check,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--ir3',type=Path,required=True);p.add_argument('--spirv-tools',type=Path,required=True);p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[1]/'shaders');p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.source.resolve(),a.out.resolve()))
