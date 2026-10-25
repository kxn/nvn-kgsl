#!/usr/bin/env python3
"""Normal actual original25 NIR/IR3 diagnostic, preserving historical failure/code evidence.
The fixed diagnostic FS and explicitly specialized source artefacts are not an
observed game pair. Historical producer input bytes are pinned, never relabelled.
"""
import argparse,json,os,subprocess,hashlib
from pathlib import Path
from package_graphics_pipelines import compiler_record,checked,sha

def check(root,compiler,reference,historical_repo,historical_ref,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 old=json.loads(reference.read_text())
 if old.get('kind')!='graphics-storage-original25-normal-diagnostic-v1'or old.get('passed')is not True or len(old['results'])!=25:raise ValueError('wrong original reference')
 inputs={str(Path(__file__).resolve()):sha(__file__),str(reference):sha(reference)};cc,_,record=compiler_record(compiler,n,inputs);out.mkdir(parents=True);history=[]
 commit=subprocess.check_output(['git','-C',str(historical_repo),'rev-parse',historical_ref+'^{commit}'],text=True).strip()
 for group in ('inputs','artifacts'):
  for file,h in old[group].items():
   p=Path(file)
   if p.is_file()and sha(p)==h:inputs[str(p)]=h;continue
   if group!='inputs'or not p.is_relative_to(historical_repo/'nvn-native'):raise ValueError('historical input/product changed '+file)
   rel=str(p.relative_to(historical_repo));raw=subprocess.check_output(['git','-C',str(historical_repo),'show',commit+':'+rel])
   if hashlib.sha256(raw).hexdigest()!=h:raise ValueError('historical source does not match declared original bytes '+file)
   dest=out/'historical-producer-source'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(raw);inputs[str(dest)]=h
   history.append(dict(original_path=file,git_commit=commit,git_path=rel,sha256=h,snapshot=str(dest),scope='Historical compiler source for existing byte reference only; not current deployment/compiler qualification'))
 commands=[];results=[]
 def run(a):
  a=list(map(str,a));r=subprocess.run(a,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'),text=True,capture_output=True);commands.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');return r
 for row in old['results']:
  i=row['index'];args=row['command'];sp=[j for j,x in enumerate(args)if x.endswith('.spv')]
  if len(sp)!=2:raise ValueError('bad original SPIR-V reference')
  prefix=args[sp[1]+1]
  for j in sp:
   if args[j]not in inputs:raise ValueError('original SPIR-V identity missing from immutable reference inputs')
  strict=list(args);strict[0]=str(cc);strict[sp[1]+1]=str(out/f'strict{i}');r=run(strict)
  if r.returncode!=row['strict_returncode']:raise ValueError('legacy explicit producer gate changed')
  extended=strict.copy();extended[sp[1]+1]=str(out/f'runtime{i}');extended.insert(1,'--runtime-constants');new=run(extended)
  if new.returncode:raise ValueError('original diagnostic extended recipe rejected '+str(i))
  for suffix in ('.vs.bin','.fs.bin'):
   p=Path(prefix+suffix)
   if not p.is_file()or sha(p)!=sha(str(out/f'runtime{i}')+suffix):raise ValueError('original native code changed')
  if not row['strict_returncode']:
   for suffix in ('.pipeline.h','.pipeline.json','.pipeline.types.json'):
    p=Path(prefix+suffix)
    if p.is_file()and (sha(p)!=sha(str(out/f'strict{i}')+suffix)or sha(p)!=sha(str(out/f'runtime{i}')+suffix)):raise ValueError('old exact recipe bytes changed')
  actual=json.loads(Path(str(out/f'runtime{i}')+'.pipeline.runtime.json').read_text());vs=actual['stages'][0];fs=actual['stages'][1]
  if vs['driver_dwords']!=row['actual_num_driver_params']or vs['ubo_entries']!=row['actual_num_ubos']or fs['driver_dwords']or fs['ubo_entries']or any(x['driver_unknown']or x['ubo_unknown']for x in actual['stages']):raise ValueError('actual typed facts changed domains')
  if vs['driver_dwords']and(vs['driver_mask']!=4 or vs['driver_offset_vec4']!=4 or vs['driver_upload_vec4']!=4):raise ValueError('original VS actual BaseInstance domain differs')
  if vs['ubo_entries']and vs['ubo_mask']!=2:raise ValueError('original VS residual binding differs')
  results.append(dict(index=i,vs_resource=row['vs_resource'],vs_source_ids=row['vs_source_ids'],old_classification=row['classification'],old_returncode=r.returncode,new_returncode=new.returncode,runtime=actual,source_sbufs=row['source_sbufs'],scope=row['scope']))
 checked(inputs,{})
 proof=dict(schema=1,kind='runtime-constants-original25-normal-diagnostic-v1',passed=True,gpu_execution=False,android_execution=False,results=results,counts=dict(original_vs=25,previous_native_successes=10,previous_driver_gates=12,previous_residual_ubo_gates=3,new_native_recipes=25,native_stage_bins_unchanged=50),historical_producer_sources=history,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Normal standalone compiler using unchanged actual original VS and fixed original diagnostic FS/full source-control-C1 provenance; explicit preexisting authored specialization, not observed game pairing or GPU/publicNVN qualification')
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','reference','historical-repo','out'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--historical-source-ref',required=True);a=p.parse_args();print(check(a.root.resolve(),a.compiler.resolve(),a.reference.resolve(),a.historical_repo.resolve(),a.historical_source_ref,a.out.resolve()))
