#!/usr/bin/env python3
"""Collector-only synthetic oracle/negative controls; never GPU execution."""
import argparse,copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from depth_shadow_probe_validation import *
def synthetic(check):
 import numpy as np
 captured={};guards=busy=readonly=0;cases=[]
 for n,c in enumerate(check['cases']):
  case={k:c[k]for k in('case','format','profile','selected','role','origin','level','layer','compare','width','height','canonical_bytes','color_pitch','color_bytes')};case.update(refs=17 if c['role']==2 else 18,readonly_bytes=check['readonly_bytes_per_chain']);cases.append(case);busy+=12*(case['refs']+3);w,h=c['width'],c['height']
  for repeat in range(2):
   number=n*2+repeat;raw,defined,written=expected(n,repeat);captured['depth-shadow-gob-%03u.raw'%number]=raw;guards+=len(raw)-c['payload'];readonly+=case['readonly_bytes']
   if c['role']==2:continue
   raw=np.full(c['color_bytes'],0xa5,dtype=np.uint8);active=np.empty((h,w,4),dtype=np.uint8);active[:]=(17,29,43,59);y,x=np.mgrid[:h,:w];u=(x+.5)/w;v=(y+.5)/h
   if c['origin']:v=1-v
   rgba=np.floor(np.clip(np.stack((1-(u+v)*.5,u*.5,v*.5,np.ones_like(u)),axis=-1),0,1)*255+.5).astype(np.uint8)
   if c['role']==0:
    for xx,yy in written:active[yy,xx]=rgba[yy,xx]
   for row in range(h):raw[4096+row*c['color_pitch']:4096+row*c['color_pitch']+w*4]=active[row].ravel()
   captured['depth-shadow-color-%03u.raw'%number]=raw.tobytes();guards+=len(raw)-w*h*4
 data=dict(schema=1,operation='native_canonical_depth',uid=10001,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=324,programs=54,submissions=648,finishes=108,files=180,completed_cases=108,replays=54,true_zero_color_cases=18,pixel_checks=0,inflight_owner_checks=busy,guard_byte_checks=guards,readonly_byte_checks=readonly,passed=True,resources_retained=False,pixel_oracle_on_host=True,canonical_fixture_input_only=True,private_depth_cpu_access=False,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True,stencil=False,run_nonce='a'*32,run_directory='pipeline-'+'a'*32,cases=cases)
 return data,captured
def run(check,host_result=None,host_raw=None):
 data,captured=synthetic(check);answer=validate(data,captured,check,'a'*32,10001);negative=[]
 def reject(name,d=data,b=captured,c=check):
  try:validate(d,b,c,'a'*32,10001)
  except(ValueError,RuntimeError,KeyError,TypeError):negative.append(name);return
  raise ValueError('bad depth collection accepted '+name)
 for key,value in [('submissions',647),('finishes',107),('files',179),('jobs',323),('programs',53),('completed_cases',107),('replays',53),('true_zero_color_cases',17),('inflight_owner_checks',data['inflight_owner_checks']-1),('readonly_byte_checks',data['readonly_byte_checks']-1),('guard_byte_checks',data['guard_byte_checks']-1),('private_depth_cpu_access',True),('canonical_fixture_input_only',False),('cpu_image_conversion',True),('nvn_api',True),('game_modules_executed',True),('resources_retained',True),('cleanup_errno',16),('uid',2000),('pixel_oracle_on_host',False),('same_job_replay',False),('run_nonce','b'*32)]:
  d=copy.deepcopy(data);d[key]=value;reject('metadata '+key,d=d)
 for key in data['cases'][0]:
  d=copy.deepcopy(data);d['cases'][0][key]+=1;reject('case '+key,d=d)
 def changed(name,at,value):
  b=dict(captured);raw=bytearray(b[name]);raw[at]=value(raw[at]);b[name]=bytes(raw);return b
 first='depth-shadow-gob-000.raw';c=check['cases'][0]
 reject('full canonical guard',b=changed(first,0,lambda v:v^1))
 reject('missing retile writes',b={**captured,first:expected_input(c,0)})
 # Actual skipped UPLOAD retains different private CLEAR0.9375 and still
 # executes the same depth test, rather than merely changing a counter.
 raw=bytearray(captured[first]);z=.5
 for y in range(c['height']):
  for x in range(c['width']):
   dep=z if 3<=x<c['width']-4 and 2<=y<c['height']-3 and accepts(c['compare'],z,.9375)else .9375
   word=depth_word(c,dep)
   for byte in range(c['cpp']):raw[address(c,c['level'],c['layer'],x*c['cpp']+byte,y)]=word[byte]
 reject('skipped A2D UPLOAD retains private CLEAR0.9375',b={**captured,first:bytes(raw)})
 reject('wrong mip write',b=changed(first,address(c,1,0,0,0),lambda v:v^0x55))
 c=check['cases'][9];name='depth-shadow-gob-018.raw';reject('wrong array layer write',b=changed(name,address(c,0,1,0,0),lambda v:v^0x55))
 c=check['cases'][0];name='depth-shadow-gob-001.raw';raw=bytearray(captured[name]);prior=expected_input(c,0)
 for byte in range(c['cpp']):at=address(c,c['level'],c['layer'],byte,0);raw[at]=prior[at]
 reject('stale replay canonical input with current guards',b={**captured,name:bytes(raw)})
 reject('target color GPU noop',b=changed('depth-shadow-color-000.raw',4096+3*check['cases'][0]['color_pitch']+4*4,lambda v:v^0x7f))
 reject('complete inventory missing canonical',b={k:v for k,v in captured.items()if k!=first})
 reject('unexpected raw file',b={**captured,'extra':b''})
 reject('truncated full canonical',b={**captured,first:captured[first][:-1]})
 altered=copy.deepcopy(check);altered['cases'][0]['levels'][1]['offset']+=512;reject('source metadata mip offset',c=altered)
 c=check['cases'][18];name='depth-shadow-gob-036.raw';reject('unselected D24 X8 exact',b=changed(name,address(c,1,0,3,0),lambda v:v^0x55))
 # Selected D24 unused X8 has no stencil/preservation contract. Positive
 # mutation proves the explicit mask is narrow; unselected X8 above is exact.
 b=changed(name,address(c,c['level'],c['layer'],3,0),lambda v:v^0x55);validate(data,b,check,'a'*32,10001)
 nonexecuting_rejected=False
 if host_result:
  d=json.loads(host_result.read_text());d.update(run_nonce='a'*32,run_directory='pipeline-'+'a'*32,uid=10001);b={name:(host_raw/name).read_bytes()for name in NAMES};reject('nonexecuting HOST GPU pixels',d=d,b=b);nonexecuting_rejected=True
 return dict(passed=True,collector_synthetic_only=True,gpu_execution=False,positive=answer,negative_cases=len(negative),negatives=negative,selected_d24x8_undefined_positive=True,nonexecuting_HOST_pixels_rejected=nonexecuting_rejected)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',type=Path,required=True);p.add_argument('--host-result',type=Path);p.add_argument('--host-raw',type=Path);a=p.parse_args();print(json.dumps(run(json.loads(a.check.read_text()),a.host_result,a.host_raw),indent=2))
