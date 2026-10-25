#!/usr/bin/env python3
"""Normal finite native depth texture qualification. No NVN/game/GPU claim."""
import argparse,json,subprocess,os,shutil
from pathlib import Path
from package_graphics_pipelines import compiler_record,checked,sha
from texture_bridge_bundle import native_views
hashes=lambda values:checked(values,{})
KIND='raw-native-depth-sampled-qualification-v1'
def produce(root,compiler,shader_check,fdl_check,depth_check,a2d_check,runtime_check,kernel_check,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};_,_,record=compiler_record(compiler,n,inputs)
 def dependency(path,kind):
  d=json.loads(path.read_text())
  if d.get('kind')!=kind or d.get('passed')is not True or d.get('gpu_execution')is not False:raise ValueError('wrong normal dependency '+str(path))
  checked(d['inputs'],{});checked(d['artifacts'],{});inputs.update(d['inputs']);inputs.update(d['artifacts']);inputs[str(path)]=sha(path);return d
 s=dependency(shader_check,'authored-native-depth-sampled-recipes-v1');depth=dependency(depth_check,'native-depth-fdl-foundation-v1');a2d=dependency(a2d_check,'native-depth-a2d-host-v1')
 runtime=json.loads(runtime_check.read_text());checked(runtime['inputs'],{});checked(runtime['artifacts'],{});inputs.update(runtime['inputs']);inputs.update(runtime['artifacts']);inputs[str(runtime_check)]=sha(runtime_check)
 if s['compiler_record']!=str(compiler/'build-record.json')or[r['role']for r in s['roles']]!=['sample','fetch','size','gather']:raise ValueError('actual normal role identity required')
 import importlib.util
 def mod(p,name):
  spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 fdl_origin=json.loads(fdl_check.read_text());origins=[Path(p).parents[2]for p in fdl_origin['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('ambiguous FDL origin')
 fdlroot=origins[0];fdllib,fdl=mod(fdlroot/'nvn-native/tools/build_runtime_fdl.py','dsfdl').verify(fdlroot,fdl_check)
 k=json.loads(kernel_check.read_text());origins=[Path(p).parents[2]for p in k['inputs']if p.endswith('/nvn-native/shaders/block_copy.comp')]
 if len(origins)!=1:raise ValueError('ambiguous block origin')
 kernel=mod(origins[0]/'nvn-native/tools/compile_block_copy.py','dskernel').verify(origins[0],kernel_check)
 for d in(fdl,kernel):inputs.update(d['inputs']);inputs.update(d['artifacts'])
 inputs[str(fdl_check)]=sha(fdl_check);inputs[str(kernel_check)]=sha(kernel_check)
 for rel in('tests/depth_sampled_layout.c','src/runtime_fdl.cc','src/texture_source.c','tests/depth_sampled_probe.c','tests/depth_shadow_cases.h','tests/pipeline_probe.c','tools/depth_sampled_probe_validation.py','tools/depth_sampled_probe_runner.py','src/depth_sampled.c','include/nvn_native/depth_sampled.h','src/depth_sampled_native.c','include/nvn_native/depth_sampled_native.h','src/pipeline.c','src/pipeline_program.c'):
  inputs[str(n/rel)]=sha(n/rel)
 out.mkdir(parents=True);views,extra,_=native_views({'compiler_build':record},compiler,out,[(33,19),(41,23)]);inputs.update(extra)
 obj=runtime_check.parent/'depth-native-api29.o'
 if runtime['artifacts'].get(str(obj))!=sha(obj):raise ValueError('real native object must be in normal record')
 libs=[str(fdllib),depth['static_library'],a2d['static_library'],str(obj)];header=out/'depth_sampled_qualification.h';header.write_text('#pragma once\n#include "nvn_native/compute.h"\n#include '+json.dumps(s['header'])+'\n#include '+json.dumps(kernel['header'])+'\n#define NN_DEPTH_SAMPLED_BLOCK_IMAGE NVN_COMPILED_IMAGE\n')
 # Run the exact source-layout calculator and normal actual Mesa FDL facade.
 normal=json.loads((fdl_check.parent/'commands.json').read_text());t=next(v for v in normal if v['argv'][0]=='g++');av=t['argv'];start=next(i for i,x in enumerate(av)if x.startswith('-I')and x.endswith('/nvn-native/include'));end=av.index('-Wl,--as-needed');flags=av[1:start];hostlibs=av[end:-2];runs=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'layout-commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-6000:])
  return r.stdout
 for tool in('gcc','g++'):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
 inc=['-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')];objects=[]
 for i,src in enumerate([n/'tests/depth_sampled_layout.c',n/'src/texture_source.c']):
  obj=out/('layout-'+str(i)+'.o');run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections',*inc,'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-c',src,'-o',obj]);objects.append(obj)
 exe=out/'layout';run(['g++',*flags,*inc,n/'src/runtime_fdl.cc',*objects,*hostlibs,'-Wl,--gc-sections','-o',exe],t['cwd']);cases=json.loads(run([exe]))
 if len(cases)!=30:raise ValueError('incomplete actual metadata catalogue')
 for r in cases:
  v=views[r['shape']]
  if (r['pitch'],r['span'])!=(v['pitch'],v['required_span_bytes']):raise ValueError('normal FDL facade/view reference mismatch')
 checked(inputs,{})
 c=dict(schema=1,kind=KIND,passed=True,producer_sha256=sha(__file__),gpu_execution=False,nvn_api=False,game_modules_executed=False,compiler_record=str(compiler/'build-record.json'),header=str(header),fdl_root=str(fdlroot),fdl_check=str(fdl_check),native_libraries=libs,targets=views,cases=cases,programs=s['roles'],jobs=30,replays=30,submissions=180,finishes=60,files=60,formats=[50,51,52],roles=['SAMPLE-nearest','SAMPLE-linear','FETCH+SIZE','SIZE','GatherR'],source_swizzle=['R','ZERO','ZERO','ONE'],compare_sampler=False,private_depth_CPU_access=False,chain=['GOB_to_LINEAR_CS','A2D_UPLOAD','actual_native_sample'],sequence_exempt_bytes=12,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 c=json.loads(check.read_text())
 if c.get('kind')!=KIND or c.get('passed')is not True or c.get('gpu_execution')is not False or c.get('producer_sha256')!=sha(__file__)or c.get('submissions')!=180:raise ValueError('unknown/stale depth sampled qualification')
 checked(c['inputs'],{});checked(c['artifacts'],{});compiler_record(Path(c['compiler_record']).parent,Path(__file__).resolve().parents[1],{})
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);keys=['root','compiler','shader-check','fdl-check','depth-check','a2d-check','runtime-check','kernel-check','out']
 for k in keys:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k.replace('-','_')).resolve()for k in keys]))
