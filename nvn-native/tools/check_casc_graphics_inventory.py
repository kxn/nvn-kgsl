#!/usr/bin/env python3
"""Verify normal original CASC inventory/full fixture identities and actual compile outcomes.
HOST filesystem/compiler evidence only, never an assertion that unknown queries or GPU passed.
"""
import argparse,hashlib,importlib.util,json,subprocess,sys
from pathlib import Path

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 a=argparse.ArgumentParser(description=__doc__)
 for n in('root','inventory','out'):a.add_argument('--'+n,type=Path,required=True)
 a.add_argument('--compiled',type=Path,action='append',default=[]);x=a.parse_args();root=x.root.resolve();out=x.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):a.error('fresh ignored output required')
 out.mkdir(parents=True);inputs={};runs=[];proof={'passed':False,'gpu_execution':False,'scope':__doc__}
 def pin(p,h=None):
  p=Path(p).resolve();actual=sha(p)
  if h is not None and actual!=h:raise ValueError('input changed '+str(p))
  inputs[str(p)]=actual
 try:
  pin(__file__);pin(x.inventory);inv=json.loads(x.inventory.read_text())
  if inv.get('kind')!='declared-graphics-container-inventory-v1'or inv.get('resource_count')!=len(inv.get('resources',[])):raise ValueError('inventory schema/count')
  for p,h in inv['source_inputs'].items():pin(p,h)
  specs=importlib.util.spec_from_file_location('shader_fixture',root/'nvn-native/tools/shader_fixture.py');mod=importlib.util.module_from_spec(specs);specs.loader.exec_module(mod);pin(mod.__file__)
  ids={}
  for e in inv['entries']:
   p=Path(e['fixture']);pin(p,e['fixture_sha256']);d=json.loads(p.read_text());mod.validate(d,p.parent)
   for b in d['blobs'].values():pin(p.parent/b['path'],b['sha256'])
   if e['source_id']in ids and ids[e['source_id']]!=e['stage']:raise ValueError('source identity stage conflict')
   ids[e['source_id']]=e['stage']
  outcomes=[]
  for p in x.compiled:
   pin(p);c=json.loads(p.read_text())
   if c.get('kind')!='offline-graphics-stage-compilation-v1'or c.get('gpu_execution')is not False or c.get('pair_selection')is None:raise ValueError('normal explicit-pair compilation required')
   for q,h in{**c['inputs'],**c['artifacts']}.items():pin(q,h)
   if len(c['stages'])>len(c['inventory']['entries']):raise ValueError('stage count')
   outcomes.append({'check':str(p.resolve()),'stages':[{'stage':s['stage'],'frontend_passed':s['frontend_passed'],'failure':s['failure'],'source_ids':s['source_ids'],'request_sha256':k}for k,s in c['stages'].items()],'pairs':[{'vs':v['vs'],'fs':v['fs'],'passed':v['passed'],'failure':v['failure']}for v in c['pairs']]})
  tests={'skipped':'metadata-only inventory has no selected source blobs','negative_cases':0}
  if inv['entries']:
   test=Path(__file__).resolve().parents[1]/'tests/test_casc_graphics_inventory.py';pin(test)
   argv=[sys.executable,str(test),'--root',str(root),'--inventory',str(x.inventory.resolve()),'--out',str(out/'tests')];r=subprocess.run(argv,text=True,capture_output=True);runs.append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
   if r.returncode:raise ValueError('HOST boundaries fixture failed '+r.stderr)
   tests=json.loads(r.stdout)
   if tests.get('negative_cases')!=16:raise ValueError('HOST boundaries count')
  for p,h in inputs.items():
   if sha(p)!=h:raise ValueError('source changed during check '+p)
  proof.update(passed=True,resource_count=inv['resource_count'],container_count=sum(len(v['validated_containers'])for v in inv['resources']),unrecognized_control_candidates=sum(v['unrecognized_control_candidates']for v in inv['resources']),unique_extracted_stages=len(ids),unsupported_stage_counts=inv['unsupported_stage_counts'],tests=tests,compile_outcomes=outcomes)
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,runs=runs);proof['artifacts']={str(p):sha(p)for p in out.rglob('*')if p.is_file()};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'check':str(out/'check.json')}));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
