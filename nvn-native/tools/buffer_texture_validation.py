"""Independent authored raw BUFFER numeric oracle; no runtime conversion or GPU simulation."""
import math,re,struct
from block_copy_validation import parse_result,typed_equal,up
FORMATS=tuple(range(1,25))+tuple(range(34,49))+(154,)
FLOAT={1:(1,1,'unorm'),2:(1,1,'snorm'),5:(2,1,'half'),6:(2,1,'unorm'),7:(2,1,'snorm'),10:(4,1,'float'),13:(1,2,'unorm'),14:(1,2,'snorm'),17:(2,2,'half'),18:(2,2,'unorm'),19:(2,2,'snorm'),22:(4,2,'float'),34:(4,3,'float'),37:(1,4,'unorm'),38:(1,4,'snorm'),41:(2,4,'half'),42:(2,4,'unorm'),43:(2,4,'snorm'),46:(4,4,'float'),154:(1,4,'unorm')}
CPP={**{f:1 for f in range(1,5)},**{f:2 for f in range(5,10)},**{f:4 for f in range(10,13)},**{f:2 for f in range(13,17)},**{f:4 for f in range(17,22)},**{f:8 for f in range(22,25)},**{f:12 for f in range(34,37)},**{f:4 for f in range(37,41)},**{f:8 for f in range(41,46)},**{f:16 for f in range(46,49)},154:4}
def case(n):
 f=FORMATS[n]if n<40 else(37,10,46)[n-40];cpp=CPP[f];count=(1,13,17,33)[n%4]if n<40 else(32767,32768,65536)[n-40];prefix=(n%3)*cpp if n<40 else(4,28,48)[n-40];offset=4096+prefix
 return dict(id=n,format=f,elements=count,cpp=cpp,prefix=prefix,float_result=int(f in FLOAT),source_offset=offset,source_bytes=up(offset+count*cpp+256,4096))
CASES=tuple(case(n)for n in range(43));NAMES=tuple(f'buffer-texture-{n:03d}-r{r}-m{m}.bin'for n,c in enumerate(CASES)for r in range(2)for m in range(2)if m or c['float_result'])
def validate_catalogue(cases,target):
 if type(cases)is not list or len(cases)!=43:raise RuntimeError('BUFFER43 cases required')
 for actual,expected in zip(cases,CASES):
  if type(actual)is not dict or set(actual)!=set(expected)or any(not typed_equal(actual[k],v)for k,v in expected.items()):raise RuntimeError('BUFFER actual case mismatch')
 if type(target)is not dict or target.get('width')!=64 or target.get('height')!=64 or any(type(target.get(k))is not int or target[k]<=0 for k in ('width','height','pitch','span','bo_bytes')):raise RuntimeError('actual FDL target shape')
 if target['pitch']%64 or target['pitch']<256 or target['span']<64*target['pitch']or target['bo_bytes']!=up(target['span']+4096,4096):raise RuntimeError('actual FDL target extent')
 if type(target.get('hex'))is not str or not re.fullmatch('[a-f0-9]+',target['hex']):raise RuntimeError('FDL typed target snapshot')
 return True

def element(c,r,index):
 seed=17+c['id']*7+r*29+index*7+(index>>8)*11
 if c['format']not in FLOAT:return bytes((seed+i*37)&255 for i in range(c['cpp']))
 width,n,kind=FLOAT[c['format']];out=bytearray()
 for channel in range(n):
  v=(seed+channel*3)%4
  if kind=='unorm':out.extend((0,(64 if width==1 else 16384),(128 if width==1 else 32768),((1<<(width*8))-1))[v].to_bytes(width,'little'))
  elif kind=='snorm':out.extend(((-127,-64,0,127)if width==1 else(-32767,-16384,0,32767))[v].to_bytes(width,'little',signed=True))
  else:out.extend(struct.pack('<e'if kind=='half'else'<f',(0,.25,.5,1)[v]))
 return bytes(out)
def raw(c,r):
 b=bytearray([0xa5])*c['source_bytes'];offset=c['source_offset']
 for index in range(c['elements']):b[offset+index*c['cpp']:offset+(index+1)*c['cpp']]=element(c,r,index)
 return bytes(b)
def index_at(c,r,x):
 if r:return c['elements']-1
 if c['elements']>=32767:return 0
 return ((2*x+1)*c['elements']*512+64)//(64*1024)
def pixel(c,r,mode,x):
 index=index_at(c,r,x)
 if mode:
  n=c['elements']+(index&1);return(n&255,(n>>8)&255,(n>>16)&255,255)
 data=element(c,r,index);width,components,kind=FLOAT[c['format']];values=[]
 for k in range(components):
  b=data[k*width:(k+1)*width]
  if kind=='unorm':v=int.from_bytes(b,'little')/((1<<(width*8))-1)
  elif kind=='snorm':v=max(-1,int.from_bytes(b,'little',signed=True)/((1<<(width*8-1))-1))
  else:v=struct.unpack('<e'if kind=='half'else'<f',b)[0]
  values.append(v)
 if c['format']==154:values[0],values[2]=values[2],values[0]
 values+=([0]*(3-components)+[1])if components<4 else[]
 return tuple(round((max(-1,min(1,v))*.5+.5)*255)for v in values)
def prepare_bytes(c):
 words=c['source_bytes']//4;packets=(words+16380)//16381;return up(4*(words+packets*3+6),4096)
def counts(check):
 t=check['target'];metadata=total=source=0
 md=check.get('shader_metadata')
 if type(md)is not list or len(md)!=2:raise RuntimeError('actual shader metadata required')
 for mode,value in enumerate(md):
  if type(value)is not dict or set(value)!={'mode','code_bo_bytes','has_ubo'}or any(type(v)is not int for v in value.values())or value['mode']!=mode or value['code_bo_bytes']<=0 or value['code_bo_bytes']%4096 or value['has_ubo']not in(0,1):raise RuntimeError('shader BO extent facts')
 for c in CASES:
  for r in range(2):
   for mode in range(2):
    if not mode and not c['float_result']:continue
    md=check['shader_metadata'][mode]
    metadata+=md['code_bo_bytes']+4096+md['has_ubo']*4096+49152-4+prepare_bytes(c)
    source+=c['source_bytes'];total+=c['source_bytes']+t['bo_bytes']
 return dict(data_bytes=total,metadata_readonly_byte_checks=metadata,source_readonly_byte_checks=source,cpu_guard_initialization_bytes=132*t['bo_bytes'])
def validate(data,captured,nonce,uid,check):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('ordinary UID/fresh nonce')
 validate_catalogue(check.get('cases'),check.get('target'));t=check['target'];exact=dict(schema=1,operation='buffer_texture',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=43,formats=40,prepares=86,draws=132,submissions=218,finishes=132,files=132,inflight_destroy_rejections=264,gpu_timestamp_write_words=132,bad_case=0xffffffff,bad_repeat=0xffffffff,bad_mode=0xffffffff,nvn_api=False,sampling=True,game_shader=False,cpu_image_conversion=False,cpu_source_writes=0,integer_texel_fetch=False,passed=True,run_nonce=nonce,run_directory='buffer-texture-'+nonce,**counts(check))
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('device result '+k)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('actual132raw inventory')
 channels=guards=readonly=total=0;maximum=[0,0]
 for c in CASES:
  for r in range(2):
   source=raw(c,r)
   for mode in range(2):
    if not mode and not c['float_result']:continue
    name=f'buffer-texture-{c["id"]:03d}-r{r}-m{mode}.bin';b=captured[name]
    if type(b)is not bytes or len(b)!=len(source)+t['bo_bytes']or b[:len(source)]!=source:raise RuntimeError('GPU source/readonly/guards '+name)
    target=b[len(source):];mask=bytearray(len(target))
    for y in range(64):
     for x in range(64):
      want=pixel(c,r,mode,x);off=y*t['pitch']+x*4
      for k in range(4):
       delta=abs(target[off+k]-want[k]);maximum[mode]=max(maximum[mode],delta)
       if delta>(1 if mode==0 else 0):raise RuntimeError(f'actual BUFFER pixel {name} xy{x},{y} c{k} actual{target[off+k]} expected{want[k]}')
       mask[off+k]=1;channels+=1
    for i,v in enumerate(target):
     if not mask[i]:
      if v!=0xa5:raise RuntimeError('target guard '+name)
      guards+=1
    readonly+=len(source);total+=len(b)
 return dict(passed=True,raw_files=132,submissions=218,finishes=132,channels=channels,guards=guards,source_readonly_byte_checks=readonly,total_bytes=total,max_channel_delta=maximum,tolerance=[1,0],public_nvn=False,game_shader=False,integer_texel_fetch=False,scope='authored raw-native FLOAT BUFFER TXF20formats+count-boundaries and 40format TXS; no UINT/SINT TXF or original Maxwell/NVN/game claim')
