"""Independent authored true3D raw-block oracle, never a runtime conversion.
Sector inverse from public minigbm addressing, Z-slab mathematics from published
Generic16Bx2. Queries use actual 3D depth, not array-layer substitution.
"""
import re
from functools import lru_cache
from block_copy_validation import FORMATS,pattern,parse_result,typed_equal,up
NAMES=tuple(f'public-volume-copy-{n:03d}-r{k}.bin'for n in range(228)for k in range(2))
def spec(n):
 if type(n)is not int or not 0<=n<228:raise RuntimeError('public volume case identity')
 f=n//6;v=n//3%2;family=n%3;fmt=FORMATS[f]if f<37 else 0x42
 flags=4 if v and fmt in (0x25,0x29) else 0
 cpp=1 if fmt==1 else 2 if 5<=fmt<=9 else 8 if 0x29<=fmt<=0x2d else 16 if 0x2e<=fmt<=0x30 else 8 if fmt in(0x41,0x42,0x45,0x46,0x49,0x4a)else 16 if 0x41<=fmt<=0x50 else 4
 block=4 if 0x41<=fmt<=0x50 else 1;w,h,d,levels=(35,19,9,3)if f<37 else(32,32,32,1);base=0
 while base<4 and d+d//2>=2<<base:base+=1
 mips=[];depths=[];bds=[];offset=0
 for i in range(levels):
  wi,hi,di=max(1,w>>i),max(1,h>>i),max(1,d>>i);bx,by=(wi+block-1)//block,(hi+block-1)//block;rb=bx*cpp;pitch=up(rb,64);bd=base
  while bd and di<=1<<(bd-1):bd-=1
  size=pitch*up(by,8)*up(di,1<<bd);mips.append([offset,size,wi,hi,bx,by,rb,pitch,0]);depths.append(di);bds.append(bd);offset+=size
 level=1 if v and f<37 else 0;m=mips[level];xb=yb=bz=v;rb=(m[4]-xb)*cpp;rows=m[5]-yb;slices=depths[level]-bz;row=rb+v;image=(rows-1)*row+rb
 if v:image=(image+1)|1
 span=(slices-1)*image+(rows-1)*row+rb;linear=512+offset+68 if v else 68;dest=up(512+offset+68,512)if v else 1024;lb=up(span,4)
 bytes0=up((dest+offset if family==2 else linear+lb)+4096,4096)if v else up(512+offset+4096,4096);bytes1=0 if v else up((dest+offset if family==2 else linear+lb)+4096,4096)
 xo,yo=xb*block,yb*block;rw,rh=m[2]-xo,m[3]-yo
 return dict(id=n,family=family,variant=v,target=2,format=fmt,width=w,height=h,depth=d,layers=1,levels=levels,flags=flags,shared=v,payload_bytes=offset,layer_stride=offset,base_bh=0,base_bd=base,canonical_offset=512,destination_offset=dest,linear_offset=linear,linear_bytes=lb,pool_bytes=[bytes0,bytes1],level=level,base_layer=bz,layer_count=slices,x_bytes=xb*cpp,y_rows=yb,row_bytes=rb,rows=rows,row_stride=row,image_stride=image,region=[[xo,yo,bz,rw,rh,slices],[xo,yo,0,rw,rh,slices]],mips=mips,mip_depths=depths,mip_bds=bds)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=228:raise RuntimeError('public volume case inventory')
 for n,c in enumerate(cases):
  if not typed_equal(c,spec(n)):raise RuntimeError('public volume actual metadata mismatch '+str(n))
 return True
def coordinate(c,z,x,y):
 m=c['mips'][c['level']];d=1<<c['mip_bds'][c['level']];xd=x%64//4;yl=y%8;k=(xd&8)*8+((yl&6)<<3)+(xd&4)*2+((yl&1)<<2)+(xd&3)
 return m[0]+(((z//d)*((m[5]+7)//8)+y//8)*(m[7]//64)+x//64)*512*d+(z%d)*512+k*4+x%4
@lru_cache(maxsize=456)
def expected(n,repeat):
 c=spec(n);data=[bytearray([0xa5])*size for size in c['pool_bytes']if size];family=c['family'];source_pool=1 if family==0 and not c['shared']else 0;source_offset=c['linear_offset']if family==0 else 512;source_bytes=c['linear_bytes']if family==0 else c['payload_bytes']
 for i in range(source_bytes):data[source_pool][source_offset+i]=pattern(source_offset+i,n,repeat)
 dest_pool=0 if family==0 or c['shared']else 1;seen=set()
 for z in range(c['layer_count']):
  for y in range(c['rows']):
   for x in range(c['row_bytes']):
    canonical=512+coordinate(c,c['base_layer']+z,c['x_bytes']+x,c['y_rows']+y);linear=c['linear_offset']+z*c['image_stride']+y*c['row_stride']+x
    to=canonical if family==0 else linear if family==1 else c['destination_offset']+coordinate(c,z,c['x_bytes']+x,c['y_rows']+y);source=linear if family==0 else canonical
    if to in seen or to>=len(data[dest_pool]):raise RuntimeError('public volume oracle overlap/bounds')
    seen.add(to);data[dest_pool][to]=pattern(source,n,repeat)
 return b''.join(data),len(seen),source_bytes

def validate(data,captured,nonce,uid,check,*,call_commands=False):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('public volume UID/nonce')
 validate_cases(check.get('cases'));exact=dict(schema=1,operation='public_volume_copy',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=228,formats=37,families=3,completed=456,submissions=456,finishes=456,replays=228,files=456,CopyCommands_calls=228,recorded_texture_finalize_rejections=304,pool_imports=570,pool_releases=570,texture_initializations=304,texture_finalizations=304,buffer_initializations=456,bad_case=0xffffffff,bad_replay=0xffffffff,bad_pool=0xffffffff,bad_offset=0xffffffffffffffff,nvn_api=True,probe_raw_submit_calls=0,probe_private_encode_calls=0,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,run_nonce=nonce,run_directory='public-volume-copy-'+nonce)
 if type(call_commands)is not bool:raise RuntimeError('public volume Call profile must be boolean')
 if call_commands:exact.update(CopyCommands_calls=0,CallCommands_calls=228,call_commands=True)
 elif not typed_equal(data.get('CallCommands_calls',0),0) or data.get('call_commands',False)is not False:raise RuntimeError('unexpected Call copy result in ordinary CopyCommands profile')
 for k,v in exact.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('public volume result mismatch '+k)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('public volume raw inventory')
 total=active=reads=0
 for n in range(228):
  for repeat in range(2):
   name=NAMES[n*2+repeat];want,writes,source=expected(n,repeat);actual=captured[name]
   if type(actual)is not bytes or len(actual)!=len(want):raise RuntimeError('public volume raw size '+name)
   if actual!=want:
    offset=next(i for i,(a,b)in enumerate(zip(actual,want))if a!=b);raise RuntimeError(f'{name} byte{offset}: {actual[offset]} != {want[offset]}')
   total+=len(actual);active+=writes;reads+=source
 counts=dict(data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,command_control_readonly_byte_checks=131072*456)
 for k,v in counts.items():
  if not typed_equal(data.get(k),v):raise RuntimeError('public volume byte count '+k)
 return dict(passed=True,cases=228,formats=37,families=3,true3D=True,image_permission_cases=sum(spec(n)['flags']==4 for n in range(228)),submissions=456,finishes=456,replays=228,raw_files=456,byte_tolerance=0,**counts,scope='Authored public true3D B2T/T2B/T2T and CopyCommands, actual builder BD/mips/Z; no sampling/decode/game/display or 3D Texels claim')
