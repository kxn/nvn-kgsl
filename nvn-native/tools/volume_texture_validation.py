"""Independent authored block/BD-Z/raw/native/pixel oracle; no runtime pixel conversion."""
import math,re,struct
from block_copy_validation import parse_result,typed_equal,up
from public_volume_copy_validation import coordinate
from volume_shadow_validation import spec,validate_cases
NAMES=tuple(f'volume-texture-{n:03d}-r{r}-m{m}.bin'for n in range(18)for r in range(2)for m in range(2))
def block(fmt,n,r,l,z,x,y):
 seed=17+n*7+r*29+l*31+z*13+x*9+y*3
 if fmt==37:return bytes((seed&255,(seed*3+47)&255,(seed*7+19)&255,255))
 if fmt==0x29:
  half=(0,0x3400,0x3800,0x3c00);return struct.pack('<4H',*[half[3 if c==3 else(seed+c)%4]for c in range(4)])
 color=((seed%30+1)<<11)|(((seed*3)%62+1)<<5)|((seed*5)%30+1);a,b=(0,color)if seed&1 else(color,0);idx=sum(((i+seed)%4)<<(i*2)for i in range(16));rgb=struct.pack('<HHI',a,b,idx)
 if fmt in(0x41,0x42):return rgb
 if fmt==0x43:return sum(((i+seed)%16)<<(i*4)for i in range(16)).to_bytes(8,'little')+rgb
 return bytes((200,20))+sum(((i+seed)%8)<<(i*3)for i in range(16)).to_bytes(6,'little')+rgb

def validate_catalogue(cases,targets):
 validate_cases(cases)
 if type(targets)is not list or len(targets)!=18:raise RuntimeError('3D sampled targets missing')
 for n,(c,t)in enumerate(zip(cases,targets)):
  s=spec(n);w=max(1,s['width']>>s['base_level']);h=max(1,s['height']>>s['base_level'])
  exact=dict(id=n,width=w,height=h)
  for k,v in exact.items():
   if not typed_equal(t.get(k),v):raise RuntimeError('target shape '+k)
  for k in ('pitch','target_span','target_bo_bytes'):
   if type(t.get(k))is not int or t[k]<=0:raise RuntimeError('target integer '+k)
  if t['pitch']%64 or t['pitch']<w*4 or t['target_span']<t['pitch']*h or t['target_bo_bytes']!=up(t['target_span']+4096,4096):raise RuntimeError('actual FDL target extent')
  if type(t.get('target_hex'))is not str or not re.fullmatch('[0-9a-f]+',t['target_hex'])or len(t['target_hex'])%2:raise RuntimeError('target typed snapshot')
 return True

def raw(c,r):
 s=spec(c['id']);source=bytearray([0xa5])*s['canonical_bo_bytes'];native=bytearray([0xa5])*c['native_bo_bytes'];n=s['id'];cpp=4 if s['format']==37 else 8 if s['format']in(0x29,0x41,0x42)else 16;active=0
 for l,m in enumerate(s['mips']):
  view=dict(mips=s['mips'],mip_depths=s['mip_depths'],mip_bds=s['mip_bds'],level=l);nm=c['native_levels'][l]
  for z in range(s['mip_depths'][l]):
   for y in range(m[5]):
    for x in range(m[4]):
     v=block(s['format'],n,r,l,z,x,y)
     for i,value in enumerate(v):source[512+coordinate(view,z,x*cpp+i,y)]=value;native[nm[4]+z*nm[5]+y*nm[3]+x*cpp+i]=value
     active+=cpp
 return bytes(source),bytes(native),active

def decode(fmt,data,x,y):
 if fmt==37:return tuple(data)
 if fmt==0x29:return tuple(round(v*255)for v in struct.unpack('<4e',data))
 offset=8 if fmt>=0x43 else 0;a,b,indices=struct.unpack('<HHI',data[offset:offset+8]);expand=lambda v:((v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31)
 colors=[(*expand(a),255),(*expand(b),255)];four=fmt>=0x43 or a>b
 if four:
  colors.extend([tuple((2*colors[0][c]+colors[1][c])/3 for c in range(3))+(255,),tuple((colors[0][c]+2*colors[1][c])/3 for c in range(3))+(255,)])
 else:colors.extend([tuple((colors[0][c]+colors[1][c])/2 for c in range(3))+(255,),(0,0,0,0 if fmt==0x42 else 255)])
 index=y*4+x;color=list(colors[(indices>>(index*2))&3])
 if fmt==0x43:color[3]=((int.from_bytes(data[:8],'little')>>(index*4))&15)*17
 if fmt==0x44:
  a,b=data[:2];alpha=[a,b]
  if a>b:alpha.extend(((7-i)*a+i*b)/7 for i in range(1,7))
  else:alpha.extend(((5-i)*a+i*b)/5 for i in range(1,5));alpha.extend((0,255))
  color[3]=alpha[(int.from_bytes(data[2:8],'little')>>(index*3))&7]
 return tuple(round(v)for v in color)

def pixel(c,r,mode,x,y,t):
 s=spec(c['id']);lod=1 if r and s['level_count']>1 else 0;l=s['base_level']+lod;m=s['mips'][l];z=s['mip_depths'][l]-1 if r else 0
 # Authored +1/1024 bias avoids exact integer boundaries for these extents,
 # and is much larger than the finite interpolation rounding bound.
 ix=((2*x+1)*m[2]*512+t['width'])//(t['width']*1024);iy=((2*y+1)*m[3]*512+t['height'])//(t['height']*1024)
 if mode:return(m[2]+(ix&1),m[3],s['mip_depths'][l],255)
 bw=1 if s['format']in(37,0x29)else 4;data=block(s['format'],s['id'],r,l,z,ix//bw,iy//bw);return decode(s['format'],data,ix%bw,iy%bw)

def validate(data,captured,nonce,uid,check):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('ordinary UID/fresh nonce')
 cases=check.get('cases');targets=check.get('targets');validate_catalogue(cases,targets)
 exact=dict(schema=1,operation='volume_texture',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=18,formats=6,completed=72,materializations=96,draws=72,submissions=168,finishes=72,files=72,inflight_destroy_rejections=144,sequence_word_checks=192,gpu_timestamp_write_words=72,bad_case=0xffffffff,bad_repeat=0xffffffff,bad_mode=0xffffffff,nvn_api=False,sampling=True,game_shader=False,cpu_image_conversion=False,passed=True,run_nonce=nonce,run_directory='volume-texture-'+nonce)
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('device result '+k)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('actual72raw inventory')
 total=channels=guards=readonly=guard_fill=0;max_delta=[0,0]
 for n,(c,t)in enumerate(zip(cases,targets)):
  for r in range(2):
   source,native,_=raw(c,r);guard_fill+=len(native)
   for mode in range(2):
    name=NAMES[n*4+r*2+mode];actual=captured[name];prefix=source+native
    if type(actual)is not bytes or len(actual)!=len(prefix)+t['target_bo_bytes']or actual[:len(prefix)]!=prefix:raise RuntimeError('canonical/native readonly/guard '+name)
    target=actual[len(prefix):];mask=bytearray(len(target))
    for y in range(t['height']):
     for x in range(t['width']):
      want=pixel(c,r,mode,x,y,t);off=y*t['pitch']+x*4
      for component in range(4):
       delta=abs(target[off+component]-want[component]);max_delta[mode]=max(max_delta[mode],delta)
       if delta>1:raise RuntimeError(f'actual3D pixel {name} xy{x},{y} channel{component} actual{target[off+component]} expected{want[component]}')
       mask[off+component]=1;channels+=1
    for i,v in enumerate(target):
     if not mask[i]:
      if v!=0xa5:raise RuntimeError('target guard '+name)
      guards+=1
    readonly+=len(prefix);total+=len(actual);guard_fill+=len(target)
 code=check['owner_check']['kernel_binary_bytes'];meta=sum(4*(up(code,4096)+c['levels']*(4096+8192-4))for c in cases)+72*(2*4096+4096+4096+49152-4)
 for k,v in dict(data_bytes=total,cpu_guard_initialization_bytes=guard_fill,metadata_readonly_byte_checks=meta).items():
  if not typed_equal(data.get(k),v):raise RuntimeError('exact device count '+k)
 return dict(passed=True,raw_files=72,submissions=168,finishes=72,texture_unit=True,public_nvn=False,game=False,channels=channels,guards=guards,canonical_native_byte_checks=readonly,metadata_readonly_byte_checks=meta,total_bytes=total,max_channel_delta=max_delta,tolerance=1,scope='authored true3D TXF/TXS plus native owner materialization; no filtered3D/storageImage/guestShader/game proof')
