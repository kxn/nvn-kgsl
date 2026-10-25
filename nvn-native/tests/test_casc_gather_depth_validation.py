#!/usr/bin/env python3
"""Synthetic completion and independent scalar negatives. No GPU/device work."""
import copy,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import casc_gather_depth_validation as v
from casc_gather_depth_oracle import cases

def fixture(check):
 rows=v.checked_layout(check);uid=10234;nonce='1234567890abcdef'*2
 data=dict(schema=1,operation='public_casc_gather_depth',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=72,finishes=72,completed_cases=72,files=72,recorded_handles=60,released_handles=60,replays=36,texture_registrations=144,registered_cpu_finalizes=180,busy_source_checks=72,copy_wrappers=12,call_wrappers=12,guard_byte_checks=sum(2*(r['target_pool_bytes']-r['target_payload'])for r in rows),readonly_byte_checks=72*147456,bad_case=0xffffffff,public_nvn=True,same_handle_replay=True,source_finalize_before_enqueue=True,late_descriptor_updates=True,cpu_image_conversion=False,game_modules_executed=False,zero_color=True,original_C1_bytes=256,public_depth_mode=0,texture_selector=10,sampler_selector=3,offline_pixel_oracle_required=True,passed=True,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,cases=[])
 for c,r in zip(cases(),rows):
  row={k:c[k]for k in('case','format','width','height','origin','wrap_index','selected_mip','compare','clear_depth_bits')};row.update(target_pool_bytes=r['target_pool_bytes'],target_payload=r['target_payload'],source_pool_bytes=12288);data['cases'].append(row)
 return data,{name:v.expected(i)[0].tobytes()for i,name in enumerate(v.NAMES)},nonce,uid

def main(check):
 c=json.loads(Path(check).read_text());data,raw,nonce,uid=fixture(c);result=v.validate(data,raw,c,nonce,uid);neg=0
 def reject(d=data,r=raw,record=c,n=nonce,u=uid):
  nonlocal neg
  try:v.validate(d,r,record,n,u)
  except (ValueError,RuntimeError,TypeError,KeyError):neg+=1;return
  raise AssertionError('invalid evidence accepted')
 for key,value in [('uid',True),('passed',1),('submissions',71),('finishes',71),('resources_retained',True),('cleanup_errno',16),('zero_color',False),('original_C1_bytes',64),('texture_selector',1),('sampler_selector',0),('public_depth_mode',1),('readonly_byte_checks',0),('recorded_handles',59),('released_handles',59),('busy_source_checks',0),('game_modules_executed',True),('clear_depth_endpoints_only',True)]:
  if key=='clear_depth_endpoints_only':continue
  d=copy.deepcopy(data);d[key]=value;reject(d=d)
 reject(n='0'*32);reject(u=0)
 for key,value in [('origin',True),('compare',8),('clear_depth_bits',0x3f000000),('selected_mip',0),('source_pool_bytes',0)]:
  d=copy.deepcopy(data);d['cases'][18 if key!='selected_mip'else 6][key]=value
  if d==data:d['cases'][0][key]=value+1
  reject(d=d)
 r=dict(raw);r.pop(v.NAMES[-1]);reject(r=r)
 r=dict(raw);r['extra.raw']=b'';reject(r=r)
 r=dict(raw);r[v.NAMES[0]]=r[v.NAMES[0]][:-1];reject(r=r)
 for number,at in [(0,0),(0,4095),(0,len(raw[v.NAMES[0]])-1),(12,4096),(0,int(v.locations(0)[4,6,0]))]:
  r=dict(raw);b=bytearray(r[v.NAMES[number]]);b[at]^=0x80;r[v.NAMES[number]]=bytes(b);reject(r=r)
 # Wrong signed C1 algebra: odd parity must MAX, never min*k^2.
 number=48;name=v.NAMES[number];r=dict(raw);b=bytearray(r[name]);at=v.locations(number//2)[4,7];b[at[0]:at[0]+1]=b[at[0]:at[0]+1] # no contiguous-address assumptions
 for lane,p in enumerate(at):b[int(p)]=struct.pack('<f',.125)[lane]
 r[name]=bytes(b);reject(r=r)
 # D24 active X8 is explicitly undefined; low24 corruption still rejects.
 d24=24;name=v.NAMES[d24];r=dict(raw);b=bytearray(r[name]);p=int(v.locations(d24//2)[4,6,3]);b[p]^=0xff;r[name]=bytes(b);v.validate(data,r,c,nonce,uid)
 b[p-1]^=0x80;r[name]=bytes(b);reject(r=r)
 result.update(synthetic_only=True,gpu_execution=False,negative_cases=neg,positive_cases=2,original_C1_preserved=True)
 print(json.dumps(result));return result
if __name__=='__main__':main(sys.argv[1])
