#!/usr/bin/env python3
"""Compile one new raw-block kernel with normal existing GLSL/NIR/IR3 tools.
Old gob_copy/linear_copy producers and artifacts remain independent.
"""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):
 s=importlib.util.spec_from_file_location('nn_block_copy_closure',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(root,ir3,tools,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored directory below ROOT/.local required')
 common=load(root/'nvn-native/tools/compute_driverparams.py');record,inputs,compiler,count=common.compiler_inputs(root,ir3)
 shader=root/'nvn-native/shaders/block_copy.comp'
 for p in [Path(__file__).resolve(),root/'nvn-native/tools/compute_driverparams.py',shader,*[tools/n for n in ['glslc','spirv-val','spirv-dis']]]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);runs=[];env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble')
 def run(cmd):
  p=subprocess.run(list(map(str,cmd)),env=env,text=True,capture_output=True);runs.append(dict(argv=list(map(str,cmd)),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
 prefix=out/'block_copy';run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',shader,'-o',prefix.with_suffix('.spv')]);run([tools/'spirv-val','--target-env','spv1.3',prefix.with_suffix('.spv')]);run([tools/'spirv-dis',prefix.with_suffix('.spv'),'-o',prefix.with_suffix('.spvasm')]);run([compiler,prefix.with_suffix('.spv'),prefix,'0x06050002'])
 p=json.loads(prefix.with_suffix('.json').read_text());n=json.loads(prefix.with_suffix('.native.json').read_text())
 expected=dict(chip_id=0x06050002,local_size=[8,8,1],ssbo_table_slots=2,ubo_table_slots=3,num_uavs=2,num_samplers=0,private_bytes=0,shared_bytes=0,early_preamble=False)
 ranges=p.get('ubo_push_ranges',[])
 if any(p.get(k)!=v for k,v in expected.items())or len(ranges)!=1 or ranges[0].get('block')!=2 or ranges[0].get('start_bytes')!=0 or ranges[0].get('end_bytes')!=64 or ranges[0].get('bindless')is not False or ranges[0].get('global')is not False or n.get('schema')!=1 or n.get('descriptor_fdl_checks')!=20:raise ValueError('unsupported actual block-kernel resource contract')
 # Constants/immediates are those actually encoded by the normal producer;
 # do not guess zero because runtime integer division needs real constants.
 for key in ['constant_data_bytes','immediate_dwords','driver_params_dwords']:
  if type(p.get(key))is not int or p[key]<0:raise ValueError('bad compiler constant metadata')
 for name,h in inputs.items():
  if sha(name)!=h:raise ValueError('compiler input changed '+name)
 c=dict(schema=1,kind='native-general-block-copy-v1',gpu_execution=False,runtime_compiler=False,environment={'IR3_SHADER_DEBUG':'asmroundtrip,nopreamble'},compiler_build=record,compiler_source_files=count,inputs=inputs,program=p,native=n,header=str(prefix.with_suffix('.native.h')),artifacts={str(x):sha(x)for x in out.iterdir()if x.is_file()})
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'

def verify(root,path):
 c=json.loads(path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='native-general-block-copy-v1'or c.get('gpu_execution')is not False or c.get('runtime_compiler')is not False:raise ValueError('unknown block-kernel package')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('block-kernel source/artifact changed '+p)
 for rel in ['tools/compile_block_copy.py','shaders/block_copy.comp']:
  p=root/'nvn-native'/rel
  if c['inputs'].get(str(p))!=sha(p):raise ValueError('block kernel differs from current normal source')
 return c
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--ir3',required=True,type=Path);p.add_argument('--spirv-tools',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.out.resolve()))
