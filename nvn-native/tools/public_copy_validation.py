"""Independent public NVN copy test oracle, no runtime image conversion.
Reuses the authored native oracle's public inverse-sector math family, not GPU
readback as expected bytes. No BC decoding/texture sampling is performed.
"""
import json,re
from functools import lru_cache
from block_copy_validation import FORMATS,pattern,parse_result,typed_equal,up,spec as native_spec
NAMES=tuple(f'public-copy-{n:03d}-r{repeat}.bin'for n in range(234)for repeat in range(2))
def spec(n):
 if type(n)is not int or not 0<=n<234:raise RuntimeError('public copy case identity')
 f=n//6;variant=n//3%2;family=n%3
 if f<37:
  b=native_spec(f*4+variant*2);target=4;fmt=b['format'];w=b['width'];h=b['height'];layers=levels=3;payload=b['payload_bytes'];stride=b['layer_stride'];mips=b['levels'];level,base,count,xb,yb,rb,rows,rs,ims,_=b['region'];block=b['block_w'];cpp=b['bytes_per_block'];base_bh=mips[0][8]
 else:
  target=1;fmt=0x42 if f==37 else 1;w=h=64 if f==37 else 0
  if f==38:w,h=31,17
  block=4 if f==37 else 1;cpp=8 if f==37 else 1;layers=levels=1;bx=(w+block-1)//block;by=(h+block-1)//block;rb0=bx*cpp;pitch=up(rb0,64);base_bh=0
  while base_bh<4 and by+by//2>=16<<base_bh:base_bh+=1
  bh=base_bh
  while bh and by<=8<<(bh-1):bh-=1
  payload=stride=pitch*up(by,8<<bh);mips=[[0,payload,w,h,bx,by,rb0,pitch,bh]];level=base=0;count=1;xb=cpp if variant else 0;yb=1 if variant else 0;rb=rb0-(2*cpp if variant else 0);rows=by-(2 if variant else 0);rs=rb+variant;ims=((rows-1)*rs+rb+variant)|variant
 m=mips[level];xo=xb//cpp*block;yo=yb*block;rw=min(rb//cpp*block,m[2]-xo);rh=min(rows*block,m[3]-yo);destination=up(512+payload+68,512)if variant else 1024;linear=512+payload+68 if variant else 68;span=(count-1)*ims+(rows-1)*rs+rb;lb=up(span,4)
 bo0=up((destination+payload if family==2 else linear+lb)+4096,4096)if variant else up(512+payload+4096,4096);bo1=0 if variant else up((destination+payload if family==2 else linear+lb)+4096,4096)
 return dict(id=n,family=family,variant=variant,target=target,format=fmt,width=w,height=h,layers=layers,levels=levels,flags=8 if variant else 0,shared=variant,payload_bytes=payload,layer_stride=stride,base_bh=base_bh,canonical_offset=512,destination_offset=destination,linear_offset=linear,linear_bytes=lb,pool_bytes=[bo0,bo1],level=level,base_layer=base,layer_count=count,x_bytes=xb,y_rows=yb,row_bytes=rb,rows=rows,row_stride=rs,image_stride=ims,region=[[xo,yo,0,rw,rh,count],[0,0,0,rw,rh,count]],mips=mips)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=234:raise RuntimeError('public copy case inventory')
 for n,c in enumerate(cases):
  if not typed_equal(c,spec(n)):raise RuntimeError('public copy geometry/region metadata '+str(n))
 return True

def coordinate(c,layer,x,y):
 m=c['mips'][c['level']];bh=1<<m[8];bc=x//64;br=y//(8*bh);xd=x%64//4;yl=y%(8*bh);k=(yl//8)*128+(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3)
 return layer*c['layer_stride']+m[0]+(br*(m[7]//64)+bc)*512*bh+k*4+x%4
def expected_case(c,n,repeat):
 data=[bytearray([0xa5])*size for size in c['pool_bytes']if size];family=c['family'];source_pool=1 if family==0 and not c['shared']else 0;source_offset=c['linear_offset']if family==0 else 512;source_bytes=c['linear_bytes']if family==0 else c['payload_bytes']
 for i in range(source_bytes):data[source_pool][source_offset+i]=pattern(source_offset+i,n,repeat)
 dest_pool=0 if family==0 or c['shared']else 1;seen=set()
 for layer in range(c['layer_count']):
  for y in range(c['rows']):
   for x in range(c['row_bytes']):
    canonical=512+coordinate(c,c['base_layer']+layer,c['x_bytes']+x,c['y_rows']+y);linear=c['linear_offset']+layer*c['image_stride']+y*c['row_stride']+x
    to=canonical if family==0 else linear if family==1 else c['destination_offset']+coordinate(c,layer,x,y);source=linear if family==0 else canonical
    if to in seen or to>=len(data[dest_pool]):raise RuntimeError('public copy oracle output overlap/bounds')
    seen.add(to);data[dest_pool][to]=pattern(source,n,repeat)
 return b''.join(data),len(seen),source_bytes

@lru_cache(maxsize=468)
def expected(n,repeat):return expected_case(spec(n),n,repeat)

def validate(data,captured,nonce,uid,check,*,call_commands=False):
 return validate_profile(data,captured,nonce,uid,check,234,37,'public_canonical_copy',validate_cases,expected,NAMES,call_commands=call_commands)
def validate_profile(data,captured,nonce,uid,check,count,formats,operation,inventory,expected_bytes,names,extra=None,*,call_commands=False):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('public copy UID/nonce')
 inventory(check.get('cases'));exact=dict(schema=1,operation=operation,uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=count,formats=formats,families=3,completed=count*2,submissions=count*2,finishes=count*2,replays=count,files=count*2,CopyCommands_calls=count,recorded_texture_finalize_rejections=count//3*4,pool_imports=count//2*5,pool_releases=count//2*5,texture_initializations=count//3*4,texture_finalizations=count//3*4,buffer_initializations=count*2,bad_case=0xffffffff,bad_replay=0xffffffff,bad_pool=0xffffffff,bad_offset=0xffffffffffffffff,nvn_api=True,probe_raw_submit_calls=0,probe_private_encode_calls=0,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,run_nonce=nonce,run_directory='public-copy-'+nonce)
 if type(call_commands)is not bool:raise RuntimeError('public copy Call profile must be boolean')
 if call_commands:exact.update(CopyCommands_calls=0,CallCommands_calls=count,call_commands=True)
 elif not typed_equal(data.get('CallCommands_calls',0),0) or data.get('call_commands',False)is not False:raise RuntimeError('unexpected Call copy result in ordinary CopyCommands profile')
 if extra:exact.update(extra)
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('public copy result mismatch '+k)
 if type(captured)is not dict or set(captured)!=set(names):raise RuntimeError('public copy raw inventory')
 total=active=reads=0
 for n in range(count):
  for repeat in range(2):
   name=names[n*2+repeat];want,writes,source=expected_bytes(n,repeat);actual=captured[name]
   if type(actual)is not bytes or len(actual)!=len(want):raise RuntimeError('public copy raw size '+name)
   if actual!=want:
    offset=next(i for i,(a,b)in enumerate(zip(actual,want))if a!=b);raise RuntimeError(f'{name} byte{offset}: {actual[offset]} != {want[offset]}')
   total+=len(actual);active+=writes;reads+=source
 counts=dict(data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,command_control_readonly_byte_checks=131072*count*2)
 for k,v in counts.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('public copy exact byte count '+k)
 return dict(passed=True,public_NVNCopy_families=['CopyBufferToTexture','CopyTextureToBuffer','CopyTextureToTexture'],cases=count,formats=formats,submissions=count*2,finishes=count*2,replays=count,raw_files=count*2,byte_tolerance=0,**counts,scope='authored actual public NVN copy/CopyCommands/imported caller storage only; no BC sampling/decode, Draw, game or display claim')
