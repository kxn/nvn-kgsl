#!/usr/bin/env python3
"""Offline synthetic full-buffer/negative oracle tests. Never GPU qualification."""
import argparse,copy,hashlib,json,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import nvn_pipeline_runtime_views_validation as oracle
from pipeline_validation import expected
from nvn_pipeline_gob_validation import addresses

def check(path):
 c=json.loads(path.read_text());gob=c['source_layout']=='gob';captured={};nonce='c'*32;uid=10234
 fields=('case','width','height','layout','mode','payload_bytes','pool_bytes');channels=0 if gob else c['channel_checks']
 data=dict(schema=1,operation='nvn_dynamic_pipeline',uid=uid,source_layout=c['source_layout'],source_flags=9 if gob else 16,source_flags_getter_checks=36 if gob else 0,stage='complete',errno=0,cleanup_errno=0,submissions=72,finishes=72,completed_cases=72,recorded_handles=36,released_handles=36,files=72,replays=36,channel_checks=channels,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3,static_view_registrations=0,readonly_byte_checks=72*5*4096,passed=True,resources_retained=False,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=gob,runtime_target_views=True,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,maximum_channel_error=0,cases=[{k:a[k]for k in fields}for a in c['cases']],guard_byte_checks=sum(2*(a['pool_bytes']-a['payload_bytes'])for a in c['cases']))
 for a in c['cases']:
  w,h=a['width'],a['height']
  index=addresses(w,h)[0]if gob else np.arange(h,dtype=np.uint32)[:,None]*a['pitch']+np.arange(w*4,dtype=np.uint32)[None,:]
  for repeat in range(2):
   raw=np.full(a['pool_bytes'],0xa5,dtype=np.uint8);raw[4096+index]=expected(w,h,a['mode'],repeat).reshape(h,w*4);captured[oracle.NAMES[a['case']*2+repeat]]=raw.tobytes()
 result=oracle.validate(data,captured,c,nonce,uid);assert result['passed'];checks=1
 def reject(d=data,raw=captured,record=c,n=nonce,u=uid):
  nonlocal checks
  try:oracle.validate(d,raw,record,n,u)
  except RuntimeError:checks+=1
  else:raise AssertionError('corrupt evidence accepted')
 for key,value in [('submissions',True),('finishes',71),('runtime_target_views',False),('static_view_registrations',1),('readonly_byte_checks',0),('guard_byte_checks',0),('cpu_image_conversion',True),('passed',1),('resources_retained',True),('channel_checks',1),('source_flags',0),('maximum_channel_error',2)]:
  d=dict(data);d[key]=value;reject(d=d)
 reject(n='b'*32);reject(u=0)
 for key,value in [('width',True),('payload_bytes',1),('pool_bytes',4096),('mode',4)]:
  d=copy.deepcopy(data);d['cases'][0][key]=value;reject(d=d)
 for key,value in [('pitch',True),('width',64),('payload_bytes',1)]:
  d=copy.deepcopy(c);d['cases'][0][key]=value;reject(record=d)
 raw=dict(captured);raw.pop(oracle.NAMES[-1]);reject(raw=raw)
 raw=dict(captured);raw['extra']=b'';reject(raw=raw)
 a=c['cases'][0];w,h=a['width'],a['height'];index=addresses(w,h)[0]if gob else np.arange(h,dtype=np.uint32)[:,None]*a['pitch']+np.arange(w*4,dtype=np.uint32)[None,:]
 pad=np.ones(a['payload_bytes'],dtype=bool);pad[index.reshape(-1)]=False;padding=int(np.flatnonzero(pad)[0]);pixel=int(index[0,0])
 for offset in [0,-1,4096+padding,4096+pixel,4096+int(index[0,3])]:
  raw=dict(captured);blob=bytearray(raw[oracle.NAMES[0]]);blob[offset]^=0x80;raw[oracle.NAMES[0]]=bytes(blob);reject(raw=raw)
 return dict(source_layout=c['source_layout'],checks=checks,synthetic_raw_bytes=sum(map(len,captured.values())),full_channels=result['channel_checks'],full_guards=result['guard_byte_checks'],actual_GPU_submissions=0)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--linear-check',type=Path,required=True);p.add_argument('--gob-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 if a.out.exists():p.error('fresh output required')
 files=[Path(__file__).resolve(),Path(oracle.__file__),Path(sys.modules['pipeline_validation'].__file__),Path(sys.modules['nvn_pipeline_gob_validation'].__file__),a.linear_check,a.gob_check];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();inputs={str(p.resolve()):sha(p)for p in files}
 results=[check(a.linear_check),check(a.gob_check)]
 for f,h in inputs.items():assert sha(Path(f))==h
 result=dict(passed=True,scope=__doc__,android_execution=False,gpu_execution=False,results=results,inputs=inputs);a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':True,'results':results,'proof':str(a.out)}))
