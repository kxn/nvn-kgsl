"""Independent closed-form authored GLSL oracle. HOST only; never runtime shader execution."""
import math,struct,re
JOBS=96;SUBMITS=192;FILES=576
NAMES=tuple('storage-%03d.raw'%i for i in range(FILES))
def catalogue():
 rows=[]
 for program in range(4):
  for offset in range(4):
   for size in(128,512,2048):
    for shape in range(2):rows.append(dict(id=len(rows),program=program,offset=[offset*4,(3-offset)*4],bytes=size,shape=shape))
 return rows

def source(stage,index,repeat):
 if not stage and index<3:return 64+17*repeat
 if stage and index<8:return 24+7*index+11*repeat
 return 0x43120000|((stage*37+index*13+repeat*19)&255)
def writes(program):return program in(1,3)
def storage(case,stage,repeat):
 raw=bytearray([0xa5])*12288;values=[source(stage,i,repeat)for i in range(case['bytes']//4)]
 if writes(case['program']):
  start,count,mask=(16,8,0x99)if stage else(8,3,0x55)
  for i in range(count):values[start+i]=source(stage,i,repeat)^mask
 start=4096+case['offset'][stage];raw[start:start+case['bytes']]=struct.pack('<'+'I'*len(values),*values);return bytes(raw)
def pixels(case,view,repeat):
 import numpy as np
 # The authored VS produces constant x from identical first three input words;
 # FS reads index=(uint(FragCoord.x)&7). This is direct fixture algebra, not an interpreter.
 size=(case['bytes']//4)/4096 if case['program']==2 else 0.
 x=source(0,0,repeat)/255+size;y=np.asarray([source(1,i&7,repeat)/255+.25+size for i in range(view['width'])])
 rgba=np.empty((view['height'],view['width'],4),dtype=np.uint8);rgba[:,:,0]=max(0,min(255,math.floor(x*255+.5)));rgba[:,:,1]=np.floor(np.clip(y,0,1)*255+.5).astype(np.uint8)[None,:];rgba[:,:,2]=128;rgba[:,:,3]=255;return rgba

def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or not isinstance(nonce,str)or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 expected=dict(schema=1,operation='native_graphics_storage',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=JOBS,programs=4,submissions=SUBMITS,finishes=SUBMITS,files=FILES,replays=JOBS,inflight_owner_checks=SUBMITS*10,unused_owner_checks=SUBMITS*4,storage_bo_bytes=12288,resource_count=7,target_prefix=4096,storage_prefix=4096,sequence_offset=45056,sequence_bytes_exempt=4)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in expected.items()):raise RuntimeError('storage completion/owner/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_job_replay=True,cpu_fixture_initialization=True,cpu_shader_execution=False,cpu_image_conversion=False,pixel_oracle_on_host=True,storage_oracle_on_host=True,nvn_api=False,game_modules_executed=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('storage scope/lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('fresh namespace mismatch')
 if check.get('cases')!=catalogue()or check.get('jobs')!=96 or check.get('submissions')!=192 or check.get('files')!=576:raise RuntimeError('normal full matrix missing')
 target=(max(v['required_span_bytes']for v in check['views'])+8192+4095)&~4095
 if data.get('target_bo_bytes')!=target or data.get('job_bytes')!=49152:raise RuntimeError('actual BO extent changed')
 ro=0
 for c in check['cases']:
  code=sum((n+4095)&~4095 for n in check['programs'][c['program']]['code_bytes'])
  ro+=2*(2*12288+3*4096+code+49152-4-(44 if writes(c['program'])else 0))
 if data.get('readonly_byte_checks')!=ro:raise RuntimeError('incomplete RO guard coverage')
 return target

def validate(data,captured,check,nonce,uid):
 import numpy as np
 target=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact full storage readback inventory required')
 channels=guards=words=storageguards=maximum=0
 for case in check['cases']:
  view=check['views'][case['shape']];h,w,pitch=view['height'],view['width'],view['pitch']
  for repeat in range(2):
   base=(case['id']*2+repeat)*3;blob=captured[NAMES[base]]
   if type(blob)is not bytes or len(blob)!=target:raise RuntimeError('target full BO extent/type mismatch')
   raw=np.frombuffer(blob,dtype=np.uint8);rows=raw[4096:4096+pitch*h].reshape(h,pitch);got=rows[:,:w*4].reshape(h,w,4);want=pixels(case,view,repeat);delta=np.abs(got.astype(np.int16)-want.astype(np.int16));maximum=max(maximum,int(delta.max()))
   if np.any(delta[:,:,:3]>1)or np.any(delta[:,:,3]):raise RuntimeError(f'authored storage pixel mismatch case{case["id"]} repeat{repeat}')
   if any(np.any(r!=0xa5)for r in(raw[:4096],rows[:,w*4:].reshape(-1),raw[4096+pitch*h:])):raise RuntimeError('target prefix/row/suffix guard changed')
   channels+=h*w*4;guards+=target-h*w*4
   for stage in range(2):
    got=captured[NAMES[base+1+stage]]
    if type(got)is not bytes or len(got)!=12288 or got!=storage(case,stage,repeat):raise RuntimeError(f'full storage side effects/unchanged/guard mismatch case{case["id"]} stage{stage} repeat{repeat}')
    words+=case['bytes']//4;storageguards+=12288-case['bytes']
 if data.get('guard_byte_checks')!=guards:raise RuntimeError('device/fullraw target guards differ')
 return dict(passed=True,submissions=SUBMITS,finishes=SUBMITS,files=FILES,channel_checks=channels,storage_word_checks=words,storage_guard_byte_checks=storageguards,guard_byte_checks=guards,readonly_byte_checks=data['readonly_byte_checks'],maximum_channel_error=maximum,storage_exact=True,scope='Authored native direct32bit VS/FS storage read/write/length/sparse15; not public NVN/original source/game qualification')

def synthetic(check,nonce='a'*32,uid=12000):
 """Offline oracle negative fixture only; never packaged/uploaded to Android."""
 import numpy as np
 target=(max(v['required_span_bytes']for v in check['views'])+8192+4095)&~4095;captured={};guards=ro=0
 for c in check['cases']:
  v=check['views'][c['shape']]
  for repeat in range(2):
   raw=bytearray([0xa5])*target;image=pixels(c,v,repeat).tobytes()
   for y in range(v['height']):raw[4096+y*v['pitch']:4096+y*v['pitch']+v['width']*4]=image[y*v['width']*4:(y+1)*v['width']*4]
   base=(c['id']*2+repeat)*3;captured[NAMES[base]]=bytes(raw)
   for st in range(2):captured[NAMES[base+st+1]]=storage(c,st,repeat)
   guards+=target-v['width']*v['height']*4
   code=sum((n+4095)&~4095 for n in check['programs'][c['program']]['code_bytes']);ro+=2*12288+3*4096+code+49152-4-(44 if writes(c['program'])else 0)
 data=dict(schema=1,operation='native_graphics_storage',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,jobs=JOBS,programs=4,submissions=SUBMITS,finishes=SUBMITS,files=FILES,replays=JOBS,inflight_owner_checks=SUBMITS*10,unused_owner_checks=SUBMITS*4,guard_byte_checks=guards,readonly_byte_checks=ro,target_bo_bytes=target,storage_bo_bytes=12288,job_bytes=49152,resource_count=7,target_prefix=4096,storage_prefix=4096,sequence_offset=45056,sequence_bytes_exempt=4,same_job_replay=True,cpu_fixture_initialization=True,cpu_shader_execution=False,cpu_image_conversion=False,pixel_oracle_on_host=True,storage_oracle_on_host=True,nvn_api=False,game_modules_executed=False,passed=True,run_nonce=nonce,run_directory='pipeline-'+nonce)
 return data,captured
