#!/usr/bin/env python3
"""Normal authored PUBLIC NVN BC qualifier inputs; never runs Android or GPU.

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
def dependencies(root,pipeline,views,fdl_path,kernel,pair_index):
 dep=root/'nvn-native';p=json.loads(pipeline.read_text())
 ph=load(dep/'tools/package_graphics_pipelines.py','public_bc_package').build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,True)
 bh,b=load(dep/'tools/texture_bridge_bundle.py','public_bc_bridge').build(root,views)
 library,f=load(dep/'tools/build_runtime_fdl.py','public_bc_fdl').verify(root,fdl_path)
 k=load(dep/'tools/compile_block_copy.py','public_bc_block').verify(root,kernel)
 if p.get('chip')!=0x06050002 or p.get('gmem_bytes')!=1179648 or type(pair_index)is not int or not 0<=pair_index<len(p['pairs']):raise ValueError('explicit A650 pair required')
 pair=p['pairs'][pair_index];stages=list(json.loads(Path(p['stage_check']).read_text())['stages'].values());selected=[stages[pair[n+'_index']]for n in ('vs','fs')]
 identities=[('c1906ac598c15e11a3029bff6db444f45b4343ec56ffb15624eff0be5f7d7369','96757bb36a024b91e6f161b424af3cc3db2f035f059842e5151ba7fb1ff89cc4'),('a98bd42200fa86b7c94c19ecb528449043c6fb71c235dafbf46ce20f7c8fdbda','a7672f1d3bfeb7d0312812c8e98eea92565d5934bad06bbb5f345e5a684f92e5')]
 for s,(code,control)in zip(selected,identities):
  v=s['envelope']['inputs']
  if v['code']['sha256']!=code or v['control']['sha256']!=control or v['control']['size']!=2176 or v['c1']['size']!=0:raise ValueError('not the bounded original stage underlying this authored oracle')
 if any(pair['metadata'].get(k)!=v for k,v in dict(texture_mask=2,semantic_sampler_mask=2,vertex_inputs=3,has_kill=True,fragcoord_mask=0).items()):raise ValueError('changed original shader semantic resource contract')
 if not any(v.get('width')==64 and v.get('height')==64 and v.get('pitch')==256 for v in b['views']):raise ValueError('actual FDL64 LINEAR target required')
 inputs={str(x):sha(x)for x in (pipeline,views,fdl_path,kernel)}
 for c in (p,b,f,k):inputs.update(c['inputs']);inputs.update(c.get('outputs',{}));inputs.update(c.get('artifacts',{}))
 return ph,bh,library,k,pair,selected,inputs

def produce(root,pipeline,views,fdl_path,kernel,pair_index,out,srgb=False):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 ph,bh,library,k,pair,selected,inputs=dependencies(root,pipeline,views,fdl_path,kernel,pair_index)
 own=Path(__file__).resolve().parents[1];dep=root/'nvn-native'
 files=[Path(__file__).resolve(),own/'tools/nvn_pipeline_bc_validation.py',own/'tools/nvn_pipeline_bc_runner.py',own/'tests/nvn_pipeline_probe.c',own/'tests/nvn_pipeline_bc_probe.inc',own/'tests/nvn_pipeline_bc_pattern.c',own/'tests/bc_srgb_cases.h',dep/'tests/bc_sampled_cases.h',dep/'tools/build_bc_sampled_qualification.py',dep/'src/texture_source.c',dep/'android/nvn_pipeline_probe_activity.c']
 for x in [*files,*sorted((dep/'include/nvn_native').glob('*.h'))]:inputs[str(x)]=sha(x)
 oracle=load(own/'tools/bc_srgb_oracle.py'if srgb else dep/'tools/build_bc_sampled_qualification.py','public_bc_closed_oracle')
 if srgb:inputs[str(own/'tools/bc_srgb_oracle.py')]=sha(own/'tools/bc_srgb_oracle.py')
 first_format=0x45 if srgb else 0x41
 expected=b''.join(oracle.expected(fmt,((2*x+1)*w)//128,((2*y+1)*h)//128,r)for fmt in range(first_format,first_format+4)for w,h in ((8,8),(37,21),(127,65))for r in range(2)for y in range(64)for x in range(64))
 out.mkdir(parents=True);commands=[];cases=None;oracle_checks=[]
 for cc,san in (('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])):
  compiler=Path(shutil.which(cc)).resolve();inputs[str(compiler)]=sha(compiler);exe=out/('oracle-'+cc)
  argv=[str(compiler),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,'-DNVN_NVN_PIPELINE_BC_SRGB='+str(int(srgb)),'-I'+str(dep/'include'),'-I'+str(dep/'tests'),str(own/'tests/nvn_pipeline_bc_pattern.c'),str(dep/'src/texture_source.c'),'-lm','-o',str(exe)]
  result=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
  if result.returncode:raise ValueError(result.stderr)
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  result=subprocess.run([str(exe)],capture_output=True,env=env)
  if result.returncode or result.stdout!=expected:raise ValueError('C/Python BC authored oracle mismatch')
  result=subprocess.run([str(exe),'--cases'],check=True,capture_output=True,text=True,env=env);inventory=json.loads(result.stdout)
  if cases is not None and inventory!=cases:raise ValueError('different sanitized source layout')
  cases=inventory;oracle_checks.append(dict(compiler=cc,channel_checks=len(expected),sha256=hashlib.sha256(expected).hexdigest(),gpu_execution=False))
 header=out/'nvn_pipeline_bc_qualification.h'
 header.write_text('/* Qualification only: exact declared 0x880 graphics control span, not game full0x898. */\n#include '+json.dumps(str(ph))+'\n#include '+json.dumps(str(Path(bh).with_name('texture_native_views.h')))+'\n#include '+json.dumps(str(bh))+'\n#include '+json.dumps(k['header'])+'\n#define NN_BC_PUBLIC_PAIR '+str(pair_index)+'u\n#define NN_BC_BLOCK_IMAGE NVN_COMPILED_IMAGE\n')
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('changed public BC dependency '+p)
 record=dict(schema=1,kind='public-nvn-bc-sampled-qualification-v1',producer_sha256=sha(__file__),pipeline_check=str(pipeline),native_views_check=str(views),runtime_fdl_check=str(fdl_path),runtime_fdl_library=str(library),kernel_check=str(kernel),pair_index=pair_index,pair=pair,header=str(header),graphics_control_profile_bytes=2176,original_stage_inputs=[s['envelope']['inputs']for s in selected],cases=cases,handles=12,repeats=2,submissions=24,channel_checks=393216,guard_byte_checks=196608,readonly_byte_checks=sum(2*(2*4096+2*c['source_pool_bytes']+32768)for c in cases),srgb=bool(srgb),qualification_sampler='nearest/clamp, normalized ordinary2D, mip0/layer0',oracle_checks=oracle_checks,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},android_execution=False,gpu_execution=False,scope='HOST production inputs and independent authored oracle; actual PUBLIC NVN descriptor/query/BC sampling remains unqualified until ordinary UID Activity GPU run. No game full control/frame, C1 pixel or GOB RT claim.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('kind')!='public-nvn-bc-sampled-qualification-v1'or c.get('schema')!=1 or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public BC qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed public BC dependency '+p)
 ph,bh,library,k,pair,selected,required=dependencies(root,Path(c['pipeline_check']),Path(c['native_views_check']),Path(c['runtime_fdl_check']),Path(c['kernel_check']),c['pair_index'])
 if any(c['inputs'].get(p)!=h for p,h in required.items())or c.get('runtime_fdl_library')!=str(library)or c.get('pair')!=pair or c.get('original_stage_inputs')!=[s['envelope']['inputs']for s in selected]:raise ValueError('incomplete/changed public BC provenance')
 first_format=0x45 if c.get('srgb')is True else 0x41
 expected_shape=[(f,w,h)for f in range(first_format,first_format+4)for w,h in ((8,8),(37,21),(127,65))]
 if len(c.get('cases',[]))!=12 or any((x.get('format'),x.get('width'),x.get('height'))!=shape or x.get('case')!=i or x.get('metadata_flags')!= (18 if shape[0]&1 else 20) for i,(x,shape)in enumerate(zip(c['cases'],expected_shape))):raise ValueError('changed public BC authored shape')
 if c.get('readonly_byte_checks')!=sum(2*(8192+2*x['source_pool_bytes']+32768)for x in c['cases'])or c.get('graphics_control_profile_bytes')!=2176 or c.get('submissions')!=24 or c.get('channel_checks')!=393216 or c.get('guard_byte_checks')!=196608:raise ValueError('changed public BC exact count contract')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','pipeline-check','native-views','runtime-fdl-check','kernel-check','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--pair-index',type=int,required=True);p.add_argument('--srgb',action='store_true',help='Independent nonzero/nonone endpoint sRGB qualification, not historical UNORM');a=p.parse_args()
 print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.pair_index,a.out.resolve(),a.srgb))
if __name__=='__main__':main()
