#!/usr/bin/env python3
"""Synthetic HOST oracle/contract tests, never actual GPU evidence."""
import copy
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from sampler_mip_validation import NAMES,expected,validate,level_sample,nearest_boundary_margin


def check(record_path,result_path,raw_dir):
 import numpy as np
 c=json.loads(record_path.read_text());d=json.loads(result_path.read_text());nonce='b'*32;uid=11234;d.update(uid=uid,run_nonce=nonce,run_directory='pipeline-'+nonce)
 fake={n:(raw_dir/n).read_bytes()for n in NAMES}
 try:validate(d,fake,c,nonce,uid)
 except RuntimeError:pass
 else:raise ValueError('nonexecuting HOST pixels accepted')
 captured={};filtered_index=None;nearest_index=None
 for i,case in enumerate(c['cases']):
  for repeat in range(2):
   pixels,filtered=expected(case,c['views'][case['view']],repeat)
   raw=np.full(d['bo_bytes'],0xa5,dtype=np.uint8);rows=raw[4096:4096+c['target']['pitch']*64].reshape(64,c['target']['pitch']);rows[:,:256]=pixels.reshape(64,256)
   captured[NAMES[i*2+repeat]]=raw.tobytes()
   if filtered and filtered_index is None:filtered_index=i*2+repeat
   if not filtered and nearest_index is None:nearest_index=i*2+repeat
 nearest_margin=nearest_boundary_margin(c)
 if nearest_margin<1/1024:raise ValueError('ambiguous exact nearest fixture texel boundary')
 positive=validate(d,captured,c,nonce,uid);negative=[]
 def reject(label,change):
  bad=copy.deepcopy(d);raw=dict(captured);change(bad,raw)
  try:validate(bad,raw,c,nonce,uid)
  except(RuntimeError,KeyError,TypeError):negative.append(label)
  else:raise ValueError('accepted invalid '+label)
 for k,v in [('passed',False),('passed',1),('resources_retained',True),('submissions',575),('finishes',True),('files',575),('programs',2),('uid',0),('nvn_api',True),('same_job_replay',False),('readonly_byte_checks',0),('readonly_bytes_per_submit',0),('guard_byte_checks',False),('run_nonce','c'*32),('cpu_fixture_source_initialization',False),('resource_count',7),('command_bytes',3),('bo_bytes',0)]:reject(k,lambda a,b,k=k,v=v:a.__setitem__(k,v))
 for index,value in[(0,True),(1,6),(2,2),(3,3),(4,2),(5,99),(6,0),(7,0),(8,0x7fc00000)]:reject('state-'+str(index),lambda a,b,index=index,value=value:a['cases'][0].__setitem__(index,value))
 reject('missing-raw',lambda a,b:b.pop(NAMES[0]));reject('extra-raw',lambda a,b:b.__setitem__('unexpected.raw',b''));reject('length',lambda a,b:b.__setitem__(NAMES[0],b[NAMES[0]][:-1]))
 for label,offset in [('prefix',0),('active',4096),('tail',len(captured[NAMES[0]])-1)]:
  def corrupt(a,b,offset=offset):
   raw=bytearray(b[NAMES[0]]);raw[offset]^=127;b[NAMES[0]]=bytes(raw)
  reject('corrupt-'+label,corrupt)
 def alter(raw,name,delta):
  value=bytearray(raw[name]);value[4096]+=delta;raw[name]=bytes(value)
 reject('nearest-plus1',lambda a,b:alter(b,NAMES[nearest_index],1))
 reject('filtered-plus3',lambda a,b:alter(b,NAMES[filtered_index],3))
 within=dict(captured);alter(within,NAMES[filtered_index],2);validate(d,within,c,nonce,uid)
 # Independently literal center/seam/bilinear points, not generated expected().
 view=dict(width=4,height=4)
 for wrap,wanted in[(0,[71,108,145,182]),(1,[32,69,106,143]),(2,[32,69,106,143])]:
  p=level_sample(np.array([[-.125]]),np.array([[.125]]),0,False,wrap,0,view)
  if p[0,0].tolist()!=wanted:raise ValueError('independent nearest address oracle')
 p=level_sample(np.array([[.25]]),np.array([[.25]]),0,True,2,0,view)
 if p[0,0].tolist()!=[42,79,116,153]:raise ValueError('independent four-texel interpolation oracle')
 print(json.dumps(dict(passed=True,negative_cases=len(negative),synthetic_positive=positive,independent_math_checks=5,nearest_boundary_margin=nearest_margin,nonexecuting_HOST_pixels_rejected=True,gpu_execution=False)))
if __name__=='__main__':check(*map(Path,sys.argv[1:]))
