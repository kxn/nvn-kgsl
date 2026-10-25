"""Independent HOST GLSL4.60 §8.9.4 formula; no GPU-inferred lane/wrap rules."""
import re
NAMES=tuple('gather-%03u.raw'%i for i in range(96))
def folded(i,n,wrap):
 if wrap==0:return i%n
 if wrap==1:
  i%=2*n;return i if i<n else 2*n-1-i
 if wrap==2:return min(n-1,max(0,i))
 raise RuntimeError('unknown Gather wrap')
def expected(case,repeat):
 blob=bytearray([0xa5])*case['bo_bytes'];component=case['component'];wrap=case['wrap_index']
 for y in range(16):
  yy=15-y if case['origin']else y
  j=(2*yy-9-4*repeat)//4
  for x in range(32):
   i=(2*x-17+8*repeat)//4
   for lane,(sx,sy)in enumerate(((i,j+1),(i+1,j+1),(i+1,j),(i,j))):
    sx=folded(sx,8,wrap);sy=folded(sy,4,wrap)
    blob[4096+y*case['pitch']+x*4+lane]=(sx*29+sy*17+component*53+repeat*71)&255
 return bytes(blob)
def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID/nonce required')
 want=dict(schema=1,operation='native_gather',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=48,programs=8,submissions=96,finishes=96,files=96,replays=48,inflight_owner_checks=864,unused_owner_checks=192,sparse_hole_checks=57600,pixel_checks=0,target_prefix=4096,coordinate_ubo_binding=5,coordinate_ubo_bytes=64,sequence_offset=45056,sequence_bytes_exempt=4)
 if any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('Gather completion/ownership contract')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,same_job_replay=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('Gather lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('Gather namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact Gather raw inventory')
 if type(data.get('cases'))is not list or len(data['cases'])!=48:raise RuntimeError('Gather matrix')
 guards=readonly=channels=0;maxerror=0;total=0
 tv=check['target'];sv=check['texture'];page=lambda n:(n+4095)//4096*4096
 bo=page(tv['required_span_bytes']+8192);srcbo=page(sv['required_span_bytes']+8192);job=page(45060);ro=srcbo+16384+job-4
 for i,case in enumerate(data['cases']):
  component,sparse=divmod(i//6,2);wrap,origin=divmod(i%6,2);binding=(1,13)[sparse]
  fields=dict(case=i,component=component,binding=binding,wrap_index=wrap,wrap=(1,6,7)[wrap],origin=origin,width=32,height=16,pitch=tv['pitch'],span=tv['required_span_bytes'],bo_bytes=bo,source_pitch=sv['pitch'],source_span=sv['required_span_bytes'],source_bo_bytes=srcbo,job_bytes=job,resource_count=6,readonly_bytes=ro)
  if type(case)is not dict or any(type(case.get(k))is not int or case[k]!=v for k,v in fields.items()):raise RuntimeError('Gather typed/layout/case identity')
  if type(case.get('command_bytes'))is not int or not 0<case['command_bytes']<=32768 or case['command_bytes']%4:raise RuntimeError('Gather command bounds')
  mask=np.zeros(bo,dtype=bool)
  for y in range(16):mask[4096+y*tv['pitch']:4096+y*tv['pitch']+128]=True
  for repeat in range(2):
   name=NAMES[i*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=bo:raise RuntimeError('Gather full raw byte length')
   actual=np.frombuffer(blob,dtype=np.uint8);wanted=np.frombuffer(expected(case,repeat),dtype=np.uint8)
   if not np.array_equal(actual[~mask],wanted[~mask]):raise RuntimeError('Gather prefix/padding/tail guard')
   delta=np.abs(actual[mask].astype(np.int16)-wanted[mask].astype(np.int16));error=int(delta.max());maxerror=max(maxerror,error)
   if error>1:raise RuntimeError('Gather component/lane/coordinate/replay pixels '+name)
   channels+=int(mask.sum());guards+=int((~mask).sum());readonly+=ro;total+=bo
 if data.get('guard_byte_checks')!=guards or type(data.get('guard_byte_checks'))is not int or data.get('readonly_byte_checks')!=readonly or type(data.get('readonly_byte_checks'))is not int:raise RuntimeError('Gather full guard/readonly counts')
 return dict(passed=True,files=96,full_raw_bytes=total,pixel_channels=channels,guard_bytes=guards,readonly_bytes=readonly,max_channel_error=maxerror,nvn_api=False,game_modules_executed=False,oracle='GLSL4.60 section8.9.4 fixed lane order + integer wrap, independent from runtime shader')
