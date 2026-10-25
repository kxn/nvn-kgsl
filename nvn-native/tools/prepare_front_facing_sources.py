#!/usr/bin/env python3
"""Normal explicit A650 FrontFacing workaround policy, unchanged original sources.
No resource answers, native pairing, runtime compiler, or GPU claim.
"""
import argparse,copy,hashlib,importlib.util,json,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def mod(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def produce(root,checks,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=Path(__file__).resolve().parents[1];validator=mod(native/'tools/shader_fixture.py','ff_fixture');inputs={str(Path(__file__).resolve()):sha(__file__),str(native/'tools/shader_fixture.py'):sha(native/'tools/shader_fixture.py')};selected={};records=[];front=None
 for path in checks:
  c=json.loads(path.read_text());inputs[str(path)]=sha(path)
  if c.get('kind')!='offline-graphics-stage-compilation-v1'or c.get('chip_id')!=0x06050002:raise ValueError('normal A650 complete original stage check required')
  for group in('inputs','artifacts'):
   for p,h in c[group].items():
    if sha(p)!=h:raise ValueError('changed original census input '+p)
    inputs[p]=h
  if front is not None and front!=c['frontend_build']:raise ValueError('mixed frontend identities')
  front=c['frontend_build'];records.append(str(path))
  for key,row in c['stages'].items():
   if 'missing explicit query: QueryHostHasFrontFacingBug'not in row.get('failure',''):continue
   if row['stage']!='fs':raise ValueError('unexpected stage for FrontFacing query')
   if key in selected and selected[key]!=row:raise ValueError('original stage identity conflict')
   selected[key]=row
 if not selected:raise ValueError('no actual first FrontFacing demands')
 # Original recorded frontend, verified against this snapshot's identical
 # translator sources. The new policy is a fixture value, not a backend shim.
 for name,h in front['sources'].items():
  p=root/name
  if sha(p)!=h:raise ValueError('normal frontend source changed '+str(p))
  inputs[str(p)]=h
 for name,h in front['artifacts'].items():
  if sha(name)!=h:raise ValueError('normal frontend artifact changed '+name)
  inputs[name]=h
 dotnet=front['commands'][0][0]
 if sha(dotnet)!=front['dotnet_sha256']:raise ValueError('normal dotnet identity')
 inputs[dotnet]=sha(dotnet);out.mkdir(parents=True);commands=[];stages={};entries=[]
 for key,row in selected.items():
  original=Path(row['fixture']);doc=json.loads(original.read_text());before=validator.validate(doc,original.parent)
  if before['request_sha256']!=key or before!=row['envelope']:raise ValueError('complete original request changed')
  if 'QueryHostHasFrontFacingBug'in doc['state']['host']:raise ValueError('census first failure is not actually absent')
  d=copy.deepcopy(doc);d['state']['host']['QueryHostHasFrontFacingBug']=False
  for blob in d['blobs'].values():blob['path']=str((original.parent/blob['path']).resolve())
  d['provenance'].update(native_front_facing_policy={'chip_id':0x06050002,'QueryHostHasFrontFacingBug':False,'meaning':'Disable translator Intel workaround only; not a GPU support claim','tool_sha256':sha(__file__)},original_fixture=str(original),original_fixture_sha256=sha(original),original_request_sha256=key)
  fixture=out/(key+'.fixture.json');fixture.write_text(json.dumps(d,indent=2)+'\n');envelope=validator.validate(d,out)
  if envelope['inputs']!=before['inputs']or envelope['request_sha256']==key:raise ValueError('policy identity/source separation')
  prefix=out/envelope['request_sha256'][:16];argv=[dotnet,front['entrypoint'],str(fixture),str(prefix)];r=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));passed=r.returncode==0
  if 'missing explicit query: QueryHostHasFrontFacingBug'in r.stderr:raise ValueError('explicit policy was not consumed')
  if passed:
   argv=['/usr/bin/spirv-val',str(prefix)+'.spv'];v=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=v.returncode,stdout=v.stdout,stderr=v.stderr));passed=v.returncode==0
  stage=dict(original_request_sha256=key,fixture=str(fixture),fixture_sha256=sha(fixture),envelope=envelope,stage='fs',prefix=str(prefix),frontend_passed=passed,failure=r.stderr,original_source_unchanged=True)
  if passed:stage['resources']=json.loads(Path(str(prefix)+'.resources.json').read_text())
  stages[envelope['request_sha256']]=stage;entries.append(dict(fixture=str(fixture),fixture_sha256=sha(fixture),stage='fs',source_id=key))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed during frontend compile '+p)
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 c=dict(schema=1,kind='original-front-facing-explicit-policy-v1',passed=True,gpu_execution=False,native_pairs_created=False,chip_id=0x06050002,original_checks=records,front_facing_policy={'QueryHostHasFrontFacingBug':False,'platform':'A650','GPU_support_proven':False},stages=stages,frontend_successes=sum(s['frontend_passed']for s in stages.values()),first_failures={k:s['failure']for k,s in stages.items()if not s['frontend_passed']},inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',type=Path,required=True);p.add_argument('--check',action='append',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(produce(a.root.resolve(),[p.resolve()for p in a.check],a.out.resolve()))
