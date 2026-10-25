"""Independent authored raw byte oracle. Actual FDL native layout is input metadata,
not a handwritten replacement. Canonical BD/Z/sector addressing is independent
of production layout/offset functions. No decoding or runtime conversion.
"""
import re
from block_copy_validation import pattern,parse_result,typed_equal,up
from public_volume_copy_validation import coordinate
NAMES=tuple(f'volume-shadow-{n:03d}-r{r}.bin'for n in range(18)for r in range(2))
FORMATS=(37,0x29,0x41,0x42,0x43,0x44)
def spec(n):
 if type(n)is not int or not 0<=n<18:raise RuntimeError('shadow case id')
 f=FORMATS[n//3];v=n%3;w,h,z,levels=((32,32,32,1),(35,19,9,3),(11,17,5,4))[v];base=(0,1,2)[v];cpp=4 if f==37 else 8 if f in(0x29,0x41,0x42)else 16;block=1 if f in(37,0x29)else 4;bd=0
 while bd<4 and z+z//2>=2<<bd:bd+=1
 mips=[];depths=[];bds=[];offset=0
 for i in range(levels):
  wi,hi,zi=max(1,w>>i),max(1,h>>i),max(1,z>>i);bx,by=(wi+block-1)//block,(hi+block-1)//block;row=bx*cpp;pitch=up(row,64);b=bd
  while b and zi<=1<<(b-1):b-=1
  size=pitch*up(by,8)*up(zi,1<<b);mips.append([offset,size,wi,hi,bx,by,row,pitch,0]);depths.append(zi);bds.append(b);offset+=size
 return dict(id=n,format=f,width=w,height=h,depth=z,levels=levels,base_level=base,level_count=levels-base,flags=4 if f==0x29 else 0,payload_bytes=offset,canonical_offset=512,canonical_bo_bytes=up(offset+512+4096,4096),mips=mips,mip_depths=depths,mip_bds=bds)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=18:raise RuntimeError('shadow catalogue')
 for n,c in enumerate(cases):
  s=spec(n)
  for k in ('id','format','width','height','depth','levels','base_level','level_count','flags','payload_bytes','canonical_offset','canonical_bo_bytes'):
   if not typed_equal(c.get(k),s[k]):raise RuntimeError('shadow source case '+str(n)+' '+k)
  if type(c.get('native_span'))is not int or not 0<c['native_span']<1<<32 or c.get('native_bo_bytes')!=up(c['native_span'],4096):raise RuntimeError('shadow native owner span')
  if type(c.get('native_levels'))is not list or len(c['native_levels'])!=s['levels']:raise RuntimeError('shadow mip count')
  intervals=[]
  for i,m in enumerate(c['native_levels']):
   if type(m)is not list or len(m)!=7 or any(type(v)is not int or v<0 for v in m):raise RuntimeError('shadow FDL level')
   w,h,z,pitch,off,slice_stride,span=m;sm=s['mips'][i]
   if (w,h,z)!=(sm[2],sm[3],s['mip_depths'][i])or pitch<sm[6]or slice_stride<pitch*sm[5]or span!=slice_stride*z or off%4 or off+span>c['native_span']:raise RuntimeError('shadow FDL mip bounds')
   if any(off<hi and lo<off+span for lo,hi in intervals):raise RuntimeError('shadow mip overlap')
   intervals.append((off,off+span))
  for name in ('source_hex','native_hex'):
   if type(c.get(name))is not str or not re.fullmatch('[0-9a-f]+',c[name])or len(c[name])%2:raise RuntimeError('shadow typed metadata blob')
  if len(bytes.fromhex(c['native_hex']))!=768:raise RuntimeError('shadow volume ABI')
 return True
def expected(c,repeat):
 s=spec(c['id']);source=bytearray([0xa5])*s['canonical_bo_bytes'];native=bytearray([0xa5])*c['native_bo_bytes'];n=s['id']
 for i in range(s['payload_bytes']):source[512+i]=pattern(512+i,n,repeat)
 active=0
 for level,m in enumerate(s['mips']):
  off=c['native_levels'][level][4];pitch=c['native_levels'][level][3];slice_stride=c['native_levels'][level][5];view=dict(mips=s['mips'],mip_depths=s['mip_depths'],mip_bds=s['mip_bds'],level=level)
  for z in range(s['mip_depths'][level]):
   for y in range(m[5]):
    start=off+z*slice_stride+y*pitch
    for x in range(m[6]):native[start+x]=pattern(512+coordinate(view,z,x,y),n,repeat)
    active+=m[6]
 return bytes(source+native),active,s['canonical_bo_bytes'],c['native_bo_bytes']
def validate(data,captured,nonce,uid,check):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('shadow ordinary UID/nonce')
 cases=check.get('cases');validate_cases(cases)
 exact=dict(schema=1,operation='volume_shadow',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=18,formats=6,completed=36,submissions=96,finishes=36,replays=18,files=36,owners=18,inflight_destroy_rejections=72,sequence_word_checks=96,bad_case=0xffffffff,bad_repeat=0xffffffff,nvn_api=False,sampling=False,game_shader=False,cpu_image_conversion=False,passed=True,run_nonce=nonce,run_directory='volume-shadow-'+nonce)
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('shadow result mismatch '+k)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('shadow raw inventory')
 total=active=readonly=guard_init=0
 for c in cases:
  for r in range(2):
   name=NAMES[2*c['id']+r];want,writes,source,fill=expected(c,r);actual=captured[name]
   if type(actual)is not bytes or actual!=want:raise RuntimeError('shadow actual byte mismatch '+name)
   total+=len(want);active+=writes;readonly+=source;guard_init+=fill
 counts=dict(data_byte_checks=total,active_byte_checks=active,canonical_readonly_byte_checks=readonly,native_cpu_guard_initialization_bytes=guard_init)
 for k,v in counts.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('shadow exact bytes count '+k)
 code=check.get('kernel_binary_bytes')
 if type(code)is not int or not 0<code<=65536:raise RuntimeError('shadow kernel code extent')
 # Current actual common nn_cs_encode allocates8192; immutable UBO allocates64
 # rounded to the qualified4096 device page. Sources are hash-bound by bundle.
 metadata=sum(2*(up(code,4096)+c['levels']*(4096+8192-4))for c in cases)
 if not typed_equal(data.get('metadata_readonly_byte_checks'),metadata):raise RuntimeError('shadow metadata readonly')
 counts['metadata_readonly_byte_checks']=metadata
 return dict(passed=True,raw_files=36,submissions=96,finishes=36,replays=18,byte_tolerance=0,**counts,guard_byte_checks=total-active-readonly,scope='Raw native owner true3D canonical to actual FDL sampled storage; authored guard CPU fill only; no texture-unit sampling, public NVN Draw, game or display claim')
