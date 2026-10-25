#!/usr/bin/env python3
"""Normal raw-native22 COLOR RT qualification inputs; HOST build, never GPU run."""
import argparse,hashlib,importlib.util,json,os,shlex,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def verified(root,pipeline,fdl):
 native=root/'nvn-native';p=json.loads(pipeline.read_text())
 if p.get('kind')!='native-graphics-pipeline-recipes-v1'or p.get('chip')!=0x06050002 or p.get('gmem_bytes')!=1179648 or p.get('draw_template')is not False:raise ValueError('normal A650 pipeline package required')
 package=load(native/'tools/package_graphics_pipelines.py','color_native_package');header=package.build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,True)
 fmodule=load(native/'tools/build_runtime_fdl.py','color_fdl_package');library,f=fmodule.verify(root,fdl)
 matches=[(i,v)for i,v in enumerate(p['pairs'])if v['vs_index']==2 and v['fs_index']==3]
 if len(matches)!=1:raise ValueError('explicit authored original color source pair absent/ambiguous')
 index,pair=matches[0];m=pair['metadata']
 if m['vertex_inputs']!=2 or m['texture_mask']or m['semantic_sampler_mask']or m['fragcoord_mask']or m['has_kill']or[s['code_bytes']for s in m['stages']]!=[384,128]:raise ValueError('original color pair resource contract')
 inputs={str(pipeline):sha(pipeline),str(fdl):sha(fdl),**p['inputs'],**p['outputs'],**f['inputs'],**f['artifacts']}
 return header,p,index,pair,library,f,inputs

def produce(root,pipeline,fdl,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 header,p,index,pair,library,f,inputs=verified(root,pipeline,fdl);out.mkdir(parents=True);native=root/'nvn-native';commands=[]
 def run(argv,cwd=None):
  r=subprocess.run(list(map(str,argv)),cwd=cwd,text=True,capture_output=True);commands.append({'argv':list(map(str,argv)),'cwd':str(cwd)if cwd else None,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('qualification command failed\n'+r.stderr)
  return r
 for rel in ['tools/color_target_qualification.py','tools/color_target_validation.py','tools/color_target_runner.py','tests/d2r_bridge/color_target_packets.c','tests/test_color_target_validation.py','tests/color_target_oracle.cc','tests/color_target_cases.h','tests/color_target_probe.inc','tests/pipeline_probe.c','src/color_target.c','include/nvn_native/color_target.h','src/pipeline.c','include/nvn_native/pipeline.h']:
  q=native/rel;inputs[str(q)]=sha(q)
 # Existing normal HOST Mesa flags/libraries; pixel pack/unpack stays HOST ONLY.
 record=json.loads((fdl.parent/'commands.json').read_text());template=next(v for v in record if v['argv'][0]=='g++'and str(native/'tests/runtime_fdl.cc')in v['argv'])
 common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
 obj=out/'color_target.o';run(['gcc',*common,'-c',native/'src/color_target.c','-o',obj])
 argv=[]
 for word in template['argv']:
  if word==str(native/'tests/runtime_fdl.cc'):argv.extend([str(native/'tests/color_target_oracle.cc'),str(obj)])
  elif word.startswith('-fsanitize=')or word=='-fno-omit-frame-pointer':continue
  else:argv.append(word)
 oracle=out/'color-oracle';argv[argv.index('-o')+1]=str(oracle);run(argv,template['cwd'])
 # Pin original format utility source/tool closure, including codec tables used
 # only by this HOST oracle. None enters the Android metadata/runtime library.
 manifest_path=Path(f['normal_ir3'])/'source-hashes.json';manifest=json.loads(manifest_path.read_text());inputs[str(manifest_path)]=sha(manifest_path)
 source=Path(f['source'])
 for q in (source/'src/util/format').rglob('*'):
  if q.is_file()and '__pycache__'not in q.parts:
   rel=str(q.relative_to(source))
   if rel not in manifest or sha(q)!=manifest[rel]:raise ValueError('original format utility source mismatch '+str(q))
   inputs[str(q)]=sha(q)
 generated=out/'expected';metadata=json.loads(run([oracle,'generate',generated]).stdout)
 if metadata.get('cases')!=132 or len(metadata.get('formats',[]))!=22:raise ValueError('expected format/case matrix')
 h=out/'color_target_qualification.h';h.write_text('/* Verified original color shader, authored RT qualifier only. */\n#include "'+str(header)+'"\n#define NN_PIPELINE_QUAL_PAIR_INDEX '+str(index)+'u\n')
 for q,v in inputs.items():
  if sha(q)!=v:raise ValueError('qualification input changed '+q)
 c={'schema':1,'kind':'raw-native-color-target-qualification-v1','producer_sha256':sha(__file__),'pipeline_check':str(pipeline),'runtime_fdl_check':str(fdl),'runtime_fdl_library':str(library),'header':str(h),'oracle':str(oracle),'expected':str(generated),'formats':metadata['formats'],'width':73,'height':41,'states':3,'repeats':2,'jobs':66,'submissions':132,'files':132,'sequence_offset':45056,'sequence_bytes_exempt':4,'selected_pair':index,'pair':pair,'native_vs_ubo_binding':1,'original_maxwell_cb':3,'ubo_bytes':64,'pixel_checks_on_host':True,'gpu_execution':False,'inputs':inputs,'commands':commands,'scope':__doc__}
 c['artifacts']={str(q):sha(q)for q in out.rglob('*')if q.is_file()};(out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!='raw-native-color-target-qualification-v1'or c.get('producer_sha256')!=sha(__file__)or c.get('gpu_execution')is not False:raise ValueError('unknown/stale color qualifier')
 for p,h in{**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('color qualifier dependency changed '+p)
 verified(root,Path(c['pipeline_check']),Path(c['runtime_fdl_check']))
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__)
 for n in('root','pipeline-check','runtime-fdl-check','out'):a.add_argument('--'+n,type=Path,required=True)
 x=a.parse_args();print(produce(x.root.resolve(),x.pipeline_check.resolve(),x.runtime_fdl_check.resolve(),x.out.resolve()))
