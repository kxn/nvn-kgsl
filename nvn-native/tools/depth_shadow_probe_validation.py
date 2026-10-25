"""HOST-only independent Generic16Bx2 and scalar depth/triangle oracle.
The GOB DWORD sector permutation reuses the existing public-minigbm inverse
address proof used by block_copy_validation; no GPU output supplies expected data.
"""
import math,re,struct
from functools import lru_cache
NAMES=tuple(name for case in range(54)for repeat in range(2)for name in(('depth-shadow-gob-%03u.raw'%(2*case+repeat),)+(()if case%3==2 else('depth-shadow-color-%03u.raw'%(2*case+repeat),))))
def up(v,a):return(v+a-1)//a*a
def spec(n):
 if type(n)is not int or not 0<=n<54:raise ValueError('case identity')
 fmt=50+n//18;profile=n//9%2;selected=n//3%3;role=n%3;width,height=(127,65)if profile else(73,41);cpp=2 if fmt==50 else 4
 bh=0
 while bh<4 and height+height//2>=16<<bh:bh+=1
 levels=[];offset=0
 for l in range(3):
  w=max(1,width>>l);h=max(1,height>>l);pitch=up(w*cpp,64);b=bh
  while b and h<=8<<(b-1):b-=1
  size=pitch*up(h,8<<b);levels.append(dict(offset=offset,bytes=size,width=w,height=h,pitch=pitch,bh=b));offset+=size
 stride=up(offset,512<<bh)if profile else offset;layers=3 if profile else 1;payload=stride*layers
 return dict(case=n,format=fmt,profile=profile,selected=selected,role=role,origin=(role+selected)&1,level=selected,layer=selected if profile else 0,compare=5 if role==1 else 4 if role==2 else 2,width=levels[selected]['width'],height=levels[selected]['height'],cpp=cpp,layers=layers,payload=payload,layer_stride=stride,canonical_bytes=up(payload+8192,4096),levels=levels)
def equal(a,b):
 if type(a)is not type(b):return False
 if type(a)is dict:return set(a)==set(b)and all(equal(a[k],v)for k,v in b.items())
 if type(a)is list:return len(a)==len(b)and all(equal(x,y)for x,y in zip(a,b))
 return a==b
def source_metadata(rows):
 if type(rows)is not list or len(rows)!=54:raise ValueError('full54 source/FDL shapes required')
 for n,row in enumerate(rows):
  s=spec(n)
  if any(not equal(row.get(k),v)for k,v in s.items()):raise ValueError('independent source layout mismatch '+str(n))
  if row['role']==2:
   if row.get('color_pitch')!=0 or row.get('color_bytes')!=0:raise ValueError('zero COLOR has attachment')
  elif type(row.get('color_pitch'))is not int or row['color_pitch']<s['width']*4 or row.get('color_bytes')!=up(8192+row['color_pitch']*s['height'],4096):raise ValueError('actual LINEAR target shape')
 return True
def address(c,level,layer,xbyte,y):
 m=c['levels'][level];h=1<<m['bh'];bc=xbyte//64;br=y//(8*h);xd=xbyte%64//4;yl=y%(8*h)
 k=(yl//8)*128+(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3)
 return 4096+layer*c['layer_stride']+m['offset']+(br*(m['pitch']//64)+bc)*512*h+k*4+xbyte%4
def input_word(c,x,y,level,layer,repeat):
 bank=(x+2*y+level+3*layer+repeat)&3
 values=((8192,24576,40959,57343),(2097152,6291456,10485759,14680063),(0x3e000000,0x3ec00000,0x3f200000,0x3f600000));v=values[c['format']-50][bank]
 return v|((x+3*y+layer+level+repeat)&255)<<24 if c['format']==51 else v
def unpack(c,word):return struct.unpack('<f',struct.pack('<I',word))[0]if c['format']==52 else(word&0xffffff)/16777215 if c['format']==51 else word/65535
def accepts(compare,z,old):return z<old if compare==2 else z>old if compare==5 else z<=old
def depth_word(c,z):
 return struct.pack('<f',z)if c['format']==52 else struct.pack('<H',math.floor(z*65535+.5))if c['format']==50 else struct.pack('<I',math.floor(z*16777215+.5))
def expected_input(c,repeat):
 n=c['case'];raw=bytearray(((at*13+n*23+repeat*17)^0xa5)&255 for at in range(c['canonical_bytes']))
 for l,m in enumerate(c['levels']):
  for layer in range(c['layers']):
   for y in range(m['height']):
    for x in range(m['width']):
     word=input_word(c,x,y,l,layer,repeat)
     for b in range(c['cpp']):raw[address(c,l,layer,x*c['cpp']+b,y)]=(word>>(8*b))&255
 return bytes(raw)
@lru_cache(maxsize=108)
def expected(n,repeat):
 import numpy as np
 c=spec(n);raw=bytearray(expected_input(c,repeat));defined=bytearray(b'\1')*len(raw);written=[];z=.25 if repeat else.5;cpp=c['cpp'];l=c['level'];layer=c['layer'];w=c['width'];h=c['height']
 for y in range(h):
  for x in range(w):
   passed=3<=x<w-4 and 2<=y<h-3 and accepts(c['compare'],z,unpack(c,input_word(c,x,y,l,layer,repeat)))
   if passed:
    word=depth_word(c,z)
    for b in range(3 if c['format']==51 else cpp):raw[address(c,l,layer,x*cpp+b,y)]=word[b]
    written.append((x,y))
   if c['format']==51:defined[address(c,l,layer,x*cpp+3,y)]=0
 return bytes(raw),bytes(defined),written

def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID/nonce')
 source_metadata(check['cases']);want=dict(schema=1,operation='native_canonical_depth',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=324,programs=54,submissions=648,finishes=108,files=180,completed_cases=108,replays=54,true_zero_color_cases=18,pixel_checks=0)
 if any(not equal(data.get(k),v)for k,v in want.items()):raise RuntimeError('depth chain completion')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,canonical_fixture_input_only=True,private_depth_cpu_access=False,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True,stencil=False).items():
  if not equal(data.get(k),v):raise RuntimeError('depth chain domain '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('whole canonical/color raw inventory')
 if type(data.get('cases'))is not list or len(data['cases'])!=54:raise RuntimeError('complete54 shapes/roles')
 guards=readonly=busy=total=pixels=channels=undefined=0;maximum=0
 for n,(actual,c)in enumerate(zip(data['cases'],check['cases'])):
  fields={k:c[k]for k in('case','format','profile','selected','role','origin','level','layer','compare','width','height','canonical_bytes','color_pitch','color_bytes')};fields.update(refs=17 if c['role']==2 else 18,readonly_bytes=check['readonly_bytes_per_chain'])
  if not equal(actual,fields):raise RuntimeError('actual view/ledger metadata '+str(n))
  w,h=c['width'],c['height'];busy+=12*(fields['refs']+3)
  for repeat in range(2):
   number=n*2+repeat;blob=captured['depth-shadow-gob-%03u.raw'%number];want,defined,written=expected(n,repeat)
   if type(blob)is not bytes or len(blob)!=len(want):raise RuntimeError('full canonical size')
   av=np.frombuffer(blob,dtype=np.uint8);ev=np.frombuffer(want,dtype=np.uint8);mask=np.frombuffer(defined,dtype=np.uint8).astype(bool);allowed=np.zeros(len(blob),dtype=bool)
   # UNORM export may differ one depth quantization unit. All non-written,
   # unselected mips/layers, GOB padding and guards are otherwise exact.
   for x,y in written:
    offsets=[address(c,c['level'],c['layer'],x*c['cpp']+b,y)for b in range(c['cpp'])];value=bytes(blob[k]for k in offsets);expected_value=depth_word(c,.25 if repeat else.5)
    if c['format']==52:
     if value!=expected_value:raise RuntimeError('D32F depth export')
    else:
     v=int.from_bytes(value,'little')& (0xffffff if c['format']==51 else 0xffff);vexp=int.from_bytes(expected_value,'little');error=abs(v-vexp);maximum=max(maximum,error)
     if error>1:raise RuntimeError('D16/D24 depth export')
     for at in offsets:allowed[at]=True
   if np.any(av[mask&~allowed]!=ev[mask&~allowed]):raise RuntimeError('canonical upload/store/untouched data '+str(number))
   total+=len(blob);pixels+=w*h;undefined+=int((~mask).sum());guards+=len(blob)-c['payload'];readonly+=fields['readonly_bytes']
   if c['role']==2:continue
   blob=captured['depth-shadow-color-%03u.raw'%number]
   if type(blob)is not bytes or len(blob)!=c['color_bytes']:raise RuntimeError('complete color size')
   wanted=np.full(len(blob),0xa5,dtype=np.uint8);y,x=np.mgrid[:h,:w];u=(x+.5)/w;v=(y+.5)/h
   if c['origin']:v=1-v
   rgba=np.floor(np.clip(np.stack((1-(u+v)*.5,u*.5,v*.5,np.ones_like(u)),axis=-1),0,1)*255+.5).astype(np.uint8);active=np.empty((h,w,4),dtype=np.uint8);active[:]=(17,29,43,59)
   if c['role']==0:
    for xx,yy in written:active[yy,xx]=rgba[yy,xx]
   for row in range(h):wanted[4096+row*c['color_pitch']:4096+row*c['color_pitch']+w*4]=active[row].ravel()
   av=np.frombuffer(blob,dtype=np.uint8);delta=np.abs(av.astype(np.int16)-wanted.astype(np.int16));mask=np.zeros(len(blob),dtype=bool)
   for row in range(h):mask[4096+row*c['color_pitch']:4096+row*c['color_pitch']+w*4]=True
   if np.any(delta[~mask])or np.any(delta[mask]>1):raise RuntimeError('depth comparison/color/write role '+str(number))
   total+=len(blob);guards+=len(blob)-w*h*4;channels+=w*h*4
 if not equal(data.get('guard_byte_checks'),guards)or not equal(data.get('readonly_byte_checks'),readonly)or not equal(data.get('inflight_owner_checks'),busy):raise RuntimeError('complete guard/readonly/owner ledger')
 return dict(passed=True,submissions=648,finishes=108,files=180,full_raw_bytes=total,depth_pixels=pixels,color_channels=channels,max_unorm_depth_error=maximum,guard_byte_checks=guards,readonly_byte_checks=readonly,inflight_owner_checks=busy,undefined_selected_d24x8_bytes=undefined,nvn_api=False,game_modules_executed=False,scope='actual GPU canonical GOB→raw CS→A2D UPLOAD→depth Draw→A2D READBACK→raw CS→canonical; independent authored scalar/GOB oracle; no public NVN/game')
