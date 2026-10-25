"""Offline independent normalized2D filtering math; no runtime upload/conversion."""
import math
import struct

JOBS=288
SUBMITS=JOBS*2
NAMES=tuple('sampler-mip-%03d.raw'%i for i in range(SUBMITS))
PROFILES=(
 (-2.,0.,1000.,0.),
 (1.25,0.,1000.,0.),
 (2.75,0.,1000.,0.),
 (1.25,0.,1000.,.5),
 (2.75,0.,1000.,-.5),
 (3.25,0.,2.25,0.),
 (1.25,2.25,3.25,0.),
 (5.25,0.,1000.,0.),
)

def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def number(value):return struct.unpack('<f',struct.pack('<I',value))[0]

def catalogue(views):
 rows=[]
 for minimum in range(6):
  for wrap in range(3):
   for view,v in enumerate(views):
    for profile,(lod,lo,hi,bias)in enumerate(PROFILES):
     rows.append(dict(id=len(rows),min_filter=minimum,mag_filter=(minimum+wrap+view+profile)%2,wrap_index=wrap,view=view,profile=profile,
      scale_bits=[bits(2.**lod*64/v['view_width']),bits(2.**lod*64/v['view_height'])],lod_min_bits=bits(lo),lod_max_bits=bits(hi),lod_bias_bits=bits(bias)))
 return rows

def source(level,x,y,c,repeat):return 32+(x*13+y*7+c*37+level*29+repeat*17)%192

def index(value,n,wrap):
 import numpy as np
 if wrap==0:return value%n
 if wrap==1:
  q=value%(2*n);return np.where(q<n,q,2*n-1-q)
 return np.clip(value,0,n-1)

def level_sample(uvx,uvy,physical,linear,wrap,repeat,view):
 import numpy as np
 w=max(1,view['width']>>physical);h=max(1,view['height']>>physical)
 def texel(x,y):
  xx=index(x,w,wrap);yy=index(y,h,wrap)
  return np.stack([np.broadcast_to(source(physical,xx,yy,c,repeat),(64,64))for c in range(4)],axis=2).astype(np.float64)
 if not linear:return texel(np.floor(uvx*w).astype(np.int64),np.floor(uvy*h).astype(np.int64))
 x=uvx*w-.5;y=uvy*h-.5;ix=np.floor(x).astype(np.int64);iy=np.floor(y).astype(np.int64)
 fx=(x-ix)[...,None];fy=(y-iy)[...,None]
 return (texel(ix,iy)*(1-fx)+texel(ix+1,iy)*fx)*(1-fy)+(texel(ix,iy+1)*(1-fx)+texel(ix+1,iy+1)*fx)*fy

def expected(case,view,repeat):
 import numpy as np
 sx,sy=map(number,case['scale_bits'])
 # This axis-aligned affine shader has real implicit derivative lengths known
 # from its actual F32 uniform and FDL selected view dimensions. No CPU shader
 # interpreter is used; the closed-form shader is a single texture SAMPLE.
 lod=math.log2(max(abs(sx)*view['view_width']/64,abs(sy)*view['view_height']/64))
 bias=math.trunc(number(case['lod_bias_bits'])*256)/256
 raw_lod=lod+bias
 minimum=case['min_filter'];mag=raw_lod<0
 linear=bool(case['mag_filter']if mag else minimum&1)
 lo=max(0.,min(4095/256,number(case['lod_min_bits'])))
 hi=max(0.,min(4095/256,number(case['lod_max_bits'])))
 clamped=max(lo,min(hi,raw_lod));last=view['level_count']-1
 clamped=max(0.,min(last,clamped))
 x=(np.arange(64,dtype=np.float64)+.5)[None,:]/64
 y=(np.arange(64,dtype=np.float64)+.5)[:,None]/64
 uvx=x*sx+(-721/2048+repeat*.125);uvy=y*sy+(-1297/4096-repeat*.25)
 # Filter transition boundaries are deliberately excluded by the profiles.
 # Nonmip minfilter and clear magnification select the actual base view.
 base=view['base_level']
 if mag or minimum<2:
  result=level_sample(uvx,uvy,base,linear,case['wrap_index'],repeat,view)
 elif minimum<4:
  selected=min(last,int(math.floor(clamped+.5)))
  result=level_sample(uvx,uvy,base+selected,linear,case['wrap_index'],repeat,view)
 else:
  first=int(math.floor(clamped));second=min(last,first+1);fraction=clamped-first
  result=(1-fraction)*level_sample(uvx,uvy,base+first,linear,case['wrap_index'],repeat,view)+fraction*level_sample(uvx,uvy,base+second,linear,case['wrap_index'],repeat,view)
 return np.floor(np.clip(result,0,255)+.5).astype(np.uint8),bool(linear or(not mag and minimum>=4))


def nearest_boundary_margin(check):
 # Explicitly avoid an exact nearest texel boundary where normal floating
 # interpolation could make two results equally defensible. This checks every
 # exact-tolerance sample in the fixed authored matrix, never changes samples
 # in response to an actual GPU result.
 margin=1.
 for case in check['cases']:
  v=check['views'][case['view']];sx,sy=map(number,case['scale_bits'])
  lod=math.log2(max(abs(sx)*v['view_width']/64,abs(sy)*v['view_height']/64))+number(case['lod_bias_bits'])
  mag=lod<0;minimum=case['min_filter']
  if bool(case['mag_filter']if mag else minimum&1)or(not mag and minimum>=4):continue
  selected=0 if mag or minimum<2 else min(v['level_count']-1,int(math.floor(max(number(case['lod_min_bits']),min(number(case['lod_max_bits']),lod))+.5)))
  physical=v['base_level']+selected
  for repeat in range(2):
   for scale,dim,origin in((sx,max(1,v['width']>>physical),-721/2048+repeat*.125),(sy,max(1,v['height']>>physical),-1297/4096-repeat*.25)):
    for i in range(64):
     value=((i+.5)/64*scale+origin)*dim;margin=min(margin,abs(value-round(value)))
 return margin

def metadata(data,check,nonce,uid):
 import re
 if type(uid)is not int or uid<10000 or not isinstance(nonce,str)or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='native_sampler_mip',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=JOBS,submissions=SUBMITS,finishes=SUBMITS,files=SUBMITS,replays=JOBS,programs=1,pixel_checks=0,target_prefix=4096,sequence_offset=45056,sequence_bytes_exempt=4)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('sampler mip completion/count metadata mismatch')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,same_job_replay=True,cpu_image_conversion=False,cpu_fixture_source_initialization=True,nvn_api=False,game_modules_executed=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('sampler mip lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('sampler mip nonce namespace')
 if type(data.get('cases'))is not list or len(data['cases'])!=JOBS:raise RuntimeError('sampler mip case inventory')
 if check['cases']!=catalogue(check['views']):raise RuntimeError('normal case catalogue differs')
 for k in ('bo_bytes','job_bytes','command_bytes','resource_count','readonly_bytes_per_submit'):
  if type(data.get(k))is not int or data[k]<=0:raise RuntimeError('invalid owned BO extent/count')
 if data['bo_bytes']%4096 or data['bo_bytes']<check['target']['span']+8192 or data['command_bytes']%4 or data['job_bytes']<45060 or data['resource_count']!=6:raise RuntimeError('job/BO/resource bounds')
 keys=('id','min_filter','mag_filter','wrap_index','view','profile','scale_bits','lod_min_bits','lod_max_bits','lod_bias_bits')
 normalized=[]
 for row,want in zip(data['cases'],check['cases']):
  if type(row)is not list or len(row)!=11 or any(type(x)is not int for x in row):raise RuntimeError('case state row shape/type')
  expected=[want['id'],want['min_filter'],want['mag_filter'],want['wrap_index'],want['view'],want['profile'],*want['scale_bits'],want['lod_min_bits'],want['lod_max_bits'],want['lod_bias_bits']]
  if row!=expected:raise RuntimeError('case state/view/quantization mismatch')
  normalized.append(dict(want,bo_bytes=data['bo_bytes'],job_bytes=data['job_bytes'],command_bytes=data['command_bytes'],resource_count=data['resource_count']))
 if data['readonly_byte_checks']!=data['readonly_bytes_per_submit']*SUBMITS:raise RuntimeError('incomplete readonly span')
 return normalized

def validate(data,captured,check,nonce,uid):
 import numpy as np
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact fullraw sampler inventory required')
 channels=guards=maximum=exact=0;filtered_max=0
 for case in cases:
  view=check['views'][case['view']];pitch=check['target']['pitch']
  for repeat in range(2):
   name=NAMES[case['id']*2+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=case['bo_bytes']:raise RuntimeError('raw type/extent mismatch')
   raw=np.frombuffer(blob,dtype=np.uint8);rows=raw[4096:4096+pitch*64].reshape(64,pitch);actual=rows[:,:256].reshape(64,64,4)
   wanted,filtered=expected(case,view,repeat);delta=np.abs(actual.astype(np.int16)-wanted.astype(np.int16));limit=2 if filtered else 0
   maximum=max(maximum,int(delta.max()));filtered_max=max(filtered_max,int(delta.max())if filtered else 0)
   if np.any(delta>limit):
    y,x,c=map(int,np.argwhere(delta>limit)[0]);raise RuntimeError(f'{name} ({x},{y})[{c}] actual{actual[y,x,c]} expected{wanted[y,x,c]} tolerance{limit}')
   if not filtered:exact+=64*64*4
   regions=(raw[:4096],rows[:,256:].reshape(-1),raw[4096+pitch*64:])
   if any(np.any(r!=0xa5)for r in regions):raise RuntimeError('target prefix/row/layer/suffix guard changed')
   channels+=64*64*4;guards+=len(blob)-64*64*4
 if data.get('guard_byte_checks')!=guards:raise RuntimeError('device vs fullraw guard count differs')
 for k in ('readonly_byte_checks','inflight_owner_checks','unused_owner_checks'):
  if type(data.get(k))is not int or data[k]<=0:raise RuntimeError('missing readonly/owner proof')
 return dict(passed=True,submissions=SUBMITS,channel_checks=channels,guard_byte_checks=guards,nearest_exact_channels=exact,max_storage_units=maximum,filtered_max_storage_units=filtered_max,tolerance_filtered_storage_units=2,scope='Authored actual native normalized2D isotropic mip/minmag/wrap/LOD clamp+bias; not public NVN/Maxwell/D2R or anisotropic qualification')
