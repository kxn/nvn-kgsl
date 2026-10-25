"""Closed original C1 formula and strict actual-public-NVN collector, not shader execution."""
import math,re,struct
FORMATS=(1,5,6,10,13,17,18,22,37,41,42,46)
NAMES=tuple(f'nvn-pipeline-{i:02d}.raw'for i in range(48))
def shape(f):
 components=1 if f<=10 else 2 if f<=22 else 4
 width=1 if f in(1,13,37)else 2 if f in(5,6,17,18,41,42)else 4
 return components,width,components*width

def palette(f,phase):
 components,width,cpp=shape(f);values=[]
 for k in range(components):
  n=0 if phase==3 and k==3 else 1+(phase+k)%3
  values.append((0,64/255,128/255,192/255)[n]if width==1 else(0,16384/65535,32768/65535,49152/65535)[n]if f in(6,18,42)else(0,.25,.5,.75)[n])
 return values+[0]*(3-components)+[1]if components<4 else values

def source(f,phase):
 components,width,cpp=shape(f);b=bytearray(b'\xa5'*8192);offset=4096+cpp
 for index in range(17):
  for k in range(components):
   n=0 if phase==3 and k==3 else 1+(phase+index+k)%3
   value=(0,.25,.5,.75)[n]
   word=bytes([(0,64,128,192)[n]])if width==1 else (0,16384,32768,49152)[n].to_bytes(2,'little')if f in(6,18,42)else struct.pack('<e'if width==2 else'<f',value)
   b[offset+(index*components+k)*width:offset+(index*components+k+1)*width]=word
 return bytes(b)

def index_image(phase):
 b=bytearray(b'\xa5'*24576);base=phase&~1
 for y in range(64):
  for x in range(64):b[4096+y*256+x*4:4100+y*256+x*4]=bytes((0,x+base,y+base,255))
 return bytes(b)

def pixels(f,phase):
 p=palette(f,phase);out=bytearray()
 for y in range(64):
  for x in range(64):
   if not p[3]:out.extend((17,29,43,59));continue
   u=(x+.5)/64;v=(y+.5)/64;c=(.25+.25*u,.375+.25*v,.5+.125*(u+v),1)
   out.extend(math.floor(a*b*255+.5)for a,b in zip(c,p))
 return bytes(out)

def raw(f,phase):
 base=phase&~1
 return b'\xa5'*4096+pixels(f,phase)+b'\xa5'*4096+index_image(phase)+source(f,base)+source(f,base+1)

def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce')
 if check.get('kind')!='public-nvn-buffer-sampled-qualification-v1'or check.get('formats')!=list(FORMATS)or check.get('index_domain')!=[0]:raise RuntimeError('checked BUFFER12 source/oracle required')
 exact=dict(schema=1,operation='nvn_buffer_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=48,finishes=48,completed_cases=48,files=48,formats=12,recorded_handles=36,released_handles=36,copy_commands=12,call_commands=12,replays=24,shader_source_finalizes=1,texture_registrations=60,registered_cpu_object_finalizes=60,inflight_destroy_rejections=96,channel_checks=786432,guard_byte_checks=393216,readonly_byte_checks=7077888,index_domain=[0],full_c1_bytes=256,graphics_control_profile_bytes=2176,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False,passed=True,bad_case=0xffffffff,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce)
 if type(data)is not dict:raise RuntimeError('device result object')
 for k,v in exact.items():
  if type(data.get(k))is not type(v)or data[k]!=v or isinstance(v,list)and any(type(x)is not int for x in data[k]):raise RuntimeError('actual public BUFFER result '+k)
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('pixel tolerance')
 return exact

def validate(data,captured,check,nonce,uid):
 metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact BUFFER48 actual raw inventory')
 channels=guards=sourcechecks=maximum=0
 for i,f in enumerate(FORMATS):
  for phase in range(4):
   name=NAMES[4*i+phase];blob=captured[name];want=raw(f,phase)
   if type(blob)is not bytes or len(blob)!=65536:raise RuntimeError('actual raw size '+name)
   if blob[:4096]!=want[:4096]or blob[20480:]!=want[20480:]:raise RuntimeError('actual prefix/guard/index image/Buffer sources '+name)
   for k,(a,b)in enumerate(zip(blob[4096:20480],want[4096:20480])):
    delta=abs(a-b);maximum=max(maximum,delta)
    if delta>1:raise RuntimeError(f'original C1 pixel {name} byte{k} actual{a} expected{b}')
   channels+=16384;guards+=8192;sourcechecks+=40960
 return dict(passed=True,submissions=48,channel_checks=channels,guard_byte_checks=guards,source_raw_byte_checks=sourcechecks,readonly_byte_checks=data['readonly_byte_checks'],maximum_channel_error=maximum,index_domain=[0],scope='Actual public NVN original bounded C1 + two real resources, FloatTXF12 and index0 only, late immutable versions, Copy/Call replay. No genuine game binding/frame or general C1 index-texture claim.')
