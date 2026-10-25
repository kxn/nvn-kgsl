#!/usr/bin/env python3
"""HOST-only actual FDL A650 buffer descriptors; no GPU runtime or image copy."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def produce(root,ir3,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 spec=importlib.util.spec_from_file_location('compiler_inputs',root/'nvn-native/tools/compute_driverparams.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 record,inputs,_,count=m.compiler_inputs(root,ir3);out.mkdir(parents=True);runs=[];build=ir3/'build'
 source=root/'nvn-native/tests/texture_buffer_metadata.cc';inputs[str(source)]=sha(source);h=root/'nvn-native/include/nvn_native/texture_buffer.h';inputs[str(h)]=sha(h);k=root/'nvn-native/include/nvn_native/kgsl.h';inputs[str(k)]=sha(k);inputs[str(Path(__file__).resolve())]=sha(__file__)
 def run(argv,ok=True):
  p=subprocess.run(list(map(str,argv)),cwd=build,text=True,capture_output=True);runs.append(dict(argv=list(map(str,argv)),cwd=str(build),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
  if ok and p.returncode:raise RuntimeError(p.stderr)
  if not ok and not p.returncode:raise ValueError('negative FDL metadata input accepted')
  return p.stdout
 cc=build/'compile_commands.json';inputs[str(cc)]=sha(cc);entry=next(x for x in json.loads(cc.read_text())if x['file'].endswith('/fd6_view.cc'));args=shlex.split(entry['command']);flags=[];skip=False
 for v in args[1:]:
  if skip:skip=False;continue
  if v in ('-o','-MQ','-MF'):skip=True;continue
  if v in ('-MD','-c')or v.endswith('/fd6_view.cc'):continue
  flags.append(v)
 obj=out/'buffer-view.o';run([args[0],*flags,'-I'+str(root/'nvn-native/include'),'-Werror','-c',source,'-o',obj])
 ninja=next(x for x in record['tools']if Path(x).name=='ninja');link=shlex.split(run([ninja,'-C',build,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if v.endswith('.o'):continue
  libs.append(v)
 exe=out/'buffer-view';run([link[0],'-o',exe,obj,*libs]);recipes=[];views=[]
 for fmt in ('rgba8','r32f'):
  recipe=json.loads(run([exe,fmt]));recipes.append(recipe)
  for elements in (1,7,32767,32768,65537,0x3fffffff):
   for address in (0x100000,0x100004,0x10003c,0x100000000c):
    row=json.loads(run([exe,fmt,elements*4,address]));views.append(row)
  views.append(json.loads(run([exe,fmt,4,(1<<49)-4])))
 negatives=[('rgba8',0,0),('rgba8',3,0),('rgba8',4,1),('rgba8',1<<32,0),('uint',4,0),('r32f',4,-1),('r32f',4,1<<49),('r32f',8,(1<<49)-4)]
 for bad in negatives:run([exe,*bad],False)
 header=out/'texture_buffer_recipes.h';header.write_text('/* Generated actual FDL; exact source/check fingerprints required. */\n#include "nvn_native/texture_buffer.h"\nstatic const struct nn_tex_buffer_recipe NN_TEXTURE_BUFFER_RECIPES[]={\n'+',\n'.join('{'+','.join('.'+k+'='+('{' + ','.join(str(x)+'u' for x in value) + '}' if isinstance(value,list) else str(value)+'ull' if k=='address_mask' else str(value)+'u') for k,value in r.items() if not k.startswith('request_'))+'}' for r in recipes)+'\n};\n#define NN_TEXTURE_BUFFER_RECIPE_COUNT 2u\n')
 for v in libs:
  p=(build/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for cc in (args[0],link[0]):
  p=Path(shutil.which(cc)).resolve();inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 result=dict(schema=1,kind='native-typed-buffer-recipes-host-v1',gpu_execution=False,actual_game_bindings=False,compiler_build=str(ir3/'build-record.json'),compiler_source_files=count,inputs=inputs,recipes=recipes,views=views,negative_checks=len(negatives),commands=runs,artifacts={str(exe):sha(exe),str(obj):sha(obj),str(header):sha(header)})
 p=out/'check.json';p.write_text(json.dumps(result,indent=2)+'\n');return p
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--ir3',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(produce(a.root.resolve(),a.ir3.resolve(),a.out.resolve()))
