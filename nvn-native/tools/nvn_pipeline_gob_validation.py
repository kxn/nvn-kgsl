"""Offline independent canonical GOB pixel/padding oracle; never runtime conversion/upload.
Uses the existing public-minigbm inverse-sector reference, not production shader
coordinate arithmetic or emitted packets, and original color shader closed form.
"""
from functools import lru_cache
from pipeline_validation import expected
from texture_conversion_validation import source_addresses
import re
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(72))
def source_shape(width,height):
 bh=0
 while bh<4 and height+height//2>=16<<bh:bh+=1
 while bh and height<=8<<(bh-1):bh-=1
 pitch=(width*4+63)//64*64;rows=(height+(8<<bh)-1)//(8<<bh)*(8<<bh)
 return pitch*rows,dict(rows=height,row_bytes=width*4,source_pitch=pitch,block_height_log2=bh)
@lru_cache(maxsize=3)
def addresses(width,height):
 import numpy as np
 payload,level=source_shape(width,height);values=np.full((height,width*4),0xffffffff,dtype=np.uint32)
 # Existing independent inverse-sector tool. This is offline observation only;
 # values never become GPU input or an uploaded CPU-converted image.
 for x,y,relative in source_addresses(dict(linear=0,level=[level]),0):
  if values[y,x]!=0xffffffff:raise RuntimeError('inverse reference duplicate coordinate')
  values[y,x]=relative
 if np.any(values==0xffffffff) or int(values.max())>=payload:raise RuntimeError('incomplete inverse GOB address inventory')
 padding=np.ones(payload,dtype=bool);padding[values.reshape(-1)]=False
 if int((~padding).sum())!=width*height*4:raise RuntimeError('inverse GOB mapping aliases')
 return values,padding

def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='nvn_dynamic_pipeline',uid=uid,source_layout='gob',source_flags=9,source_flags_getter_checks=36,stage='complete',errno=0,cleanup_errno=0,submissions=72,finishes=72,completed_cases=72,recorded_handles=36,released_handles=36,files=72,replays=36,channel_checks=0,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3,guard_byte_checks=589824,readonly_byte_checks=1474560,maximum_channel_error=0)
 if type(data)is not dict or any(type(data.get(k))is not type(v) or data[k]!=v for k,v in want.items()):raise RuntimeError('public NVN GOB completion/identity/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=True).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('GOB lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('GOB nonce/namespace')
 if check.get('source_layout')!='gob' or check.get('source_flags')!=9 or check.get('target_pool_flags')!=164 or check.get('source_kind')!=254 or check.get('source_policy')!='immutable-native-no-NVIDIA-compression-Generic16Bx2-v1' or check.get('channel_checks')!=449180832:raise RuntimeError('checked GOB source policy')
 cases=data.get('cases')
 if type(cases)is not list or len(cases)!=36:raise RuntimeError('GOB case inventory')
 shapes=((64,64),(127,73),(2880,1620))
 for i,c in enumerate(cases):
  shape,rem=divmod(i,12);layout,mode=divmod(rem,4);w,h=shapes[shape];payload,_=source_shape(w,h)
  fields=dict(case=i,width=w,height=h,layout=layout,mode=mode,payload_bytes=payload,pool_bytes=payload+8192)
  if type(c)is not dict or any(type(c.get(k))is not int or c[k]!=v for k,v in fields.items()):raise RuntimeError('GOB case/source extent/mode')
 return cases

def compare(case,repeat,blob):
 import numpy as np
 w,h=case['width'],case['height'];payload,_=source_shape(w,h);index,padding=addresses(w,h)
 if type(blob)is not bytes or len(blob)!=payload+8192:raise RuntimeError('GOB raw type/size')
 raw=np.frombuffer(blob,dtype=np.uint8);canonical=raw[4096:4096+payload];actual=canonical[index].reshape(h,w,4)
 want=expected(w,h,case['mode'],repeat);delta=np.abs(actual.astype(np.int16)-want.astype(np.int16))
 if np.any(delta[:,:,:3]>1) or np.any(delta[:,:,3]):raise RuntimeError('GOB original shader pixel mismatch')
 if np.any(raw[:4096]!=0xa5) or np.any(raw[4096+payload:]!=0xa5) or np.any(canonical[padding]!=0xa5):raise RuntimeError('GOB prefix/padding/suffix guard')
 return actual,int(delta.max()),len(blob)-w*h*4

def validate(data,captured,check,nonce,uid):
 import numpy as np
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact72 GOB raw inventory')
 channels=guards=left_checks=maximum=0
 for c in cases:
  prior=None
  for repeat in range(2):
   actual,error,guard=compare(c,repeat,captured[NAMES[c['case']*2+repeat]]);maximum=max(maximum,error)
   if repeat:
    left=(np.arange(c['width'])+.5)/c['width']<.375
    if not np.array_equal(actual[:,left],prior[:,left]):raise RuntimeError('GOB replay corrupted preserved GPU-written left region')
    left_checks+=int(left.sum())*c['height']*4
   else:prior=actual.copy()
   channels+=c['width']*c['height']*4;guards+=guard
 if channels!=449180832 or guards!=13437792:raise RuntimeError('GOB offline measured matrix/guard count')
 return dict(passed=True,submissions=72,channel_checks=channels,canonical_guard_byte_checks=guards,device_outer_guard_byte_checks=data['guard_byte_checks'],preserved_replay_channels=left_checks,maximum_channel_error=maximum,scope='Public NVN original shader/UBO0/VFD/draw/replay to canonical Generic16Bx2 flags9 source target. Offline oracle only; no runtime CPU conversion or game frame claim.')
