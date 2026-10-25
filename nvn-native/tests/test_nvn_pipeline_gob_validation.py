"""HOST synthetic negative oracle only. No device/packet/shader execution."""
from pathlib import Path
import copy,json,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import nvn_pipeline_gob_validation as v
nonce='a'*32;check=dict(source_layout='gob',source_flags=9,target_pool_flags=164,source_kind=254,source_policy='immutable-native-no-NVIDIA-compression-Generic16Bx2-v1',channel_checks=449180832)
d=dict(schema=1,operation='nvn_dynamic_pipeline',uid=10001,source_layout='gob',source_flags=9,source_flags_getter_checks=36,stage='complete',errno=0,cleanup_errno=0,submissions=72,finishes=72,completed_cases=72,recorded_handles=36,released_handles=36,files=72,replays=36,channel_checks=0,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3,guard_byte_checks=589824,readonly_byte_checks=1474560,maximum_channel_error=0,passed=True,resources_retained=False,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=True,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,cases=[])
for i in range(36):
 shape,rem=divmod(i,12);layout,mode=divmod(rem,4);w,h=((64,64),(127,73),(2880,1620))[shape];payload,_=v.source_shape(w,h)
 d['cases'].append(dict(case=i,width=w,height=h,layout=layout,mode=mode,payload_bytes=payload,pool_bytes=payload+8192))
assert len(v.metadata(d,check,nonce,10001))==36;checks=1
for key,value in [('source_flags',8),('source_flags_getter_checks',35),('channel_checks',1),('readonly_byte_checks',1),('guard_byte_checks',1),('submissions',71),('released_handles',35),('offline_pixel_oracle_required',False),('resources_retained',True)]:
 bad=copy.deepcopy(d);bad[key]=value
 try:v.metadata(bad,check,nonce,10001)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
for w,h in ((64,64),(127,73)):
 payload,_=v.source_shape(w,h);index,padding=v.addresses(w,h)
 case=dict(width=w,height=h,mode=3)
 for repeat in (0,1):
  raw=np.full(payload+8192,0xa5,dtype=np.uint8);raw[4096+index]=v.expected(w,h,3,repeat).reshape(h,w*4)
  assert v.compare(case,repeat,raw.tobytes())[1]==0;checks+=1
  offsets=[0,4096+int(index[0,0]),4096+int(index[0,3]),len(raw)-1]
  if padding.any():offsets.append(4096+int(np.flatnonzero(padding)[0]))
  for offset in offsets:
   changed=raw.copy();changed[offset]^=127
   try:v.compare(case,repeat,changed.tobytes())
   except RuntimeError:checks+=1
   else:raise AssertionError((w,h,offset))
print(json.dumps(dict(passed=True,checks=checks,scope='HOST synthetic flags9 GOB metadata/inverse-oracle negatives only; no GPU execution')))
