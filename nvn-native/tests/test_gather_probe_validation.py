#!/usr/bin/env python3
"""Synthetic HOST contract negatives; no hardware result is created or blessed."""
import copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from gather_probe_validation import NAMES,expected,validate

def check(record_path,result_path,raw_dir):
 c=json.loads(record_path.read_text());d=json.loads(result_path.read_text());nonce='b'*32;uid=11234;d.update(uid=uid,run_nonce=nonce,run_directory='pipeline-'+nonce)
 fake={n:(raw_dir/n).read_bytes()for n in NAMES}
 try:validate(d,fake,c,nonce,uid)
 except RuntimeError:pass
 else:raise ValueError('nonexecuting HOST seed accepted as actual GPU pixels')
 captured={n:expected(d['cases'][i//2],i%2)for i,n in enumerate(NAMES)};positive=validate(d,captured,c,nonce,uid);negative=[]
 def reject(name,mutate):
  bad=copy.deepcopy(d);blobs=dict(captured);mutate(bad,blobs)
  try:validate(bad,blobs,c,nonce,uid)
  except (RuntimeError,KeyError,TypeError):negative.append(name)
  else:raise ValueError('invalid Gather proof accepted: '+name)
 for k,v in [('passed',False),('passed',1),('resources_retained',True),('submissions',95),('finishes',True),('files',95),('programs',7),('uid',0),('nvn_api',True),('same_job_replay',False),('readonly_byte_checks',0),('guard_byte_checks',False),('sparse_hole_checks',0),('run_nonce','c'*32)]:reject(k,lambda a,b,k=k,v=v:a.__setitem__(k,v))
 for k,v in [('binding',10),('component',3),('wrap',0),('origin',True),('pitch',128),('resource_count',5),('command_bytes',0),('readonly_bytes',0)]:reject('case-'+k,lambda a,b,k=k,v=v:a['cases'][0].__setitem__(k,v))
 reject('missing-raw',lambda a,b:b.pop(NAMES[0]));reject('extra-raw',lambda a,b:b.__setitem__('gather-extra.raw',b''));reject('raw-length',lambda a,b:b.__setitem__(NAMES[0],b[NAMES[0]][:-1]))
 for label,offset in [('prefix',0),('active',4096),('padding',4096+128),('tail',len(captured[NAMES[0]])-1)]:
  def corrupt(a,b,off=offset):
   value=bytearray(b[NAMES[0]]);value[off]^=127;b[NAMES[0]]=bytes(value)
  reject('corrupt-'+label,corrupt)
 # Normative neighbour order must not accidentally pass channel rotation.
 def lanes(a,b):
  value=bytearray(b[NAMES[0]]);value[4096:4100]=value[4097:4100]+value[4096:4097];b[NAMES[0]]=bytes(value)
 reject('rotated-lanes',lanes)
 print(json.dumps(dict(passed=True,negative_cases=len(negative),synthetic_positive=positive,nonexecuting_HOST_pixels_rejected=True,gpu_execution=False)))
if __name__=='__main__':check(*map(Path,sys.argv[1:]))
