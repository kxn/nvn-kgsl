"""Normal authored3D texture-unit qualification bundle; no GPU execution."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def build(config,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();n=root/'nvn-native';config=Path(config).resolve();out=Path(out).resolve();c=json.loads(config.read_text())
 if set(c)!={'schema','kind','kernel_check','runtime_fdl_check','shader_check'}or c['schema']!=1 or c['kind']!='volume-texture-qualification-inputs-v1':raise ValueError('unknown volume texture config')
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local output required')
 out.mkdir(parents=True);owner_config=out/'owner-inputs.json';owner_config.write_text(json.dumps(dict(schema=1,kind='volume-shadow-qualification-inputs-v1',kernel_check=c['kernel_check'],runtime_fdl_check=c['runtime_fdl_check']),indent=2)+'\n')
 shadow=module(n/'tools/volume_shadow_bundle.py','texture_actual_shadow_bundle');header,record,inputs=shadow.build(owner_config,out/'owner',root=root)
 sp=Path(c['shader_check']).resolve();sh=json.loads(sp.read_text())
 if sh.get('kind')!='authored-native-volume-texture-fetch-v1'or sh.get('passed')is not True or sh.get('gpu_execution')is not False or sh.get('original_game_shader')is not False:raise ValueError('unknown actual3D shader producer')
 for p,h in {**sh['inputs'],**sh['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed authored compiler/source '+p)
  inputs[p]=h
 for rel in ('tools/compile_volume_fetch.py','include/nvn_native/pipeline_native_program.h','include/nvn_native/pipeline_texture_types.h','tests/shaders/volume_fetch.vert','tests/shaders/volume_fetch.frag','tests/shaders/volume_size.frag'):
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in sh['inputs'].items()):raise ValueError('different current normal shader source '+rel)
 inputs[str(sp)]=sha(sp);inputs[str(config)]=sha(config);fdl=json.loads(Path(c['runtime_fdl_check']).read_text());host=Path(fdl['normal_ir3'])/'build';normal=module(n/'tools/build_runtime_fdl.py','texture_normal_fdl');entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));cpp,flags=normal.flags(entry);ninja=Path(shutil.which('ninja')).resolve();runs=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 link=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 sources=[n/'tests/volume_texture_layout.c',n/'src/texture_volume_shadow.c',n/'src/texture_source.c',n/'src/block_copy.c'];objects=[];cc=Path(shutil.which('cc')).resolve()
 for i,s in enumerate(sources):
  o=out/(str(i)+'.o');run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'tests'),'-c',s,'-o',o]);objects.append(o)
 o=out/'fdl.o';run([cpp,*flags,'-I'+str(n/'include'),'-Werror','-ffunction-sections','-fdata-sections','-c',n/'src/runtime_fdl.cc','-o',o],host);objects.append(o);exe=out/'actual-volume-target-layout';run([cpp,*objects,*libs,'-Wl,--gc-sections','-o',exe],host);targets=json.loads(run([exe]));(out/'targets.json').write_text(json.dumps(targets,indent=2)+'\n')
 from volume_texture_validation import validate_catalogue
 validate_catalogue(record['cases'],targets)
 generated=out/'volume_texture_bundle.h';text='/* Authored actual FDL+native compiler facts; no game shader/control. */\n#include '+json.dumps(str(header))+'\n#include '+json.dumps(sh['header'])+'\nstatic const unsigned char nn_volume_texture_target_bytes[18][sizeof(struct nn_tex_native_view)]={\n'+''.join('{'+','.join(map(str,bytes.fromhex(x['target_hex'])))+'},\n'for x in targets)+'};\n';generated.write_text(text)
 for p in [Path(__file__).resolve(),n/'tools/volume_texture_validation.py',n/'tests/volume_texture_cases.h',n/'tests/volume_texture_pattern.h',*sources,ninja,cc,host/'compile_commands.json']:inputs[str(p)]=sha(p)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('bundle input changed '+p)
 result=dict(schema=1,kind='native-volume-texture-qualification-v1',gpu_execution=False,android_execution=False,original_game_shader=False,cases=record['cases'],targets=targets,owner_check=record,shader_check=sh,runtime_fdl_library=record['runtime_fdl_library'],inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');inputs[str(out/'check.json')]=sha(out/'check.json');return generated,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','check','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.check,a.out,root=a.root)[0])
