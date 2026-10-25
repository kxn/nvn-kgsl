#!/usr/bin/env python3
"""Normal public canonical GOB22 qualifier; original shaders, no device execution."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
KIND='public-nvn-gob-color-targets-qualification-v1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def checked(root,pipeline,fdl,kernel):
 native=root/'nvn-native'
 raw=module(native/'tools/color_target_qualification.py','public_color_original')
 header,p,index,pair,library,f,inputs=raw.verified(root,pipeline,fdl)
 block=module(native/'tools/compile_block_copy.py','public_color_block');block.verify(root,kernel);b=json.loads(kernel.read_text())
 inputs.update(b['inputs']);inputs.update(b['artifacts']);inputs[str(kernel)]=sha(kernel)
 return header,index,pair,library,Path(b['header']),inputs

def produce(root,pipeline,fdl,kernel,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 header,index,pair,library,bh,inputs=checked(root,pipeline,fdl,kernel);out.mkdir(parents=True);own=Path(__file__).resolve().parents[1]
 raw=module(root/'nvn-native/tools/color_target_qualification.py','public_color_oracle')
 raw.produce(root,pipeline,fdl,out/'oracle');oracle=json.loads((out/'oracle/check.json').read_text());inputs.update(oracle['inputs']);inputs.update(oracle['artifacts']);inputs[str(out/'oracle/check.json')]=sha(out/'oracle/check.json')
 commands=[];metadata=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  tool=Path(shutil.which(cc)).resolve();inputs[str(tool)]=sha(tool);binary=out/('metadata-'+cc)
  argv=[str(tool),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,'-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),str(own/'tests/public_color_target_metadata.c'),str(own/'src/texture_source.c'),'-o',str(binary)]
  p=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
  if p.returncode:raise ValueError(p.stderr)
  p=subprocess.run([binary],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=[str(binary)],returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
  if p.returncode:raise ValueError(p.stderr)
  metadata.append(json.loads(p.stdout))
 if metadata[0]!=metadata[1]or len(metadata[0])!=22:raise ValueError('actual canonical metadata disagrees')
 formats=[]
 for native,source in zip(oracle['formats'],metadata[0]):
  if native['format']!=source['format']or native['cpp']!=source['cpp']:raise ValueError('actual source/native format bytes differ')
  formats.append(dict(native,**source))
 h=out/'nvn_pipeline_color_qualification.h'
 h.write_text('/* Qualification source selection only; runtime still exact source/typed recipe. */\n#include '+json.dumps(str(header))+'\n#include '+json.dumps(str(bh))+'\n#define NN_PUBLIC_COLOR_BLOCK_IMAGE NVN_COMPILED_IMAGE\n#define NN_PIPELINE_QUAL_PAIR_INDEX '+str(index)+'u\nstruct nn_public_color_shape{unsigned format,cpp,storage_bytes,pool_bytes,gob_pitch,bh;};\nstatic const struct nn_public_color_shape NN_PUBLIC_COLOR_SHAPES[22]={\n'+''.join('{'+','.join(str(s[k])+'u'for k in('format','cpp','storage_bytes','pool_bytes','gob_pitch','block_height_log2'))+'},\n'for s in formats)+'};\n')
 for rel in ('tools/nvn_pipeline_color_qualification.py','tools/nvn_pipeline_color_validation.py','tools/nvn_pipeline_color_runner.py','tests/public_color_target_metadata.c','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_color_probe.inc','tests/color_target_cases.h','src/texture_source.c','android/nvn_pipeline_probe_activity.c'):
  inputs[str(own/rel)]=sha(own/rel)
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 if any(sha(p)!=v for p,v in inputs.items()):raise ValueError('source changed during producer')
 c=dict(schema=1,kind=KIND,producer_sha256=sha(__file__),pipeline_check=str(pipeline),runtime_fdl_check=str(fdl),block_copy_check=str(kernel),runtime_fdl_library=str(library),header=str(h),pair_index=index,pair=pair,formats=formats,oracle=oracle['oracle'],expected=oracle['expected'],width=73,height=41,submissions=132,files=132,replays=66,raw_arena_snapshot_bytes=131072,raw_readonly_bytes=139264,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},gpu_execution=False,android_execution=False,scope='Authored public NVN canonical GOB22 overwrite/blend/mask, original color shader full source/control, Copy+Call and same-handle replay. No GPU/game claim from producer.')
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public GOB22 qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('public GOB22 input changed '+p)
 h,index,pair,library,bh,locked=checked(root,Path(c['pipeline_check']),Path(c['runtime_fdl_check']),Path(c['block_copy_check']))
 if index!=c['pair_index']or pair!=c['pair']or str(library)!=c['runtime_fdl_library']or any(c['inputs'].get(p)!=v for p,v in locked.items()):raise ValueError('normal GOB22 provenance incomplete')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','pipeline-check','runtime-fdl-check','block-copy-check','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.runtime_fdl_check.resolve(),a.block_copy_check.resolve(),a.out.resolve()))
