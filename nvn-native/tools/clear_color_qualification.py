#!/usr/bin/env python3
"""Normal public ClearColor inputs; original tools and HOST-only Mesa oracle."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def proof(path,kind):
 c=json.loads(path.read_text())
 if c.get('kind')!=kind or c.get('passed',True)is not True or c.get('gpu_execution',c.get('GPU_execution'))is not False:raise ValueError('wrong normal proof '+str(path))
 for p,h in {**c['inputs'],**c.get('artifacts',{})}.items():
  if sha(p)!=h:raise ValueError('normal dependency changed '+p)
 return c
def verified(root,constant,kernel,call):
 n=root/'nvn-native';c=proof(constant,'native-clear-color-constants-v1');k=proof(kernel,'native-clear-color-v1');v=json.loads(call.read_text())
 if v.get('passed')is not True or v.get('GPU_execution')is not False:raise ValueError('normal public Call proof required')
 for p,h in {**v['inputs'],**v.get('artifacts',{})}.items():
  if sha(p)!=h:raise ValueError('Call proof changed '+p)
 origin=[Path(p).parents[2]for p in k['inputs']if p.endswith('/nvn-native/tools/compile_clear_color.py')]
 if len(origin)!=1:raise ValueError('one normal kernel producer required')
 load(n/'tools/compile_clear_color.py','clear_kernel_verify').verify(origin[0],kernel)
 for record,rels in[(c,['include/nvn_native/clear_color.h','src/clear_color_pattern.c','tools/check_clear_color.py']),
   (k,['tools/compile_clear_color.py','shaders/clear_color.comp']),
   (v,['src/nvn.c','src/resource_target_clear.inc','src/clear_color_record.inc','src/nvn_command_projection.inc','src/nvn_command_copy_projection.inc','src/clear_color.c','include/nvn_native/clear_color.h'])]:
  for rel in rels:
   values=[h for p,h in record['inputs'].items()if p.endswith('/nvn-native/'+rel)]
   if values!=[sha(n/rel)]:raise ValueError('current qualified source differs '+rel)
 inputs={str(constant):sha(constant),str(kernel):sha(kernel),str(call):sha(call),**c['inputs'],**c['artifacts'],**k['inputs'],**k['artifacts'],**v['inputs'],**v['artifacts']}
 return c,k,v,inputs
def produce(root,constant,kernel,call,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 c,k,v,inputs=verified(root,constant,kernel,call);n=root/'nvn-native';out.mkdir(parents=True);commands=[]
 def run(argv,cwd=None):
  r=subprocess.run(list(map(str,argv)),cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 for rel in ['tests/clear_color_cases.h','tests/clear_color_layout.c','tests/clear_color_oracle.c','tests/clear_color_probe.c','android/clear_color_activity.c','android/public_copy_activity.c','tools/clear_color_qualification.py','tools/clear_color_validation.py','tools/clear_color_runner.py','tools/pipeline_runner.py','tools/run_android_probe.py']:
  inputs[str(n/rel)]=sha(n/rel)
 utility=out/'layout';run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'tests'),n/'tests/clear_color_layout.c',n/'src/texture_source.c','-o',utility]);cases=json.loads(run([utility]));from clear_color_validation import validate_cases
 validate_cases(cases);manifest=out/'cases.json';manifest.write_text(json.dumps(cases,indent=2)+'\n')
 records=[Path(p)for p in k['inputs']if Path(p).name=='build-record.json'and json.loads(Path(p).read_text())==k['compiler_build']]
 if len(records)!=1:raise ValueError('one actual compiler record required')
 ir3=records[0].parent;host=ir3/'build';entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/format/u_format_other.c'))
 _,flags=load(n/'tools/check_clear_color.py','clear_oracle_flags').flags(entry);lib=host/'src/util/libmesa_util.a';oracle=out/'oracle';inputs[str(lib)]=sha(lib)
 run(['gcc',*flags,'-Werror','-I'+str(n/'include'),'-I'+str(n/'tests'),'-I'+str(n/'third_party/nvn'),n/'tests/clear_color_oracle.c',n/'src/texture_source.c',lib,'-lm','-ldl','-lpthread','-o',oracle],host)
 expected=out/'expected';meta=json.loads(run([oracle,expected]));assert meta['cases']==67 and meta['files']==536 and meta['HOST_expected_only']is True
 header=out/'clear_color_qualification.h';header.write_text('/* Normal immutable authored MetaClear kernel only. */\n#include '+json.dumps(k['header'])+'\n')
 for p in [Path(shutil.which('gcc')).resolve(),Path(__file__).resolve(),n/'src/texture_source.c',n/'src/texture_volume.inc',n/'tests/public_copy_cases.h',header,manifest]:inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('qualification input changed '+p)
 result=dict(schema=1,kind='public-nvn-clear-color-qualification-v1',producer_sha256=sha(__file__),gpu_execution=False,android_execution=False,constant_check=str(constant),kernel_check=str(kernel),call_check=str(call),constant_library=c['static_library'],header=str(header),case_manifest=str(manifest),cases=cases,expected=str(expected),expected_bytes=meta['bytes'],files=536,submissions=536,origins=2,phases=2,repeats=2,commands=commands,inputs=inputs,scope='HOST expected images only; public normal/Call 22format GPU qualification candidate, no GPU execution')
 result['artifacts']={str(p):sha(p)for p in out.rglob('*')if p.is_file()};(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=proof(check,'public-nvn-clear-color-qualification-v1')
 if c.get('producer_sha256')!=sha(__file__)or c.get('files')!=536 or c.get('submissions')!=536:raise ValueError('stale/unknown public clear package')
 verified(root,Path(c['constant_check']),Path(c['kernel_check']),Path(c['call_check']))
 from clear_color_validation import validate_cases
 validate_cases(c['cases'])
 if c['cases']!=json.loads(Path(c['case_manifest']).read_text()):raise ValueError('clear case manifest mismatch')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['root','constant-check','kernel-check','call-check','out']:p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.constant_check.resolve(),a.kernel_check.resolve(),a.call_check.resolve(),a.out.resolve()))
