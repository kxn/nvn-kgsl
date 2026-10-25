"""Normal actual-FDL + authored implicit-SAMPLE mip qualification bundle; no GPU."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def module(path,name):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m


def build(config,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();native=Path(__file__).resolve().parents[1]
 config=Path(config).resolve();out=Path(out).resolve();c=json.loads(config.read_text());actual=root/'nvn-native'
 if set(c)!={'schema','kind','runtime_fdl_check','shader_check'}or type(c['schema'])is not int or c['schema']!=1 or c['kind']!='sampler-mip-qualification-inputs-v1':raise ValueError('unknown sampler mip config')
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 fdl_check=Path(c['runtime_fdl_check']).resolve();normal=module(actual/'tools/build_runtime_fdl.py','mip_actual_fdl');fdl=json.loads(fdl_check.read_text());origins=[Path(p).parents[2]for p in fdl['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('ambiguous FDL normal source')
 library,fdl=normal.verify(origins[0],fdl_check)
 for rel in('tools/build_runtime_fdl.py','src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','include/nvn_native/texture_buffer_format.h','tests/runtime_fdl.cc'):
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(actual/rel)for p,h in fdl['inputs'].items()):raise ValueError('different current FDL source '+rel)
 sp=Path(c['shader_check']).resolve();shader=json.loads(sp.read_text())
 if shader.get('kind')!='native-authored-color-sampler_mip-v1'or shader.get('gpu_execution')is not False or shader.get('runtime_compiler')is not False:raise ValueError('unknown authored SAMPLE compiler output')
 inputs={str(config):sha(config),str(fdl_check):sha(fdl_check),str(sp):sha(sp),**fdl['inputs'],**fdl['artifacts'],**shader['inputs'],**shader['artifacts']}
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal source/product changed '+p)
 sys.path.insert(0,str(actual/'tools'));from package_graphics_pipelines import compiler_record
 compiler_record(Path(shader['compiler_record']).parent,actual,{})
 for rel in('tools/compile_sampler_mip.py','shaders/sampler_mip.vert','shaders/sampler_mip.frag'):
  if shader['inputs'].get(str(native/rel))!=sha(native/rel):raise ValueError('authored normal source differs '+rel)
 out.mkdir(parents=True);runs=[];host=Path(fdl['normal_ir3'])/'build';entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));cpp,flags=normal.flags(entry);ninja=Path(shutil.which('ninja')).resolve()
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 command=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for value in command[1:]:
  if skip:skip=False;continue
  if value=='-o':skip=True;continue
  if not value.endswith('.o'):libs.append(value)
 cc=Path(shutil.which('cc')).resolve();common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(actual/'include'),'-I'+str(actual/'third_party/nvn'),'-I'+str(native/'tests')]
 objects=[]
 for name,source in [('layout',native/'tests/sampler_mip_layout.c'),('color',actual/'src/color_target.c')]:
  obj=out/(name+'.o');run([cc,*common,'-c',source,'-o',obj]);objects.append(obj)
 obj=out/'fdl.o';run([cpp,*flags,'-I'+str(actual/'include'),'-Werror','-c',actual/'src/runtime_fdl.cc','-o',obj],host);objects.append(obj)
 exe=out/'actual-mip-layout';run([cpp,*objects,*libs,'-o',exe],host);data=json.loads(run([exe]));(out/'layout.json').write_text(json.dumps(data,indent=2)+'\n')
 from sampler_mip_validation import catalogue
 if data['target']['width']!=64 or data['target']['height']!=64 or len(data['views'])!=2:raise ValueError('actual FDL shape catalogue')
 for v in data['views']:
  if v['format']!=37 or(v['width'],v['height'],v['levels'])!=(127,73,7)or len(v['mips'])!=7 or v['span']<=0:raise ValueError('actual FDL mip domain')
 cases=catalogue(data['views']);header=out/'sampler_mip_qualification.h'
 lines=['/* Authored native implicit SAMPLE + actual FDL, no Maxwell/game. */','#pragma once','#include '+json.dumps(shader['header']),'#include "sampler_mip_cases.h"','#include "nvn_native/runtime_fdl.h"']
 t=data['target'];lines.append('static const union{struct nn_tex_native_view view;unsigned char bytes[sizeof(struct nn_tex_native_view)];}nn_mip_target_storage={.bytes={'+','.join(map(str,bytes.fromhex(t['hex'])))+'}};\n#define NN_SAMPLER_MIP_TARGET nn_mip_target_storage.view')
 lines.append('static const union{struct nn_fdl_image_view view;unsigned char bytes[sizeof(struct nn_fdl_image_view)];}nn_mip_view_storage[]={'+','.join('{.bytes={'+','.join(map(str,bytes.fromhex(v['hex'])))+'}}'for v in data['views'])+'};\n#define NN_SAMPLER_MIP_VIEW(i) (nn_mip_view_storage[(i)].view)')
 lines.append('static const struct nn_sampler_mip_case nn_sampler_mip_cases[]={'+','.join('{'+','.join('.'+k+'='+('{'+','.join(map(str,value))+'}'if isinstance(value,list)else str(value))for k,value in row.items())+'}'for row in cases)+'};')
 header.write_text('\n'.join(lines)+'\n')
 for p in [Path(__file__).resolve(),native/'tools/sampler_mip_validation.py',native/'tests/sampler_mip_layout.c',native/'tests/sampler_mip_cases.h',actual/'src/color_target.c',ninja,cc,host/'compile_commands.json']:inputs[str(p)]=sha(p)
 for value in libs:
  path=(host/value).resolve()
  if path.is_file():inputs[str(path)]=sha(path)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('qualification input changed '+p)
 result=dict(schema=1,kind='raw-native-sampler-mip-qualification-v1',producer_sha256=sha(__file__),gpu_execution=False,nvn_api=False,game_modules_executed=False,header=str(header),shader_check=str(sp),runtime_fdl_library=str(library),target=data['target'],views=data['views'],cases=cases,jobs=288,submissions=576,repeats=2,sequence_offset=45056,coordinate_ubo_binding=5,coordinate_ubo_bytes=64,max_filtered_storage_units=2,nearest_tolerance=0,multilevel_anisotropy=1,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return header,result,{str(out/'check.json'):sha(out/'check.json'),**inputs,**result['artifacts']}


def verify(check,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();native=Path(__file__).resolve().parents[1];check=Path(check).resolve();c=json.loads(check.read_text())
 if c.get('kind')!='raw-native-sampler-mip-qualification-v1'or c.get('producer_sha256')!=sha(__file__)or c.get('gpu_execution')is not False or c.get('jobs')!=288 or c.get('submissions')!=576:raise ValueError('unknown/stale mip qualifier')
 for field in('inputs','artifacts'):
  for p,h in c[field].items():
   if sha(p)!=h:raise ValueError('qualified source/product changed '+p)
 from sampler_mip_validation import catalogue
 if c['cases']!=catalogue(c['views']):raise ValueError('mip state catalogue differs')
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}


if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in('root','check','out'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();print(build(a.check,a.out,root=a.root)[0])
