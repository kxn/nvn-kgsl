"""Independent raw-native original color shader closed-form oracle.
Uses NumPy only for offline comparison, never runtime conversion or upload.
"""
import re
NAMES=tuple('pipeline-%02d.rgba'%i for i in range(72))

def expected(width,height,mode,repeat):
 import numpy as np
 x=(np.arange(width,dtype=np.float64)+.5)/width
 y=(np.arange(height,dtype=np.float64)[:,None]+.5)/height
 shifted=(x>=.375) if repeat else np.zeros(width,dtype=bool)
 u=np.where(shifted,x-.375,x)
 a=1-(u+y)*.5;b=np.broadcast_to(u*.5,(height,width));c=np.broadcast_to(y*.5,(height,width))
 rgb=np.stack((np.where(shifted,c,a),np.where(shifted,a,b),np.where(shifted,b,c)),axis=2)
 rgba=np.empty((height,width,4),dtype=np.uint8);rgba[:,:,:3]=np.floor(np.clip(rgb,0,1)*255+.5).astype(np.uint8);rgba[:,:,3]=255
 if repeat and mode==3:rgba[:,shifted]=[255,0,255,255]
 return rgba

def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/nonce required')
 want=dict(schema=1,operation='native_pipeline',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=36,submissions=72,finishes=72,files=72,completed_cases=72,replays=36,target_prefix=4096,native_ubo_binding=1,original_maxwell_cb=3,ubo_bytes=64,channel_checks=check['channel_checks'])
 if type(data)is not dict or any(type(data.get(k))is not type(v) or data[k]!=v for k,v in want.items()):raise RuntimeError('pipeline completion/count/resource metadata mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_job_replay=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,legacy_image_template=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('pipeline lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('pipeline namespace/nonce mismatch')
 for k in ('guard_byte_checks','readonly_byte_checks','inflight_owner_checks'):
  if type(data.get(k))is not int or data[k]<=0:raise RuntimeError('pipeline guard/readonly/owner proof missing')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('invalid pixel error')
 cases=data.get('cases')
 if type(cases)is not list or len(cases)!=36:raise RuntimeError('case inventory')
 for i,c in enumerate(cases):
  shape,rem=divmod(i,12);layout,mode=divmod(rem,4);v=check['views'][shape]
  fields=dict(case=i,shape=shape,layout=layout,mode=mode,width=v['width'],height=v['height'],pitch=v['pitch'],span=v['required_span_bytes'])
  if type(c)is not dict or any(type(c.get(k))is not int or c[k]!=want for k,want in fields.items()):raise RuntimeError('case/view identity mismatch')
  if any(type(c.get(k))is not int for k in ('bo_bytes','command_bytes','resource_count')) or c['bo_bytes']<c['span']+8192 or c['bo_bytes']%4096 or c['bo_bytes']>c['span']+8192+65536 or not 0<c['command_bytes']<=32768 or c['command_bytes']%4 or not 4<=c['resource_count']<=128:raise RuntimeError('case BO/command/resource range')
 return cases

def validate(data,captured,check,nonce,uid):
 import numpy as np
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact pipeline raw inventory required')
 channels=guards=left_checks=maximum=0;cache={}
 for case in cases:
  i=case['case'];w,h,pitch=case['width'],case['height'],case['pitch'];prior=None
  for repeat in range(2):
   name=NAMES[i*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=case['bo_bytes']:raise RuntimeError('pipeline raw type/size '+name)
   raw=np.frombuffer(blob,dtype=np.uint8);rows=raw[4096:4096+pitch*h].reshape(h,pitch);actual=rows[:,:w*4].reshape(h,w,4)
   key=(w,h,case['mode']==3,repeat)
   if key not in cache:cache[key]=expected(w,h,case['mode'],repeat)
   want=cache[key];delta=np.abs(actual.astype(np.int16)-want.astype(np.int16));maximum=max(maximum,int(delta.max()))
   if np.any(delta[:,:,:3]>1) or np.any(delta[:,:,3]):
    bad=np.argwhere((delta>1)|((np.arange(4)==3)&(delta!=0)))[0];y,x,c=map(int,bad)
    raise RuntimeError(f'{name}: ({x},{y})[{c}] actual{actual[y,x,c]} expected{want[y,x,c]}')
   channels+=w*h*4
   if repeat:
    left=(np.arange(w)+.5)/w<.375
    if not np.array_equal(actual[:,left],prior[:,left]):raise RuntimeError('replay changed preserved GPU-written left region')
    left_checks+=int(left.sum())*h*4
   else:prior=actual.copy()
   regions=(raw[:4096],rows[:,w*4:].reshape(-1),raw[4096+pitch*h:])
   if any(np.any(region!=0xa5) for region in regions):raise RuntimeError(name+': prefix/rowtail/layer/suffix guard')
   guards+=len(blob)-w*h*4
 if channels!=check['channel_checks'] or guards!=data['guard_byte_checks']:raise RuntimeError('actual raw/checker count differs from device')
 return dict(passed=True,submissions=72,channel_checks=channels,guard_byte_checks=guards,preserved_replay_channels=left_checks,maximum_channel_error=maximum,scope='Actual dynamic native pipeline original color math/FDL/VFD/direct/index/indirect/replay; no NVN API/game/mixed-conversion claim')
