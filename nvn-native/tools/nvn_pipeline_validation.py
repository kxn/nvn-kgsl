"""Offline public-NVN color closed-form comparison; no runtime pixel conversion/upload."""
from pipeline_validation import expected
import re
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(24))
def metadata(data,check,nonce,uid,call=False):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='nvn_dynamic_pipeline',uid=uid,source_layout='linear',stage='complete',errno=0,cleanup_errno=0,submissions=24,finishes=24,completed_cases=24,recorded_handles=12,released_handles=12,files=24,replays=12,channel_checks=393216,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3)
 if type(data)is not dict or any(type(data.get(k))is not type(v) or data[k]!=v for k,v in want.items()):raise RuntimeError('public NVN completion/identity/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('public NVN lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('public NVN nonce/namespace')
 if check.get('source_layout')!='linear' or check.get('submissions')!=24 or check.get('channel_checks')!=393216:raise RuntimeError('public NVN checked qualifier shape')
 for k in ('guard_byte_checks','readonly_byte_checks'):
  if type(data.get(k))is not int or data[k]<=0:raise RuntimeError('missing guard/readonly checks')
 if data['readonly_byte_checks']!=24*5*4096:raise RuntimeError('incomplete readonly pool checks')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('pixel tolerance')
 cases=data.get('cases')
 if type(cases)is not list or len(cases)!=12:raise RuntimeError('public NVN case inventory')
 for i,c in enumerate(cases):
  layout,mode=divmod(i,4);fields=dict(case=i,width=64,height=64,layout=layout,mode=mode,payload_bytes=16384,pool_bytes=24576)
  if type(c)is not dict or any(type(c.get(k))is not int or c[k]!=v for k,v in fields.items()):raise RuntimeError('public NVN case extent/mode')
 if call:
  wanted=dict(call_commands=True,call_source_ubo_bound=False,call_parent_ubo_index=0,call_wrappers=12,call_children_finalized_before_submit=12)
  if any(type(data.get(k))is not type(v) or data[k]!=v for k,v in wanted.items()):raise RuntimeError('public Call/UBO0/retired-child qualification mismatch')
 return cases

def validate(data,captured,check,nonce,uid,call=False):
 import numpy as np
 cases=metadata(data,check,nonce,uid,call=call)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact public NVN raw inventory')
 channels=guards=left_checks=maximum=0
 for case in cases:
  prior=None
  for repeat in range(2):
   name=NAMES[case['case']*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=24576:raise RuntimeError('public NVN raw size '+name)
   raw=np.frombuffer(blob,dtype=np.uint8);actual=raw[4096:20480].reshape(64,64,4);want=expected(64,64,case['mode'],repeat)
   delta=np.abs(actual.astype(np.int16)-want.astype(np.int16));maximum=max(maximum,int(delta.max()))
   if np.any(delta[:,:,:3]>1) or np.any(delta[:,:,3]):raise RuntimeError('public NVN original shader pixel mismatch '+name)
   if np.any(raw[:4096]!=0xa5) or np.any(raw[20480:]!=0xa5):raise RuntimeError('public NVN target guard '+name)
   if repeat:
    left=(np.arange(64)+.5)/64<.375
    if not np.array_equal(actual[:,left],prior[:,left]):raise RuntimeError('same-handle replay corrupted preserved GPU output')
    left_checks+=int(left.sum())*64*4
   else:prior=actual.copy()
   channels+=16384;guards+=8192
 if channels!=393216 or guards!=data['guard_byte_checks']:raise RuntimeError('public NVN actual counts mismatch')
 return dict(passed=True,submissions=24,channel_checks=channels,guard_byte_checks=guards,preserved_replay_channels=left_checks,maximum_channel_error=maximum,scope='Actual public NVN original color Draw/API UBO0/sourceCB3/VFD/direct/index/indirect/replay. No game Draw/frame claim.')
