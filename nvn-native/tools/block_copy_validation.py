"""Independent authored raw-block oracle; never runtime image conversion.
Inverse dword-sector order from public minigbm; see MINIGBM-LICENSE.
"""
import json,re
from functools import lru_cache
FORMATS=(1,5,6,7,8,9,0x25,0x26,0x27,0x28,0x38,0x9a,0x9c,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50)
NAMES=tuple(f'block-{n:03d}-r{repeat}.bin' for n in range(148) for repeat in range(2))
def up(v,a):return (v+a-1)//a*a
def parse_result(text):
 def pairs(items):
  out={}
  for k,v in items:
   if k in out:raise RuntimeError('duplicate block-copy JSON key '+k)
   out[k]=v
  return out
 try:r=json.loads(text,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
 except (ValueError,TypeError)as e:raise RuntimeError('invalid block-copy JSON')from e
 if type(r)is not dict:raise RuntimeError('block-copy result not object')
 return r

def spec(n):
 if type(n)is not int or not 0<=n<148:raise RuntimeError('invalid case identity')
 f=n//4;fmt=FORMATS[f];partial=n//2%2;direction=n%2
 cpp=1 if fmt==1 else 2 if 5<=fmt<=9 else 8 if 0x29<=fmt<=0x2d else 16 if 0x2e<=fmt<=0x30 else 8 if fmt in (0x41,0x42,0x45,0x46,0x49,0x4a) else 16 if 0x41<=fmt<=0x50 else 4
 block=4 if 0x41<=fmt<=0x50 else 1;width=block*13-(block-1);height=block*(8,11,17,128)[f%4]-(block-1);base=0;hblocks=(height+block-1)//block
 while base<4 and hblocks+hblocks//2>=16<<base:base+=1
 levels=[];offset=0
 for i in range(3):
  w=max(1,width>>i);h=max(1,height>>i);bx=(w+block-1)//block;by=(h+block-1)//block;rb=bx*cpp;pitch=up(rb,64);bh=base
  while bh and by<=8<<(bh-1):bh-=1
  size=pitch*up(by,8<<bh);levels.append([offset,size,w,h,bx,by,rb,pitch,bh]);offset+=size
 stride=up(offset,512<<levels[0][8]);payload=stride*3;level=1+f%2 if partial else 0;m=levels[level];x=1 if partial and m[4]>2 else 0;y=1 if partial and m[5]>2 else 0;rowbytes=(m[4]-(2 if x else 0))*cpp;rows=m[5]-(2 if y else 0);rowstride=rowbytes+partial;image=(rows-1)*rowstride+rowbytes+partial
 if partial:image|=1
 base_layer=partial;layers=2 if partial else 3;span=(layers-1)*image+(rows-1)*rowstride+rowbytes;words=(span+3)//4
 gx=min((words+63)//64,65535)if not direction else((x*cpp%4+rowbytes+3)//4+7)//8;gy=(words+gx*64-1)//(gx*64)if not direction else(rows+7)//8;gz=1 if not direction else layers
 linear_offset=512+payload+68 if partial else 68;linear_bytes=up(span,4);bo=[up((linear_offset+linear_bytes if partial else 512+payload)+4096,4096),0 if partial else up(linear_offset+linear_bytes+4096,4096)]
 return dict(id=n,format=fmt,width=width,height=height,block_w=block,block_h=block,bytes_per_block=cpp,payload_bytes=payload,layer_stride=stride,shared=partial,canonical_offset=512,linear_offset=linear_offset,bo_bytes=bo,region=[level,base_layer,layers,x*cpp,y,rowbytes,rows,rowstride,image,direction],args=[m[0]+stride*base_layer,stride,m[7]//64,m[8],x*cpp,y,rowbytes,rows,rowstride,image,layers,direction,span,words,gx*8 if not direction else 0,0],groups=[gx,gy,gz],levels=levels)

def typed_equal(actual,want):
 if type(actual)is not type(want):return False
 if type(want)is dict:return set(actual)==set(want)and all(typed_equal(actual[k],v)for k,v in want.items())
 if type(want)is list:return len(actual)==len(want)and all(typed_equal(a,b)for a,b in zip(actual,want))
 return actual==want

def validate_cases(cases):
 if type(cases)is not list or len(cases)!=148:raise RuntimeError('block case inventory')
 for n,c in enumerate(cases):
  if not typed_equal(c,spec(n)):raise RuntimeError('block geometry/source/region metadata mismatch '+str(n))
 return True

def pattern(offset,n,repeat,role=0):
 x=(offset*1664525+1013904223+(n+1)*2246822519+repeat*3266489917+role*668265263)&0xffffffff;x^=x>>13;x=x*1274126177&0xffffffff;x^=x>>16;return x&255

def canonical_offset(c,layer,x,y):
 level=c['region'][0];m=c['levels'][level];h=1<<m[8];bc=x//64;br=y//(8*h);xd=x%64//4;yl=y%(8*h)
 k=(yl//8)*128+(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3)
 return 512+layer*c['layer_stride']+m[0]+(br*(m[7]//64)+bc)*512*h+k*4+x%4

@lru_cache(maxsize=296)
def expected(n,repeat):
 c=spec(n);level,base,layers,xb,yb,rb,rows,rs,ims,direction=c['region'];data=[bytearray([0xa5])*size for size in c['bo_bytes']if size];linear=c['linear_offset'];srcindex=1 if direction and not c['shared']else 0;srcbase=linear if direction else 512;srcbytes=up(c['args'][12],4)if direction else c['payload_bytes']
 for i in range(srcbytes):data[srcindex][srcbase+i]=pattern(srcbase+i,n,repeat)
 destindex=0 if direction or c['shared']else 1;seen=set()
 for layer in range(layers):
  for y in range(rows):
   for x in range(rb):
    canonical=canonical_offset(c,base+layer,xb+x,yb+y);lb=linear+layer*ims+y*rs+x;to=canonical if direction else lb;source=lb if direction else canonical
    if to in seen or to>=len(data[destindex]):raise RuntimeError('oracle output overlap/bounds')
    seen.add(to);data[destindex][to]=pattern(source,n,repeat)
 return b''.join(data),len(seen),srcbytes

def validate(data,captured,nonce,uid,check):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('block UID/nonce invalid')
 validate_cases(check.get('cases'))
 exact=dict(schema=1,operation='block_copy',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submission='accepted',cases=148,formats=37,completed=296,submissions=296,finishes=296,replays=148,files=296,inflight_destroy_rejections=296,sequence_word_checks=296,nvn_api=False,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,bad_case=0xffffffff,bad_repeat=0xffffffff,bad_bo=0xffffffff,bad_offset=0xffffffffffffffff,run_nonce=nonce,run_directory='block-'+nonce)
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('block-copy result mismatch '+k)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('block-copy raw inventory')
 total=active=source=0
 for n in range(148):
  for repeat in range(2):
   name=NAMES[n*2+repeat];actual=captured[name];want,writes,reads=expected(n,repeat)
   if type(actual)is not bytes or len(actual)!=len(want):raise RuntimeError('block raw size/type '+name)
   if actual!=want:
    offset=next(i for i,(a,b)in enumerate(zip(actual,want))if a!=b);raise RuntimeError(f'{name} byte{offset}: {actual[offset]} != {want[offset]}')
   total+=len(actual);active+=writes;source+=reads
 codebytes=check.get('code_bo_bytes')
 if type(codebytes)is not int or not 0<codebytes<=65536 or codebytes%4096:raise RuntimeError('block actual code allocation profile')
 counts=dict(data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=source,aux_readonly_byte_checks=(codebytes+4096+8192-4)*296)
 for k,v in counts.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('block exact byte counter '+k)
 return dict(passed=True,formats=37,cases=148,submissions=296,finishes=296,replays=148,raw_files=296,byte_tolerance=0,**counts,scope='actual authored raw block GPU copying only; no NVN public-copy, BC sampling/decode, game or display claim')
