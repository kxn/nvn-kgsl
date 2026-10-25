#!/usr/bin/env python3
"""Offline verifier input/transport negatives; no ADB and no GPU execution."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys


def main():
 p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 root=a.root.resolve();out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh worktree/.local output required')
 out.mkdir(parents=True);native=Path(__file__).resolve().parents[1];tool=native/'tools/verify_sampler_mip_cts.py'
 spec=importlib.util.spec_from_file_location('cts_provider',tool);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 proof=m.load(a.check);assert proof['passed']is True and proof['original_exact_oracle_passed']is False
 original=m.load(proof['original_evidence']);manifest=m.load(native/'tools/cts_texture_sources.json');checks=[]
 def reject(name,fn):
  try:fn()
  except (ValueError,RuntimeError,KeyError,TypeError,AssertionError,FileNotFoundError)as error:checks.append(dict(name=name,rejected=True,error=str(error)[:180]));return
  raise AssertionError('accepted negative: '+name)
 for field,value in [('operation','wrong'),('submissions',575),('finishes',575),('uid',True),('resources_retained',True),('guard_byte_checks',0)]:
  d=copy.deepcopy(original);d['result'][field]=value;path=out/(field+'.json');m.write(path,d);reject(field,lambda path=path:m.evidence_inputs(root,path,manifest))
 for name,mutate in [
  ('stop-unproved',lambda d:d.update(stopped=False)),
  ('pidof-transport',lambda d:d['final_pidof'].update(returncode=127,stderr='transport failed')),
  ('missing-raw',lambda d:d['copy_images'].pop('sampler-mip-386.raw')),
  ('extra-raw',lambda d:d['copy_images'].update(extra=d['copy_images']['sampler-mip-386.raw'])),
  ('wrong-raw-sha',lambda d:d['copy_images']['sampler-mip-386.raw'].update(sha256='0'*64)),
  ('bool-raw-size',lambda d:d['copy_images']['sampler-mip-386.raw'].update(bytes=True)),
  ('fixture-identity',lambda d:d['build']['source'].update({'tests/sampler_mip_cases.h':'0'*64})),
  ('profile-version',lambda d:d['build']['native_sampler_mip_check'].update(kind='other'))]:
  d=copy.deepcopy(original);mutate(d);path=out/(name+'.json');m.write(path,d);reject(name,lambda path=path:m.evidence_inputs(root,path,manifest))
 link=out/'raw-link';link.symlink_to(original['copy_images']['sampler-mip-386.raw']['path']);d=copy.deepcopy(original);d['copy_images']['sampler-mip-386.raw']['path']=str(link);path=out/'symlink.json';m.write(path,d);reject('raw-symlink',lambda:m.evidence_inputs(root,path,manifest))
 bad=out/'bad-source';bad.mkdir();relative=next(iter(manifest['files']));f=bad/relative;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b'corrupt');reject('pinned-source-corrupt',lambda:m.sources(manifest,out,bad))
 def cli(name,args):
  r=subprocess.run([sys.executable,str(tool),*args],capture_output=True,text=True);assert r.returncode!=0;(out/(name+'.stderr')).write_text(r.stderr);checks.append(dict(name=name,rejected=True,returncode=r.returncode))
 cli('existing-out',['--root',str(root),'--evidence',proof['original_evidence'],'--out',str(out)])
 cli('outside-local',['--root',str(root),'--evidence',proof['original_evidence'],'--out','/tmp/cts-outside-local-must-not-create'])
 cli('no-profile-override',['--root',str(root),'--evidence',proof['original_evidence'],'--out',str(out/'unused'),'--lod-bits','4'])
 build=a.check.resolve().parent;exe=build/'cts_texture_lookup';raw=(build/'actual-cases.bin').read_bytes();h=struct.unpack_from('<20I',raw,8)
 size=80+sum(max(1,h[5]>>i)*max(1,h[6]>>i)*4 for i in range(h[8]))+h[16]*h[17]*4
 one=struct.pack('<2I',0x32535443,1)+raw[8:8+size]
 for name,blob in [('bad-magic',b'badmagic'),('zero-count',struct.pack('<2I',0x32535443,0)),('truncated-reference',one[:100]),('trailing-data',one+b'x'),('invalid-width',one[:8+16*4]+struct.pack('<I',0)+one[8+17*4:])]:
  path=out/(name+'.bin');path.write_bytes(blob);r=subprocess.run([str(exe),str(path)],capture_output=True,text=True);assert r.returncode!=0;checks.append(dict(name=name,rejected=True,returncode=r.returncode))
 # Actual linked CTS controls and complete saved evidence are required, not mocks.
 assert proof['reference_result']['independent_negative_controls']==7 and proof['reference_result']['files']==576
 result=dict(schema=1,passed=True,checks=checks,negative_count=len(checks),actual_CTS_negative_controls=7,complete_actual_files=576,GPU_execution=False,ADB_invocations=0,original_exact_failure_preserved=proof['original_exact_oracle_error'],inputs={str(tool):m.sha(tool),str(Path(__file__).resolve()):m.sha(__file__),str(a.check.resolve()):m.sha(a.check)})
 m.write(out/'check.json',result);print(json.dumps(dict(passed=True,negatives=len(checks),check=str(out/'check.json'))))
if __name__=='__main__':main()
