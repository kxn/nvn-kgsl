#!/usr/bin/env python3
"""Normal authored PUBLIC NVN BC multi-mip96 qualifier; no GPU execution.

The exact bounded 0x880 original stages identify the independent oracle only.
Runtime source matching/query answers remain generic and use actual NVN state.
"""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
from nvn_pipeline_bc_qualification import dependencies

def produce(root,pipeline,views,fdl_path,kernel,pair_index,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 from bc_mip_oracle import pixels,base_count
 ph,bh,library,k,pair,selected,inputs=dependencies(root,pipeline,views,fdl_path,kernel,pair_index)
 types=pair.get('native_texture_types',{})
 if types.get('schema')!=1 or types.get('basis')!='actual common NIR texture instructions' or types.get('texture_mask')!=2 or types.get('sampler_mask')!=2 or types.get('dimensions')!=[0,2]+[0]*14 or types.get('operations')!=[0,1]+[0]*14:raise ValueError('normal actual-NIR 2D sampled typed archive required')
 own=Path(__file__).resolve().parents[1];dep=root/'nvn-native'
 for rel in ('tools/nvn_pipeline_bc_mip_qualification.py','tools/nvn_pipeline_bc_mip_validation.py','tools/nvn_pipeline_bc_mip_runner.py','tools/bc_mip_oracle.py','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_bc_mip_probe.inc','tests/nvn_pipeline_bc_mip_pattern.c','tests/bc_mip_cases.h'):
  p=own/rel;inputs[str(p)]=sha(p)
 inputs[str(dep/'tools/nvn_pipeline_bc_qualification.py')]=sha(dep/'tools/nvn_pipeline_bc_qualification.py')
 for p in [dep/'src/texture_source.c',dep/'android/nvn_pipeline_probe_activity.c',*sorted((dep/'include/nvn_native').glob('*.h'))]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];cases=None;expected=None;oracle_checks=[]
 for cc,san in (('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])):
  compiler=Path(shutil.which(cc)).resolve();inputs[str(compiler)]=sha(compiler);exe=out/('oracle-'+cc)
  argv=[str(compiler),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,'-I'+str(dep/'include'),'-I'+str(own/'tests'),str(own/'tests/nvn_pipeline_bc_mip_pattern.c'),str(dep/'src/texture_source.c'),'-lm','-o',str(exe)]
  result=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
  if result.returncode:raise ValueError(result.stderr)
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  result=subprocess.run([str(exe),'--cases'],check=True,capture_output=True,text=True,env=env);inventory=json.loads(result.stdout)
  if cases is not None and inventory!=cases:raise ValueError('different sanitized source layout')
  cases=inventory;expected=b''.join(pixels(c,r)for c in cases for r in range(2))
  result=subprocess.run([str(exe)],capture_output=True,env=env)
  if result.returncode or result.stdout!=expected:raise ValueError('C/NumPy multi-mip endpoint oracle mismatch')
  for c in cases:
   if pixels(c,0)==pixels(c,1):raise ValueError('same-handle replay not distinguishable')
  oracle_checks.append(dict(compiler=cc,channel_checks=len(expected),sha256=hashlib.sha256(expected).hexdigest(),gpu_execution=False))
 (out/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')
 header=out/'nvn_pipeline_bc_mip_qualification.h'
 header.write_text('/* Qualification only: original bounded0x880 graphics identity; no full898 game claim. */\n#include '+json.dumps(str(ph))+'\n#include '+json.dumps(str(Path(bh).with_name('texture_native_views.h')))+'\n#include '+json.dumps(str(bh))+'\n#include '+json.dumps(k['header'])+'\n#define NN_BC_PUBLIC_PAIR '+str(pair_index)+'u\n#define NN_BC_BLOCK_IMAGE NVN_COMPILED_IMAGE\n')
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('changed public BC mip dependency '+p)
 record=dict(schema=1,kind='public-nvn-bc-mip-sampled-qualification-v1',producer_sha256=sha(__file__),pipeline_check=str(pipeline),native_views_check=str(views),runtime_fdl_check=str(fdl_path),runtime_fdl_library=str(library),kernel_check=str(kernel),pair_index=pair_index,pair=pair,header=str(header),graphics_control_profile_bytes=2176,original_stage_inputs=[s['envelope']['inputs']for s in selected],cases=cases,handles=48,repeats=2,submissions=96,channel_checks=1572864,guard_byte_checks=786432,readonly_byte_checks=sum(2*(8192+2*c['source_pool_bytes']+32768)for c in cases),mixed_unorm_srgb=True,typed_archive_required=True,source_levels=4,qualification_sampler='nearest/clamp non-mip; actual selected base/count, all four canonical mips copied',oracle_checks=oracle_checks,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},android_execution=False,gpu_execution=False,scope='HOST C/NumPy independent endpoint oracle + source metadata, actual PUBLIC NVN multi-mip sampling pending ordinary UID GPU. No full-game/control898, mipfilter or all-LOD claim.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('kind')!='public-nvn-bc-mip-sampled-qualification-v1'or c.get('schema')!=1 or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public BC mip qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed public BC mip dependency '+p)
 ph,bh,library,k,pair,selected,required=dependencies(root,Path(c['pipeline_check']),Path(c['native_views_check']),Path(c['runtime_fdl_check']),Path(c['kernel_check']),c['pair_index'])
 if any(c['inputs'].get(p)!=h for p,h in required.items())or c.get('runtime_fdl_library')!=str(library)or c.get('pair')!=pair or c.get('original_stage_inputs')!=[s['envelope']['inputs']for s in selected]:raise ValueError('incomplete/changed public BC mip provenance')
 from bc_mip_oracle import base_count
 shapes=[(f,w,h,v)for f in range(0x41,0x49)for w,h in ((64,64),(127,65))for v in range(3)]
 if c.get('cases')!=json.loads((check.parent/'cases.json').read_text())or len(c.get('cases',[]))!=48:raise ValueError('changed checked case inventory')
 for i,(x,(f,w,h,v))in enumerate(zip(c['cases'],shapes)):
  expected=dict(case=i,format=f,width=w,height=h,levels=4,view=v,base0=base_count(v,0)[0],count0=base_count(v,0)[1],base1=base_count(v,1)[0],count1=base_count(v,1)[1],metadata_flags=18 if f&1 else 20)
  if any(type(x.get(k))is not int or x[k]!=y for k,y in expected.items())or type(x.get('payload_bytes'))is not int or x['payload_bytes']<=0 or x.get('source_pool_bytes')!=8192+((x['payload_bytes']+4095)&~4095):raise ValueError('changed authored multi-mip shape')
 if c.get('readonly_byte_checks')!=sum(2*(8192+2*x['source_pool_bytes']+32768)for x in c['cases'])or c.get('graphics_control_profile_bytes')!=2176 or c.get('submissions')!=96 or c.get('channel_checks')!=1572864 or c.get('guard_byte_checks')!=786432 or c.get('mixed_unorm_srgb')is not True or c.get('source_levels')!=4 or c.get('typed_archive_required')is not True:raise ValueError('changed public BC mip count contract')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','pipeline-check','native-views','runtime-fdl-check','kernel-check','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--pair-index',type=int,required=True);a=p.parse_args()
 print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.pair_index,a.out.resolve()))
if __name__=='__main__':main()
