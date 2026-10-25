"""Experimental writer profile oracle. No NVN normative identity claim/codec."""
from functools import lru_cache
from public_copy_validation import up,typed_equal,expected_case,validate_profile,parse_result
NAMES=tuple(f'public-copy-{n:03d}-r{repeat}.bin'for n in range(72)for repeat in range(2))
def spec(n):
 if type(n)is not int or not 0<=n<72:raise RuntimeError('experimental package case identity')
 fmt=0x44 if n//36 else 0x25;bh=n//6%6;variant=n//3%2;family=n%3
 w=73 if variant else 128;h=(651 if fmt==0x44 else 163)if variant else 128;target=4 if variant else 1;layers=3 if variant else 1;levels=4 if variant else 8;block=4 if fmt==0x44 else 1;cpp=16 if fmt==0x44 else 4
 mips=[];offset=0
 for level in range(levels):
  mw=max(1,w>>level);mh=max(1,h>>level);bx=(mw+block-1)//block;by=(mh+block-1)//block;row=bx*cpp;pitch=up(row,64);effective=bh
  while effective and by<=8<<(effective-1):effective-=1
  size=pitch*up(by,8<<effective);mips.append([offset,size,mw,mh,bx,by,row,pitch,effective]);offset+=size
 stride=up(offset,512<<mips[0][8])if target==4 else offset;payload=stride*layers;level=0 if variant else bh%2;base=1 if variant else 0;count=2 if variant else 1;m=mips[level]
 xb=cpp if variant else 0;yb=1 if variant else 0;rb=m[6]-(2*cpp if variant else 0);rows=m[5]-(2 if variant else 0);rs=rb+variant;ims=((rows-1)*rs+rb+variant)|variant;span=(count-1)*ims+(rows-1)*rs+rb;lb=up(span,4)
 xo=xb//cpp*block;yo=yb*block;rw=min(rb//cpp*block,m[2]-xo);rh=min(rows*block,m[3]-yo);dest=up(512+payload+68,512)if variant else 1024;linear=512+payload+68 if variant else 68;bo0=up((dest+payload if family==2 else linear+lb)+4096,4096)if variant else up(512+payload+4096,4096);bo1=0 if variant else up((dest+payload if family==2 else linear+lb)+4096,4096)
 return dict(id=n,family=family,variant=variant,target=target,format=fmt,width=w,height=h,layers=layers,levels=levels,flags=0,shared=variant,payload_bytes=payload,layer_stride=stride,base_bh=bh,canonical_offset=512,destination_offset=dest,linear_offset=linear,linear_bytes=lb,pool_bytes=[bo0,bo1],level=level,base_layer=base,layer_count=count,x_bytes=xb,y_rows=yb,row_bytes=rb,rows=rows,row_stride=rs,image_stride=ims,region=[[xo,yo,0,rw,rh,count],[0,0,0,rw,rh,count]],mips=mips)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=72:raise RuntimeError('package case inventory')
 for i,c in enumerate(cases):
  if not typed_equal(c,spec(i)):raise RuntimeError('package geometry '+str(i))
 return True
@lru_cache(maxsize=144)
def expected(n,repeat):return expected_case(spec(n),n,repeat)
def validate(data,captured,nonce,uid,check):
 value=validate_profile(data,captured,nonce,uid,check,72,2,'public_packaged_bntx_copy',validate_cases,expected,NAMES,dict(packaged_experiment=True,NVN_encoding_normative_proof=False))
 value['scope']='ordinary UID experimental BNTX-writer8B→canonical Generic16Bx2 public Copy interoperability only; not normative NVN encoding, sampling, game frame or display'
 return value
