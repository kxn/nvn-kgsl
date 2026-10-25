#!/usr/bin/env python3
"""Authored true3D owner GPU catalogue from actual normal FDL/kernel inputs."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(config,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();n=root/'nvn-native';config=Path(config).resolve();out=Path(out).resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local volume-shadow bundle required')
 c=json.loads(config.read_text())
 if set(c)!={'schema','kind','kernel_check','runtime_fdl_check'}or c['schema']!=1 or c['kind']!='volume-shadow-qualification-inputs-v1':raise ValueError('unknown volume-shadow inputs')
 kp=Path(c['kernel_check']).resolve();fp=Path(c['runtime_fdl_check']).resolve();k=json.loads(kp.read_text());f=json.loads(fp.read_text())
 ko=[Path(p).parents[2]for p in k['inputs']if p.endswith('/nvn-native/tools/compile_volume_copy.py')];fo=[Path(p).parents[2]for p in f['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(ko)!=1 or len(fo)!=1:raise ValueError('ambiguous normal metadata source')
 normal=load(n/'tools/build_runtime_fdl.py','shadow_normal_fdl');library,f=normal.verify(fo[0],fp)
 load(n/'tools/compile_volume_copy.py','shadow_normal_volume').verify(ko[0],kp)
 for doc,files in [(f,['src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','tests/runtime_fdl.cc','tools/build_runtime_fdl.py']),(k,['shaders/volume_copy.comp','tools/compile_volume_copy.py'])]:
  for rel in files:
   if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in doc['inputs'].items()):raise ValueError('current source differs '+rel)
 if 'volume-v1'not in f['features']or k['program']['local_size']!=[8,8,1]or k['program']['ssbo_table_slots']!=2:raise ValueError('missing volume feature')
 inputs={str(config):sha(config),str(kp):sha(kp),str(fp):sha(fp),**f['inputs'],**f['artifacts'],**k['inputs'],**k['artifacts']}
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('changed normal input '+p)
 out.mkdir(parents=True);runs=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 host=Path(f['normal_ir3'])/'build';ninja=Path(shutil.which('ninja')).resolve();entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));cpp,flags=normal.flags(entry)
 link=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 sources=[n/'tests/volume_shadow_layout.c',n/'src/texture_volume_shadow.c',n/'src/texture_source.c',n/'src/block_copy.c'];objects=[];cc=Path(shutil.which('cc')).resolve()
 for i,src in enumerate(sources):
  obj=out/(str(i)+'.o');run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'tests'),'-c',src,'-o',obj]);objects.append(obj)
 obj=out/'fdl.o';run([cpp,*flags,'-I'+str(n/'include'),'-Werror','-ffunction-sections','-fdata-sections','-c',n/'src/runtime_fdl.cc','-o',obj],host);objects.append(obj)
 utility=out/'volume-shadow-layout';run([cpp,*objects,*libs,'-Wl,--gc-sections','-o',utility],host);cases=json.loads(run([utility]))
 from volume_shadow_validation import validate_cases
 validate_cases(cases);kernel=out/'volume_shadow.native.h';kernel.write_bytes(Path(k['header']).read_bytes());header=out/'volume_shadow_bundle.h'
 lines=['/* Authored actual FDL catalogue; normal volume kernel unchanged. */','#include "volume_shadow_cases.h"','#include "volume_shadow.native.h"','static const struct nn_volume_shadow_case nn_volume_shadow_cases[]={']
 for x in cases:
  fields=('id','format','width','height','depth','levels','base_level','level_count','flags','canonical_bo_bytes','native_bo_bytes','canonical_offset')
  lines.append('{'+','.join('.'+v+'='+str(x[v])for v in fields)+',.source={'+','.join(map(str,bytes.fromhex(x['source_hex'])))+'},.native={'+','.join(map(str,bytes.fromhex(x['native_hex'])))+'}},')
 lines.append('};');header.write_text('\n'.join(lines)+'\n');(out/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')
 for p in [Path(__file__).resolve(),n/'tools/volume_shadow_validation.py',n/'tests/volume_shadow_cases.h',n/'include/nvn_native/texture_volume_shadow.h',n/'include/nvn_native/texture_volume.h',n/'include/nvn_native/texture_source.h',n/'src/volume_copy.inc',n/'src/texture_volume.inc',n/'tools/compute_driverparams.py',*sources,cc,ninja,host/'compile_commands.json']:
  inputs[str(p)]=sha(p)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('volume-shadow closure changed '+p)
 result=dict(schema=1,kind='native-volume-shadow-qualification-v1',gpu_execution=False,android_execution=False,cases=cases,runtime_fdl_library=str(library),kernel_binary_bytes=k['program']['binary_bytes'],kernel_check=str(kp),runtime_fdl_check=str(fp),inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');inputs[str(out/'check.json')]=sha(out/'check.json');return header,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--check',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(build(a.check,a.out,root=a.root)[0])
