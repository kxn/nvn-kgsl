"""Independent whole-buffer color/GOB oracle; never used to produce GPU input."""
import re
from pipeline_validation import expected
from nvn_pipeline_gob_validation import addresses,source_shape

SHAPES=((73,41),(257,113),(1153,641))
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(72))
def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID and nonce required')
 layout=check.get('source_layout')
 if check.get('kind')!='public-nvn-runtime-target-qualification-v1'or layout not in ('linear','gob')or check.get('runtime_target_views')is not True or check.get('static_view_registrations')!=0:raise RuntimeError('runtime-target-only profile required')
 gob=layout=='gob';cases=check.get('cases')
 if type(cases)is not list or len(cases)!=36:raise RuntimeError('exact authored target inventory required')
 channels=24*4*sum(w*h for w,h in SHAPES)
 want=dict(schema=1,operation='nvn_dynamic_pipeline',uid=uid,source_layout=layout,source_flags=9 if gob else 16,source_flags_getter_checks=36 if gob else 0,stage='complete',errno=0,cleanup_errno=0,submissions=72,finishes=72,completed_cases=72,recorded_handles=36,released_handles=36,files=72,replays=36,channel_checks=0 if gob else channels,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3,static_view_registrations=0,readonly_byte_checks=72*5*4096)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('public NVN runtime target counts/identity mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=gob,runtime_target_views=True).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('runtime target lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('runtime target nonce')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=int(not gob):raise RuntimeError('declared RGB tolerance')
 observed=data.get('cases')
 if type(observed)is not list or len(observed)!=36:raise RuntimeError('actual case inventory required')
 outer=0
 for i,(c,actual)in enumerate(zip(cases,observed)):
  shape,rem=divmod(i,12);attribute,mode=divmod(rem,4);w,h=SHAPES[shape]
  if any(type(c.get(k))is not int or c[k]!=v for k,v in dict(case=i,width=w,height=h,layout=attribute,mode=mode).items()):raise RuntimeError('authored dimension/layout/mode mismatch')
  if type(c.get('pitch'))is not int or c['pitch']<w*4 or c['pitch']%64:raise RuntimeError('actual metadata pitch')
  native=check['views'][shape]
  if native['width']!=w or native['height']!=h or native['pitch']!=c['pitch'] or type(native.get('bytes'))is not int or native['bytes']<c['pitch']*h:raise RuntimeError('actual checked FDL allocation extent')
  payload=source_shape(w,h)[0]if gob else native['bytes']
  fields=dict(case=i,width=w,height=h,layout=attribute,mode=mode,payload_bytes=payload,pool_bytes=8192+((payload+4095)//4096)*4096)
  if type(actual)is not dict or any(type(actual.get(k))is not int or actual[k]!=v or c.get(k)!=v for k,v in fields.items()):raise RuntimeError('actual source extent mismatch')
  outer+=2*(fields['pool_bytes']-payload)
 if type(data.get('guard_byte_checks'))is not int or data['guard_byte_checks']!=outer:raise RuntimeError('device outer guards incomplete')
 if check.get('channel_checks')!=channels or check.get('submissions')!=72:raise RuntimeError('checked target totals')
 return cases

def validate(data,captured,check,nonce,uid):
 import numpy as np
 cases=metadata(data,check,nonce,uid);gob=check['source_layout']=='gob'
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact target readback inventory required')
 channels=guards=preserved=maximum=0
 for c in cases:
  w,h=c['width'],c['height'];payload=c['payload_bytes'];prior=None
  if gob:index,padding=addresses(w,h)
  else:
   index=np.arange(h,dtype=np.uint32)[:,None]*c['pitch']+np.arange(w*4,dtype=np.uint32)[None,:]
   padding=np.ones(payload,dtype=bool);padding[index.reshape(-1)]=False
  for repeat in range(2):
   name=NAMES[c['case']*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=c['pool_bytes']:raise RuntimeError('target readback extent '+name)
   raw=np.frombuffer(blob,dtype=np.uint8);source=raw[4096:4096+payload];actual=source[index].reshape(h,w,4)
   want=expected(w,h,c['mode'],repeat);delta=np.abs(actual.astype(np.int16)-want.astype(np.int16));maximum=max(maximum,int(delta.max()))
   if np.any(delta[:,:,:3]>1)or np.any(delta[:,:,3]):raise RuntimeError('original shader pixel mismatch '+name)
   if np.any(raw[:4096]!=0xa5)or np.any(raw[4096+payload:]!=0xa5)or np.any(source[padding]!=0xa5):raise RuntimeError('target prefix/row-or-GOB-padding/suffix guard '+name)
   if repeat:
    left=(np.arange(w)+.5)/w<.375
    if not np.array_equal(actual[:,left],prior[:,left]):raise RuntimeError('same-handle retained pixel mismatch '+name)
    preserved+=int(left.sum())*h*4
   else:prior=actual.copy()
   channels+=w*h*4;guards+=len(blob)-w*h*4
 if channels!=check['channel_checks']:raise RuntimeError('full pixel count mismatch')
 return dict(passed=True,submissions=72,channel_checks=channels,guard_byte_checks=guards,preserved_replay_channels=preserved,maximum_channel_error=maximum,runtime_target_views=True,static_view_registrations=0,scope='Actual public NVN original color Draw/replay at three noncatalogued target dimensions; runtime actual FDL cache. No game frame claim.')
