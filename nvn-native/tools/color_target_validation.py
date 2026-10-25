"""Independent original color math+Mesa HOST packing; offline NumPy comparison."""
import hashlib,re,subprocess
from pathlib import Path
NAMES=tuple('color-target-%03d.raw'%i for i in range(132))

def decode(blob,case,check):
 import numpy as np
 argv=[check['oracle'],'decode',str(case['format']),'73','41',str(case['pitch']),'4096']
 r=subprocess.run(argv,input=blob,capture_output=True)
 if r.returncode or len(r.stdout)!=73*41*4*4:raise RuntimeError('existing Mesa HOST unpack tool failed')
 return np.frombuffer(r.stdout,dtype='<f4').reshape(41,73,4)

def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID/nonce required')
 want=dict(schema=1,operation='native_color_targets',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=66,submissions=132,finishes=132,files=132,completed_cases=132,replays=66,pixel_checks=0,target_prefix=4096,native_ubo_binding=1,original_maxwell_cb=3,ubo_bytes=64,sequence_offset=45056,sequence_bytes_exempt=4)
 if any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('COLOR completion/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,same_job_replay=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,legacy_image_template=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('COLOR lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('COLOR namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact COLOR raw inventory')
 cases=data.get('cases');formats=check['formats']
 if type(cases)is not list or len(cases)!=66 or len(formats)!=22:raise RuntimeError('COLOR matrix')
 channels=guards=readonly=active=preserved=0;maximum=0.;mask_preserved=0
 for i,case in enumerate(cases):
  fi,state=divmod(i,3);view=formats[fi]
  fields=dict(case=i,format_index=fi,format=view['format'],state=state,width=73,height=41,cpp=view['cpp'],component_mask=view['component_mask'],pitch=view['pitch'],span=view['span'],rb_mrt_buf_info=view['rb_mrt_buf_info'],sp_ps_mrt_reg=view['sp_ps_mrt_reg'],srgb=view['srgb'])
  if any(type(case.get(k))is not int or case[k]!=v for k,v in fields.items()):raise RuntimeError('COLOR FDL/case identity')
  if any(type(case.get(k))is not int for k in('bo_bytes','job_bytes','command_bytes','resource_count'))or case['bo_bytes']<case['span']+8192 or case['bo_bytes']%4096 or case['bo_bytes']>case['span']+8192+65536 or case['job_bytes']<45060 or case['job_bytes']%4096 or case['job_bytes']>65536 or not 0<case['command_bytes']<=32768 or case['command_bytes']%4 or case['resource_count']!=6:raise RuntimeError('COLOR owner/BO bounds')
  prior=None
  for repeat in range(2):
   name=NAMES[i*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=case['bo_bytes']:raise RuntimeError('COLOR raw byte length '+name)
   raw=np.frombuffer(blob,dtype=np.uint8);rows=raw[4096:4096+case['pitch']*41].reshape(41,case['pitch']);actual=decode(blob,case,check)
   expected=Path(check['expected'])/('expected-%03u.f32'%(i*2+repeat));expected_raw=Path(check['expected'])/('expected-%03u.raw'%(i*2+repeat))
   for path in(expected,expected_raw):
    if check['artifacts'].get(str(path))!=hashlib.sha256(path.read_bytes()).hexdigest():raise RuntimeError('HOST expected changed')
   wanted=np.frombuffer(expected.read_bytes(),dtype='<f4').reshape(41,73,4)
   if not np.all(np.isfinite(actual))or not np.all(np.isfinite(wanted)):raise RuntimeError('nonfinite COLOR pixel')
   for c in range(4):
    if not(case['component_mask']&(1<<c)):continue
    a=actual[:,:,c].astype(np.float64);w=wanted[:,:,c].astype(np.float64);bits=view['channel_bits'][c]
    if view['srgb']and c<3:
     def srgb(x):return np.where(x<=.0031308,x*12.92,1.055*np.power(np.maximum(x,0),1/2.4)-.055)
     units=np.abs(srgb(a)-srgb(w))*255
    elif view['floating']:
     mantissa=bits-5 if bits in(10,11) else 10 if bits==16 else 23
     ulp=np.exp2(np.floor(np.log2(np.maximum(np.abs(w),np.finfo(np.float32).tiny)))-mantissa)
     ulp=np.maximum(ulp,np.exp2(-24 if bits==16 else -14-mantissa if bits in(10,11) else -149))
     units=np.abs(a-w)/ulp
    else:units=np.abs(a-w)*((1<<(bits-1))-1 if view['snorm']else(1<<bits)-1)
    maximum=max(maximum,float(units.max()));channels+=73*41
    if np.any(units>1.0001):
     y,x=map(int,np.argwhere(units>1.0001)[0]);raise RuntimeError(f'{name} ({x},{y})[{c}] actual{a[y,x]} expected{w[y,x]} units{units[y,x]}')
    if state==2 and c in(1,3):
     if not np.array_equal(a,w):raise RuntimeError('masked component changed')
     mask_preserved+=73*41
   pixel_bytes=rows[:,:73*case['cpp']].reshape(41,73,case['cpp'])
   if repeat:
    left=(np.arange(73)+.5)/73<.25
    if not np.array_equal(pixel_bytes[:,left],prior[:,left]):raise RuntimeError('samejob matrix replay changed preserved pixels')
    preserved+=int(left.sum())*41*case['cpp']
   else:prior=pixel_bytes.copy()
   regions=(raw[:4096],rows[:,73*case['cpp']:].reshape(-1),raw[4096+case['pitch']*41:])
   if any(np.any(x!=0xa5)for x in regions):raise RuntimeError('COLOR prefix/rowtail/layer/suffix guard '+name)
   guards+=len(blob)-73*41*case['cpp'];active+=73*41*case['cpp'];readonly+=4*4096+case['job_bytes']-4
 if guards!=data.get('guard_byte_checks')or readonly!=data.get('readonly_byte_checks')or active!=data.get('active_bytes_saved')or data.get('inflight_owner_checks')!=132*8:raise RuntimeError('COLOR real guard/readonly/owner totals')
 return dict(passed=True,formats=22,submissions=132,channel_checks=channels,guard_byte_checks=guards,readonly_byte_checks=readonly,active_bytes=active,preserved_replay_bytes=preserved,masked_components=mask_preserved,maximum_storage_units=maximum,pixel_oracle='existing Mesa HOST pack/unpack + original color closed form',scope='raw-nativefloat-compatible COLOR only; no public NVN/GOB/integer/Z/S/game claim')
