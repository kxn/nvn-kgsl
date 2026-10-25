#!/usr/bin/env python3
"""Synthetic-only normal matrix/oracle corruption negatives; never GPU evidence."""
import copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import public_depth_sampled_validation as v
from public_depth_sampled_oracle import cases

def fixture(check):
 rows=v.canonical();uid=10234;nonce='1234567890abcdef'*2
 data=dict(schema=1,operation='public_depth_sampled',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=144,finishes=144,completed_cases=144,files=144,recorded_handles=120,released_handles=120,replays=72,texture_registrations=288,registered_cpu_finalizes=360,busy_source_checks=144,copy_wrappers=24,call_wrappers=24,guard_byte_checks=sum(2*(r['target']['pool_bytes']-r['target']['payload'])for r in rows),readonly_byte_checks=sum(2*(122880+2*r['source']['pool_bytes'])for r in rows),bad_case=0xffffffff,public_nvn=True,same_handle_replay=True,source_finalize_before_enqueue=True,late_descriptor_updates=True,cpu_image_conversion=False,game_modules_executed=False,zero_color_draws=72,original_C1_bytes=256,public_depth_mode=0,texture_selector=10,sample_sampler_selector=0,gather_sampler_selector=3,offline_pixel_oracle_required=True,passed=True,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,cases=[])
 for c,r in zip(cases(),rows):data['cases'].append({**c,'clear_depth_bits':0x3f800000,'target_pool_bytes':r['target']['pool_bytes'],'target_payload':r['target']['payload'],'source_pool_bytes':r['source']['pool_bytes']})
 return data,{name:v.expected(i).tobytes()for i,name in enumerate(v.NAMES)},nonce,uid

def main(check):
 c=json.loads(Path(check).read_text());data,raw,nonce,uid=fixture(c);result=v.validate(data,raw,c,nonce,uid);neg=0
 def reject(d=data,r=raw,record=c,n=nonce,u=uid):
  nonlocal neg
  try:v.validate(d,r,record,n,u)
  except(ValueError,RuntimeError,TypeError,KeyError):neg+=1;return
  raise AssertionError('invalid evidence accepted')
 for key,value in [('uid',True),('passed',1),('submissions',143),('finishes',143),('resources_retained',True),('cleanup_errno',16),('zero_color_draws',144),('original_C1_bytes',64),('texture_selector',1),('sample_sampler_selector',3),('gather_sampler_selector',0),('public_depth_mode',1),('readonly_byte_checks',0),('recorded_handles',119),('released_handles',119),('busy_source_checks',0),('game_modules_executed',True),('cpu_image_conversion',True),('texture_registrations',0),('registered_cpu_finalizes',0)]:
  d=copy.deepcopy(data);d[key]=value;reject(d=d)
 reject(n='0'*32);reject(u=0)
 for key,value in [('origin',True),('role',1),('format',10),('compare',8),('clear_depth_bits',0x3f000000),('selected_mip',1),('source_pool_bytes',0)]:
  d=copy.deepcopy(data);d['cases'][0][key]=value;reject(d=d)
 r=dict(raw);r.pop(v.NAMES[-1]);reject(r=r)
 r=dict(raw);r['extra.raw']=b'';reject(r=r)
 r=dict(raw);r[v.NAMES[0]]=r[v.NAMES[0]][:-1];reject(r=r)
 for number,at in [(0,0),(0,4095),(0,len(raw[v.NAMES[0]])-1),(0,int(v.locations(0)[0,0,0])),(0,int(v.locations(0)[4,6,0])),(72,int(v.locations(36)[4,6,0]))]:
  r=dict(raw);b=bytearray(r[v.NAMES[number]]);b[at]^=0x80;r[v.NAMES[number]]=bytes(b);reject(r=r)
 for role in range(2):
  for fmt in(50,51,52):
   bank=[v.red(c,3,5,0)for c in cases()if c['role']==role and c['format']==fmt]
   assert bank and all(0<v<1 for v in bank)
 result.update(synthetic_only=True,gpu_execution=False,negative_cases=neg,positive_cases=144,original_C1_preserved=True)
 print(json.dumps(result));return result
if __name__=='__main__':main(sys.argv[1])
