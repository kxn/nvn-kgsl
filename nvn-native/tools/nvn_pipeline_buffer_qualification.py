#!/usr/bin/env python3
"""Checked original C1 public BUFFER qualification inputs. Never executes GPU."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
KIND='public-nvn-buffer-sampled-qualification-v1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def checked(root,pipeline,views,fdl):
 native=root/'nvn-native';p=json.loads(pipeline.read_text());stages=list(json.loads(Path(p['stage_check']).read_text())['stages'].values())
 ph=load(native/'tools/package_graphics_pipelines.py','public_buffer_package').build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,True)
 bh,b=load(native/'tools/texture_bridge_bundle.py','public_buffer_views').build(root,views)
 library,f=load(native/'tools/build_runtime_fdl.py','public_buffer_fdl').verify(root,fdl)
 candidates=[]
 for i,pair in enumerate(p['pairs']):
  vs,fs=[stages[pair[n+'_index']]for n in ('vs','fs')];vr,fr=vs['resources'],fs['resources']
  q=lambda n:[v.get('value')for v in fr['queries']if v.get('name')==n]
  if vr['code_sha256']=='c1906ac598c15e11a3029bff6db444f45b4343ec56ffb15624eff0be5f7d7369'and fr['code_sha256']=='a9eda0e15d4ccb957d900b600d94d9585299ca671f06218e3b98bfc26ac8aba8'and {v.get('value')for v in vr['queries']if v.get('name')=='graphics/depth_mode'}=={True}and set(q('QueryTextureFormat'))=={1}and set(q('QueryHostSupportsSnormBufferTextureFormat'))=={False}:candidates.append((i,pair,vs,fs))
 if len(candidates)!=1:raise ValueError('one explicitly identified original source/C1 R8 format specialization required')
 index,pair,vs,fs=candidates[0];m=pair['metadata'];t=pair['native_texture_types']
 if (p['chip']!=0x06050002 or p['gmem_bytes']!=1179648 or m['texture_mask']!=6 or m['semantic_sampler_mask']or m['vertex_inputs']!=3 or not m['has_kill']or t['dimensions'][:3]!=[0,2,1]or t['operations'][:3]!=[0,6,2]):raise ValueError('changed genuine lowered-NIR Buf and full C1 resource facts')
 want=[('96757bb36a024b91e6f161b424af3cc3db2f035f059842e5151ba7fb1ff89cc4',0),('7c603a02d455070aee3720be41669e5c9e2f6562e501006fa9129677f4b07a35',256)]
 for s,(control,c1bytes)in zip((vs,fs),want):
  i=s['envelope']['inputs']
  if i['control']['size']!=2176 or i['control']['sha256']!=control or i['c1']['size']!=c1bytes:raise ValueError('changed original graphics880/C1 identity')
 if fs['envelope']['inputs']['c1']['sha256']!='fa7b186fd1a8809d9181bcd1dfc415d50511b36797198b6593fa6d512d5a8af1':raise ValueError('changed immutable C1 math')
 if not any(v.get('width')==64 and v.get('height')==64 and v.get('pitch')==256 for v in b['views']):raise ValueError('actual FDL64 view required')
 inputs={str(x):sha(x)for x in (pipeline,views,fdl)}
 for c in(p,b,f):inputs.update(c['inputs']);inputs.update(c.get('outputs',{}));inputs.update(c.get('artifacts',{}))
 return ph,Path(bh).with_name('texture_native_views.h'),library,index,pair,[vs,fs],inputs

def produce(root,pipeline,views,fdl,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 ph,vh,library,index,pair,selected,inputs=checked(root,pipeline,views,fdl);own=Path(__file__).resolve().parents[1]
 oracle=load(own/'tools/nvn_pipeline_buffer_validation.py','public_buffer_math');expected=b''.join(oracle.raw(f,p)for f in oracle.FORMATS for p in range(4))
 for rel in('tools/nvn_pipeline_buffer_qualification.py','tools/nvn_pipeline_buffer_validation.py','tools/nvn_pipeline_buffer_runner.py','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_buffer_probe.inc','tests/nvn_pipeline_buffer_cases.h','tests/nvn_pipeline_buffer_pattern.c','android/nvn_pipeline_probe_activity.c'):inputs[str(own/rel)]=sha(own/rel)
 out.mkdir(parents=True);commands=[]
 for cc,san in(('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])):
  compiler=Path(shutil.which(cc)).resolve();inputs[str(compiler)]=sha(compiler);exe=out/('oracle-'+cc);argv=[str(compiler),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,'-I'+str(own/'tests'),str(own/'tests/nvn_pipeline_buffer_pattern.c'),'-lm','-o',str(exe)];r=subprocess.run(argv,text=True,capture_output=True);commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise ValueError(r.stderr)
  r=subprocess.run([str(exe)],capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  if r.returncode or r.stdout!=expected:raise ValueError('independent C/Python raw input+closed original C1 math mismatch')
 # Existing SPIRV-Cross/disassembler, read-only on NORMAL original shader SPV.
 # Outputs are evidence only, never substituted into the normal compiler.
 for stage in selected:
  spv=Path(stage['prefix']+'.spv');inputs[str(spv)]=sha(spv)
  for name,suffix,args in(('spirv-cross','.glsl',['--version','450']),('spirv-dis','.spvasm',[])):
   tool=Path(shutil.which(name)).resolve();inputs[str(tool)]=sha(tool);argv=[str(tool),str(spv),*args];r=subprocess.run(argv,text=True,capture_output=True);commands.append(dict(argv=argv,returncode=r.returncode,stderr=r.stderr))
   if r.returncode:raise ValueError(r.stderr)
   (out/(stage['stage']+suffix)).write_text(r.stdout)
 h=out/'nvn_pipeline_buffer_qualification.h';h.write_text('/* Explicit original source qualification only; no runtime first fallback. */\n#include '+json.dumps(str(ph))+'\n#include '+json.dumps(str(vh))+'\n#define NN_PUBLIC_BUFFER_PAIR '+str(index)+'u\n#define NN_PUBLIC_BUFFER_VIEW_INDEX 0u\n')
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 if any(sha(p)!=h for p,h in inputs.items()):raise ValueError('source/product changed')
 record=dict(schema=1,kind=KIND,producer_sha256=sha(__file__),pipeline_check=str(pipeline),native_views_check=str(views),runtime_fdl_check=str(fdl),runtime_fdl_library=str(library),header=str(h),pair_index=index,pair=pair,original_stage_inputs=[s['envelope']['inputs']for s in selected],formats=list(oracle.FORMATS),index_domain=[0],submissions=48,files=48,data_bytes=3145728,channel_checks=786432,readonly_byte_checks=7077888,full_c1_bytes=256,graphics_control_profile_bytes=2176,oracle_sha256=hashlib.sha256(expected).hexdigest(),inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},gpu_execution=False,android_execution=False,scope='Original bounded C1 Program + real 2D RGBA8 index0 and BUFFER FloatTXF12. HOST input/oracle evidence, not GPU/game. Arbitrary C1 index texture needs actual 2D R32F consumer.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public BUFFER qualifier')
 for p,h in{**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed public BUFFER input '+p)
 _,_,lib,index,pair,selected,locked=checked(root,Path(c['pipeline_check']),Path(c['native_views_check']),Path(c['runtime_fdl_check']))
 if str(lib)!=c['runtime_fdl_library']or index!=c['pair_index']or pair!=c['pair']or c['original_stage_inputs']!=[s['envelope']['inputs']for s in selected]or any(c['inputs'].get(p)!=h for p,h in locked.items()):raise ValueError('incomplete original C1 exact normal provenance')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','pipeline-check','native-views','runtime-fdl-check','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.runtime_fdl_check.resolve(),a.out.resolve()))
