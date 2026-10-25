"""Normal authored native BUFFER qualification bundle, no GPU execution."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def build(config,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();n=root/'nvn-native';config=Path(config).resolve();out=Path(out).resolve();c=json.loads(config.read_text())
 if set(c)!={'schema','kind','runtime_fdl_check','shader_check'}or c['schema']!=1 or c['kind']!='buffer-texture-qualification-inputs-v1':raise ValueError('unknown BUFFER config')
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local output required')
 fdl_check=Path(c['runtime_fdl_check']).resolve();normal=module(n/'tools/build_runtime_fdl.py','buffer_actual_fdl');fdl=json.loads(fdl_check.read_text());origins=[Path(p).parents[2]for p in fdl['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('ambiguous actual FDL source')
 library,fdl=normal.verify(origins[0],fdl_check)
 for rel in ('tools/build_runtime_fdl.py','src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','include/nvn_native/texture_buffer_format.h','tests/runtime_fdl.cc'):
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in fdl['inputs'].items()):raise ValueError('different current FDL source '+rel)
 sp=Path(c['shader_check']).resolve();sh=json.loads(sp.read_text())
 if sh.get('kind')!='authored-native-buffer-texture-fetch-v1'or sh.get('passed')is not True or sh.get('gpu_execution')is not False or sh.get('original_game_shader')is not False:raise ValueError('unknown authored BUFFER compiler output')
 inputs={str(config):sha(config),str(fdl_check):sha(fdl_check),str(sp):sha(sp),**fdl['inputs'],**fdl['artifacts'],**sh['inputs'],**sh['artifacts']}
 for rel in ('tools/compile_buffer_fetch.py','include/nvn_native/pipeline_native_program.h','include/nvn_native/pipeline_texture_types.h','include/nvn_native/texture_buffer_sample.h','tests/shaders/buffer_fetch.vert','tests/shaders/buffer_fetch.frag','tests/shaders/buffer_size.frag'):
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in sh['inputs'].items()):raise ValueError('different authored shader source '+rel)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 out.mkdir(parents=True);runs=[];host=Path(fdl['normal_ir3'])/'build';entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));cpp,flags=normal.flags(entry);ninja=Path(shutil.which('ninja')).resolve()
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 link=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 cc=Path(shutil.which('cc')).resolve();obj=out/'layout.o';run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'tests'),'-DNVN_BUFFER_SHADER_HEADER='+json.dumps(sh['header']),'-c',n/'tests/buffer_texture_layout.c','-o',obj]);fdl_obj=out/'fdl.o';run([cpp,*flags,'-I'+str(n/'include'),'-Werror','-c',n/'src/runtime_fdl.cc','-o',fdl_obj],host)
 # The case catalogue uses the pure source-format numeric classification only.
 classifier=out/'classify.o';run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-c',n/'src/texture_buffer_sample.c','-o',classifier]);exe=out/'actual-buffer-cases';run([cpp,obj,fdl_obj,classifier,*libs,'-Wl,--gc-sections','-o',exe],host);data=json.loads(run([exe]));(out/'cases.json').write_text(json.dumps(data,indent=2)+'\n')
 from buffer_texture_validation import validate_catalogue
 validate_catalogue(data['cases'],data['target'])
 generated=out/'buffer_texture_bundle.h';text='/* Actual normal FDL+authored shader artifacts, no NVN source identity. */\n#include '+json.dumps(sh['header'])+'\n#include "buffer_texture_cases.h"\nstatic const struct nn_buffer_texture_case nn_buffer_texture_cases[]={\n'+''.join('{'+','.join(str(v[k])+'u'for k in ('id','format','elements','cpp','prefix','float_result','source_offset','source_bytes'))+'},\n'for v in data['cases'])+'};\nstatic const union{struct nn_tex_native_view view;unsigned char bytes[sizeof(struct nn_tex_native_view)];}nn_buffer_target_storage={.bytes={'+','.join(map(str,bytes.fromhex(data['target']['hex'])))+'}};\n#define nn_buffer_texture_target nn_buffer_target_storage.view\n';generated.write_text(text)
 for p in [Path(__file__).resolve(),n/'tools/buffer_texture_validation.py',n/'tests/buffer_texture_layout.c',n/'tests/buffer_texture_cases.h',n/'tests/volume_texture_cases.h',n/'src/texture_buffer_sample.c',ninja,cc,host/'compile_commands.json']:inputs[str(p)]=sha(p)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('bundle changed '+p)
 result=dict(schema=1,kind='native-buffer-texture-qualification-v1',gpu_execution=False,android_execution=False,original_game_shader=False,**data,shader_check=sh,runtime_fdl_library=str(library),inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');inputs[str(out/'check.json')]=sha(out/'check.json');return generated,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','check','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.check,a.out,root=a.root)[0])
