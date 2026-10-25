#!/usr/bin/env python3
"""Offline synthetic oracle negatives only. Never uploaded or a GPU result."""
import argparse,copy,json,sys,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from graphics_storage_probe_validation import validate,synthetic,NAMES,catalogue,source

def test(check):
 nonce='a'*32;uid=12000;data,raw=synthetic(check,nonce,uid);positive=validate(data,raw,check,nonce,uid);negatives=[]
 def bad(name,d=None,r=None,c=None):
  try:validate(d or data,r or raw,c or check,nonce,uid)
  except RuntimeError:negatives.append(name);return
  raise ValueError('false acceptance '+name)
 for key,value in [('submissions',191),('finishes',191),('files',575),('jobs',95),('replays',95),('programs',3),('resources_retained',True),('cpu_shader_execution',True),('game_modules_executed',True),('storage_bo_bytes',12284),('job_bytes',45056),('readonly_byte_checks',data['readonly_byte_checks']-1),('inflight_owner_checks',data['inflight_owner_checks']-1),('unused_owner_checks',0),('run_nonce','b'*32),('target_bo_bytes',data['target_bo_bytes']-4096)]:
  d=dict(data);d[key]=value;bad(key,d=d)
 r=dict(raw);r.pop(NAMES[-1]);bad('missing_last_fullraw',r=r)
 r=dict(raw);r[NAMES[1]]=r[NAMES[1]][:-1];bad('short_storage',r=r)
 # A successful submission with no shader side effects cannot pass the oracle.
 case=next(c for c in check['cases']if c['program']==1);i=case['id']*6+1;r=dict(raw);b=bytearray(raw[NAMES[i]]);start=4096+case['offset'][0]+8*4;b[start:start+4]=struct.pack('<I',source(0,8,0));r[NAMES[i]]=bytes(b);bad('no_VS_UAV_write',r=r)
 i=case['id']*6+2;r=dict(raw);b=bytearray(raw[NAMES[i]]);start=4096+case['offset'][1]+16*4;b[start:start+4]=struct.pack('<I',source(1,16,0));r[NAMES[i]]=bytes(b);bad('no_FS_UAV_write',r=r)
 r=dict(raw);r[NAMES[2]]=raw[NAMES[1]];bad('VS_FS_descriptor_alias',r=r)
 r=dict(raw);r[NAMES[3]]=raw[NAMES[0]];bad('stale_replay_pixels',r=r)
 r=dict(raw);r[NAMES[4]]=raw[NAMES[1]];bad('stale_replay_storage',r=r)
 for role in range(3):
  r=dict(raw);b=bytearray(raw[NAMES[role]]);b[0]^=1;r[NAMES[role]]=bytes(b);bad('prefix_guard_role'+str(role),r=r)
 case=next(c for c in check['cases']if c['program']==2);i=case['id']*6;r=dict(raw);b=bytearray(raw[NAMES[i]]);b[4096]=0;r[NAMES[i]]=bytes(b);bad('wrong_get_ssbo_size_pixel',r=r)
 r=dict(raw);r[NAMES[0]]=bytes([0xa5])*len(raw[NAMES[0]]);bad('no_actual_draw',r=r)
 c=copy.deepcopy(check);c['cases']=c['cases'][:-1];bad('partial_matrix',c=c)
 assert len(negatives)==29,negatives
 return dict(passed=True,oracle='independent closed-form authored GLSL and full storage uint32 writes',positive=positive,negative_cases=len(negatives),negatives=negatives,gpu_execution=False,synthetic_only=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',type=Path,required=True);a=p.parse_args();print(json.dumps(test(json.loads(a.check.read_text()))))
