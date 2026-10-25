"""Independent original shader math + inverse GOB sectors for NVN bridge raw files.

Read-only oracle only. Does not produce native images or runtime texels.
"""
import math
import re
import struct
NAMES=tuple('bridge-%d.bin'%n for n in range(4))

def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def quantize(v):return min(255,max(0,math.floor(v*255+.5)))
def pixel(n,x,y):
 clear=(17,29,43,59)
 if 4<=x<15 and 5<=y<22:return (0,0,255,255),0
 # Source VS affine map and constant per-vertex test colors, derived from the
 # original Maxwell expression, independent of IR3/shadow/private GPU buffers.
 u=(x+.5)/64;v=(y+.5)/64
 if n==3:
  if u+v/2>1:return clear,0
  u*=2
 if n==2:u=1-u
 tx=min(1,max(0,math.floor(u*2)));ty=min(1,max(0,math.floor(v*2)))
 texels=(((64,128,192,255),(192,64,128,128),(128,192,64,64),(96,160,224,0)),
         ((192,128,64,255),(128,192,64,192),(64,96,160,128),(224,160,96,0)))
 sample=[f32(t/255) for t in texels[n in (1,2)][ty*2+tx]]
 color=(.5,.75,1,.5) if n==2 else (.75,.5,1,.75) if n==3 else (1,1,1,1)
 alpha=f32(sample[3]*color[3])
 if alpha==0:return clear,0
 mix=.5 if n==2 else .25 if n==3 else 0
 gamma=2 if n==3 else 1;contrast=.5 if n in (2,3) else 1;target=(.25,.5,.75,.875);out=[]
 for c in range(3):
  encoded=f32(abs(f32(sample[c]*color[c]))**f32(1/gamma))
  corrected=f32(f32(encoded-.5)*contrast+.5);linear=f32(min(1,max(0,corrected))**gamma)
  out.append(quantize(f32(f32(target[c]-linear)*mix+linear)))
 out.append(quantize(f32(f32(target[3]-alpha)*mix+alpha)))
 return tuple(out),1

def sectors():
 # Inverse minigbm sector decomposition, rather than the shader's direct
 # x/y-to-GOB equation. A64x64 RGBA payload uses four x-GOBs and height8GOBs.
 seen=set()
 for gx in range(4):
  for word in range(128*8):
   for lane in range(4):
    xb=gx*64+(((word>>3)&8)|((word>>1)&4)|(word&3))*4+lane
    y=(word>>7)*8+((word>>3)&6)+((word>>2)&1)
    offset=gx*4096+word*4+lane
    if (xb,y) in seen:raise AssertionError('inverse not injective')
    seen.add((xb,y));yield offset,xb//4,y,xb%4
 if len(seen)!=16384:raise AssertionError('incomplete GOB oracle')

def validate(data,captured,nonce,uid):
 if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('invalid nonce')
 if type(uid) is not int or uid<10000 or type(data) is not dict:raise RuntimeError('ordinary UID/type')
 required={'schema':1,'uid':uid,'chip_id':0x06050002,'errno':0,'cleanup_errno':0,'completed_cases':4,'submits':4,'finishes':4,'raw_files':4,'channel_checks':65536,'alias_channel_checks':65536,'target_offset':4096,'readback_offset':32768,'target_payload_bytes':16384,'raw_bytes':65536}
 for k,v in required.items():
  if type(data.get(k)) is not int or data[k]!=v:raise RuntimeError('bridge metadata '+k)
 for k,v in {'passed':True,'resources_retained':False,'cpu_image_conversion':False,'original_game_execution':False}.items():
  if type(data.get(k)) is not bool or data[k] is not v:raise RuntimeError('bridge lifecycle '+k)
 if data.get('operation')!='nvn_texture_bridge' or data.get('stage')!='complete':raise RuntimeError('bridge operation/stage')
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='texture-bridge-'+nonce:raise RuntimeError('bridge nonce namespace')
 if type(data.get('guard_byte_checks')) is not int or data['guard_byte_checks']!=131072 or type(data.get('readonly_byte_checks')) is not int or data['readonly_byte_checks']<=0:raise RuntimeError('missing bridge guards/readonly')
 if type(captured) is not dict or set(captured)!=set(NAMES):raise RuntimeError('bridge exact file inventory')
 channels=guards=aliases=maximum=0;hashes=[]
 for n,name in enumerate(NAMES):
  raw=captured[name]
  if type(raw) is not bytes or len(raw)!=65536:raise RuntimeError('bridge raw type/size')
  active=set()
  for offset,x,y,c in sectors():
   expected,tolerance=pixel(n,x,y);a=raw[4096+offset];delta=abs(a-expected[c]);maximum=max(maximum,delta)
   if delta>tolerance:raise RuntimeError(f'{name}: ({x},{y})[{c}] actual{a} expected{expected[c]}')
   if raw[32768+offset]!=a:raise RuntimeError('canonical Buffer alias mismatch')
   channels+=1;aliases+=1;active.add(4096+offset);active.add(32768+offset)
  for offset,a in enumerate(raw):
   if offset not in active:
    if a!=0xa5:raise RuntimeError(f'{name}: guard{offset}')
    guards+=1
  hashes.append(raw[4096:4096+16384])
 if len(set(hashes))!=4:raise RuntimeError('source/UBO/vertex replay not observably distinct')
 return dict(passed=True,cases=4,channel_checks=channels,alias_checks=aliases,guard_checks=guards,max_channel_error=maximum,partial_clear_pixels=11*17*4,scope='actual NVN GOB register/sample/RT clear/draw/writeback/Buffer alias,64RT+2x2source; no game/fullsize/CPU_NO_ACCESS qualification')
