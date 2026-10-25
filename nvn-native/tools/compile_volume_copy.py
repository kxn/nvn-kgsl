#!/usr/bin/env python3
"""Compile/verify the separate true3D raw-block kernel with normal existing tools."""
import argparse,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--root',required=True,type=Path);p.add_argument('--ir3',required=True,type=Path);p.add_argument('--spirv-tools',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
 root=a.root.resolve();source=root/'nvn-native/shaders';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 sys.path.insert(0,str(root/'nvn-native/tools'));import compute_driverparams
 record,inputs,compiler,count=compute_driverparams.compiler_inputs(root,a.ir3.resolve())
 tools=a.spirv_tools.resolve();shader=source/'volume_copy.comp'
 for q in [Path(__file__).resolve(),shader,root/'nvn-native/tools/compute_driverparams.py',*[tools/x for x in ['glslc','spirv-val','spirv-dis']]]:inputs[str(q)]=sha(q)
 out.mkdir();commands=[];env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble');prefix=out/'volume_copy'
 def run(argv):
  q=subprocess.run(list(map(str,argv)),env=env,text=True,capture_output=True);commands.append(dict(argv=list(map(str,argv)),returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if q.returncode:raise ValueError(q.stdout+q.stderr)
 run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',shader,'-o',prefix.with_suffix('.spv')]);run([tools/'spirv-val','--target-env','spv1.3',prefix.with_suffix('.spv')]);run([tools/'spirv-dis',prefix.with_suffix('.spv'),'-o',prefix.with_suffix('.spvasm')]);run([compiler,prefix.with_suffix('.spv'),prefix,'0x06050002'])
 program=json.loads(prefix.with_suffix('.json').read_text());native=json.loads(prefix.with_suffix('.native.json').read_text());expected=dict(chip_id=0x06050002,local_size=[8,8,1],ssbo_table_slots=2,ubo_table_slots=3,num_uavs=2,num_samplers=0,private_bytes=0,shared_bytes=0,early_preamble=False)
 ranges=program.get('ubo_push_ranges',[])
 if any(program.get(k)!=v for k,v in expected.items())or len(ranges)!=1 or ranges[0].get('block')!=2 or ranges[0].get('start_bytes')!=0 or ranges[0].get('end_bytes')!=64 or ranges[0].get('bindless')is not False or ranges[0].get('global')is not False or native.get('schema')!=1 or native.get('descriptor_fdl_checks')!=20:raise ValueError('unsupported actual volume kernel contract')
 for q,h in inputs.items():
  if sha(q)!=h:raise ValueError('input changed during normal compile '+q)
 result=dict(schema=1,kind='native-volume-block-copy-v1',gpu_execution=False,runtime_compiler=False,production_registered=False,scope='Authored volume kernel; existing normal compiler and exact source closure, no Android/GPU/BC decode',compiler_build=record,compiler_source_files=count,inputs=inputs,program=program,native=native,header=str(prefix.with_suffix('.native.h')),artifacts={str(q):sha(q)for q in out.iterdir()if q.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');print(out/'check.json')
def verify(root,path):
 c=json.loads(path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='native-volume-block-copy-v1' or c.get('gpu_execution')is not False or c.get('runtime_compiler')is not False:raise ValueError('unknown volume-kernel package')
 for name,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(name)!=h:raise ValueError('volume kernel source/artifact changed '+name)
 for rel in ['tools/compile_volume_copy.py','shaders/volume_copy.comp']:
  q=root/'nvn-native'/rel
  if c['inputs'].get(str(q))!=sha(q):raise ValueError('volume kernel differs from current normal source')
 return c
if __name__=='__main__':main()
