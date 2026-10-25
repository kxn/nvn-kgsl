#!/usr/bin/env python3
"""Re-run existing explicit native pairs; byte regression, no new source pairing."""
import argparse,json,os,subprocess,hashlib
from pathlib import Path
from package_graphics_pipelines import compiler_record,checked,sha

def check(root,compiler,references,historical_repo,origins,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__),str(n/'tools/package_graphics_pipelines.py'):sha(n/'tools/package_graphics_pipelines.py'),str(origins):sha(origins)};cc,_,record=compiler_record(compiler,n,inputs);history=json.loads(origins.read_text());by_path={r['path']:r for r in history};out.mkdir(parents=True);commands=[];pairs=0;compared={};checks=[]
 for reference in references:
  proof=json.loads(reference.read_text());inputs[str(reference)]=sha(reference)
  if proof.get('passed')is not True or proof.get('gpu_execution')is not False:raise ValueError('wrong historical reference')
  for group in ('inputs','artifacts','outputs'):
   for file,h in proof.get(group,{}).items():
    p=Path(file)
    if p.is_file()and sha(p)==h:inputs[file]=h;continue
    if group!='inputs'or file not in by_path:raise ValueError('changed historical product/source '+file)
    row=by_path[file]
    if row['expected_sha256']!=h or row['git_path']!=str(p.relative_to(historical_repo)):raise ValueError('source origin mismatches declared reference')
    raw=subprocess.check_output(['git','-C',str(historical_repo),'show',row['git_commit']+':'+row['git_path']])
    if hashlib.sha256(raw).hexdigest()!=h:raise ValueError('historical git blob changed')
    dest=out/'historical-producer-source'/row['git_path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);inputs[str(dest)]=h
  rows=proof.get('commands')or json.loads((reference.parent/'commands.json').read_text())
  for row in rows:
   old=row['argv'];name=Path(old[0]).name;sp=[j for j,a in enumerate(old)if str(a).endswith('.spv')]
   if name not in ('nvn-pipeline-ir3','nvn-graphics-ir3')or len(sp)!=2:continue
   old_prefix=old[sp[1]+1];new_prefix=out/f'pair{pairs}';a=old.copy();a[0]=str(cc.with_name(name));a[sp[1]+1]=str(new_prefix);r=subprocess.run(a,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
   if r.returncode!=row['returncode']:raise ValueError('legacy returncode changed '+old_prefix)
   for suffix in ('.vs.bin','.fs.bin','.link.json','.vs.io.json','.fs.io.json','.pipeline.h','.pipeline.json','.pipeline.types.json','.pipeline.storage.json'):
    p=Path(old_prefix+suffix)
    if not p.exists():continue
    q=Path(str(new_prefix)+suffix)
    if not q.is_file()or sha(p)!=sha(q):raise ValueError('legacy exact artifact changed '+str(p))
    inputs[str(p)]=sha(p);compared[suffix]=compared.get(suffix,0)+1
   checks.append(dict(reference=str(reference),old_prefix=old_prefix,new_prefix=str(new_prefix),returncode=r.returncode));pairs+=1
 checked(inputs,{})
 if pairs<293 or compared.get('.vs.bin',0)<293 or compared.get('.pipeline.h',0)<104:raise ValueError('incomplete original whole-family byte regression')
 proof=dict(schema=1,kind='runtime-constants-legacy-native-byte-regression-v1',passed=True,gpu_execution=False,android_execution=False,native_pair_invocations=pairs,artifact_comparisons=compared,default_profiles_unchanged=True,original_pairings_unchanged=True,checks=checks,historical_producer_origins=history,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Current normal compiler reruns only existing explicit native pair commands; historical sources/outputs remain immutable byte references, not relabelled as current deployment qualification')
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','historical-repo','origins','out'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--reference',type=Path,action='append',required=True);a=p.parse_args();print(check(a.root.resolve(),a.compiler.resolve(),[p.resolve()for p in a.reference],a.historical_repo.resolve(),a.origins.resolve(),a.out.resolve()))
