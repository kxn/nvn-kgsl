"""Offline public GOB22 oracle: inverse sectors + actual Mesa pixel utility.
No result of this module is uploaded or used by the Android runtime.
"""
from functools import lru_cache
import hashlib,re,struct
from texture_conversion_validation import source_addresses
from color_target_validation import decode
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(132))
@lru_cache(maxsize=22)
def addresses(storage,cpp,pitch,bh):
 import numpy as np
 values=np.full((41,73*cpp),0xffffffff,dtype=np.uint32)
 case=dict(linear=0,level=[dict(rows=41,row_bytes=73*cpp,source_pitch=pitch,block_height_log2=bh)])
 for x,y,offset in source_addresses(case,0):
  if values[y,x]!=0xffffffff or offset>=storage:raise RuntimeError('GOB inverse reference overlap/extent')
  values[y,x]=offset
 if np.any(values==0xffffffff)or np.unique(values).size!=41*73*cpp:raise RuntimeError('incomplete GOB inverse reference')
 padding=np.ones(storage,dtype=bool);padding[values.reshape(-1)]=False
 return values,padding

def readonly_inputs(state,repeat):
 uniform=bytearray(b'\xa5'*4096);vertex=bytearray(b'\xa5'*4096);matrix=[0.]*16
 for i in (0,5,10,15):matrix[i]=1.
 matrix[3]=.5 if repeat else 0.;uniform[256:320]=struct.pack('<16f',*matrix)
 rgba=(.25,.5,.75,.5)if not repeat else(.75,.25,.5,.5)if state==1 else(-.5,1.5,2.,.5)
 for i,pos in enumerate(((-1.,-1.),(3.,-1.),(-1.,3.))):vertex[256+24*i:280+24*i]=struct.pack('<6f',*pos,*rgba)
 return bytes(uniform+vertex)

def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('ordinary UID/fresh nonce')
 if check.get('kind')!='public-nvn-gob-color-targets-qualification-v1'or check.get('submissions')!=132 or len(check.get('formats',[]))!=22:raise RuntimeError('normal public GOB22 record')
 formats=check['formats'];outer=sum((v['pool_bytes']-v['storage_bytes'])*6 for v in formats);active=sum(73*41*v['cpp']*6 for v in formats)
 exact=dict(schema=1,operation='nvn_gob_color_targets',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,formats=22,submissions=132,finishes=132,completed_cases=132,files=132,recorded_handles=198,released_handles=198,copy_commands=66,call_commands=66,replays=66,source_address_checks=132,shader_source_finalizes=1,inflight_finalize_rejections=264,guard_byte_checks=outer,readonly_byte_checks=132*139264,active_bytes_saved=active,pixel_checks=0,pixel_oracle_on_host=True,same_handle_replay=True,source_flags=9,pool_flags=164,canonical_prefix=4096,source_kind=254,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False,passed=True,bad_case=0xffffffff,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce)
 if type(data)is not dict:raise RuntimeError('result object required')
 for k,v in exact.items():
  if type(data.get(k))is not type(v)or data[k]!=v:raise RuntimeError('public GOB22 completion/identity '+k)
 cases=data.get('cases')
 if type(cases)is not list or len(cases)!=66:raise RuntimeError('66 actual canonical state cases')
 for i,c in enumerate(cases):
  fi,state=divmod(i,3);v=formats[fi];fields=dict(case=i,format=v['format'],state=state,storage_bytes=v['storage_bytes'],pool_bytes=v['pool_bytes'],cpp=v['cpp'],gob_pitch=v['gob_pitch'],block_height_log2=v['block_height_log2'],handle_kind='call'if state==1 else'copy')
  if type(c)is not dict or any(type(c.get(k))is not type(w)or c[k]!=w for k,w in fields.items()):raise RuntimeError('actual case/source shape '+str(i))
 return cases

def validate(data,captured,check,nonce,uid):
 import numpy as np
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact132 raw inventory')
 channels=guards=readonly=preserved=masked=bytes_checked=0;maximum=0.
 for i,c in enumerate(cases):
  v=check['formats'][i//3];index,padding=addresses(c['storage_bytes'],c['cpp'],c['gob_pitch'],c['block_height_log2']);prior=None
  for repeat in range(2):
   number=i*2+repeat;name=NAMES[number];blob=captured[name];target_bytes=c['pool_bytes']
   if type(blob)is not bytes or len(blob)!=target_bytes+270336:raise RuntimeError('complete target/input/arena pre+post bytes '+name)
   raw=np.frombuffer(blob,dtype=np.uint8);canonical=raw[4096:4096+c['storage_bytes']];pixels=canonical[index].reshape(41,73*c['cpp'])
   if np.any(raw[:4096]!=0xa5)or np.any(raw[4096+c['storage_bytes']:target_bytes]!=0xa5)or np.any(canonical[padding]!=0):raise RuntimeError('canonical pixel-neighbor/prefix/suffix guard '+name)
   guards+=target_bytes-73*41*c['cpp']
   if blob[target_bytes:target_bytes+8192]!=readonly_inputs(c['state'],repeat):raise RuntimeError('actual VFD/UBO source changed '+name)
   pre=blob[target_bytes+8192:target_bytes+139264];post=blob[target_bytes+139264:]
   if pre!=post:raise RuntimeError('original command-arena pre/post changed '+name)
   readonly+=139264
   # Offline gather into a HOST-only linear decode window, never GPU input.
   native=bytearray(4096+v['pitch']*41)
   for y in range(41):native[4096+y*v['pitch']:4096+y*v['pitch']+73*c['cpp']]=pixels[y].tobytes()
   actual=decode(bytes(native),dict(format=c['format'],pitch=v['pitch']),check)
   from pathlib import Path
   path=Path(check['expected'])/('expected-%03u.f32'%number)
   if check['artifacts'].get(str(path))!=hashlib.sha256(path.read_bytes()).hexdigest():raise RuntimeError('normal HOST expected changed')
   wanted=np.frombuffer(path.read_bytes(),dtype='<f4').reshape(41,73,4)
   if not np.all(np.isfinite(actual))or not np.all(np.isfinite(wanted)):raise RuntimeError('nonfinite color output')
   for channel in range(4):
    if not(v['component_mask']&(1<<channel)):continue
    a=actual[:,:,channel].astype(np.float64);w=wanted[:,:,channel].astype(np.float64);bits=v['channel_bits'][channel]
    if v['srgb']and channel<3:
     def srgb(x):return np.where(x<=.0031308,x*12.92,1.055*np.power(np.maximum(x,0),1/2.4)-.055)
     units=np.abs(srgb(a)-srgb(w))*255
    elif v['floating']:
     mantissa=bits-5 if bits in(10,11)else 10 if bits==16 else 23
     ulp=np.exp2(np.floor(np.log2(np.maximum(np.abs(w),np.finfo(np.float32).tiny)))-mantissa)
     ulp=np.maximum(ulp,np.exp2(-24 if bits==16 else -14-mantissa if bits in(10,11)else -149));units=np.abs(a-w)/ulp
    else:units=np.abs(a-w)*((1<<(bits-1))-1 if v['snorm']else(1<<bits)-1)
    maximum=max(maximum,float(units.max()));channels+=73*41
    if np.any(units>1.0001):raise RuntimeError(f'{name} component{channel} maxstorageerror{float(units.max())}')
    if c['state']==2 and channel in(1,3):
     if not np.array_equal(a,w):raise RuntimeError('masked target component changed')
     masked+=73*41
   pixels=pixels.reshape(41,73,c['cpp'])
   if repeat:
    left=(np.arange(73)+.5)/73<.25
    if not np.array_equal(pixels[:,left],prior[:,left]):raise RuntimeError('recorded handle replay overwrote preserved pixels')
    preserved+=int(left.sum())*41*c['cpp']
   else:prior=pixels.copy()
   bytes_checked+=len(blob)
 return dict(passed=True,formats=22,public_nvn=True,submissions=132,copy_submissions=88,call_submissions=44,same_handle_replays=66,channel_checks=channels,canonical_guard_byte_checks=guards,readonly_byte_checks=readonly,preserved_replay_bytes=preserved,masked_components=masked,full_raw_bytes=bytes_checked,maximum_storage_units=maximum,scope='Authored original color source through actual public NVN canonical GOB22 target Draw/Copy/Call/retile; not a real D2R game frame.')
