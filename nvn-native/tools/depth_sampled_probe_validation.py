"""Independent numeric depth filtering oracle; no CPU runtime conversion."""
import numpy as np
NAMES=tuple('depth-sampled-%03d.raw'%i for i in range(60))
def case(index):
 t=index//2;f=50+t//10;r=t//2%5;s=t%2
 return dict(case=t,format=f,role=r,shape=s,level=s,layer=s,source_width=36 if s else 17,source_height=20 if s else 11,width=41 if s else 33,height=23 if s else 19,origin=s)
def scalar(f,x,y,level,layer,repeat):
 b=(x+2*y+level+3*layer+repeat)%4
 if f==50:return np.array([8192,24576,40959,57343],dtype=np.float64)[b]/65535.
 if f==51:return np.array([2097152,6291456,10485759,14680063],dtype=np.float64)[b]/16777215.
 return np.array([.125,.375,.625,.875])[b]
def pixels(c,repeat):
 h,w=c['height'],c['width'];sh,sw=c['source_height'],c['source_width'];y,x=np.mgrid[:h,:w];u=(x+.5)/w;v=(y+.5)/h
 if c['origin']:v=1-v
 f=lambda a,b:scalar(c['format'],np.clip(a,0,sw-1),np.clip(b,0,sh-1),c['level'],c['layer'],repeat)
 r=c['role']
 if r in(0,2):d=f(np.floor(u*sw).astype(int),np.floor(v*sh).astype(int));out=np.stack((d,d*.5,d*.25,np.ones_like(d)),axis=-1)
 elif r==1:
  fx=(x+.5)/2-.5;fy=((h-y-.5)if c['origin']else(y+.5))/2-.5;ix=np.floor(fx).astype(int);iy=np.floor(fy).astype(int);a=fx-ix;b=fy-iy
  d=(1-a)*(1-b)*f(ix,iy)+a*(1-b)*f(ix+1,iy)+(1-a)*b*f(ix,iy+1)+a*b*f(ix+1,iy+1);out=np.stack((d,d*.5,d*.25,np.ones_like(d)),axis=-1)
 elif r==3:out=np.stack((np.full_like(u,sw/255.),np.full_like(u,sh/255.),u,np.ones_like(u)),axis=-1)
 else:
  ix=np.floor(u*sw-.5).astype(int);iy=np.floor(v*sh-.5).astype(int)
  out=np.stack((f(ix,iy+1),f(ix+1,iy+1),f(ix+1,iy),f(ix,iy)),axis=-1)
 return np.floor(np.clip(out,0,1)*255+.5).astype(np.uint8)
def validate(result,readbacks,qualification=None):
 # This is independently enforced by runner, not trusted self-reported passed.
 if result.get('operation')!='native_depth_sampled'or result.get('passed')is not True or result.get('errno')!=0 or result.get('cleanup_errno')!=0 or result.get('resources_retained')is not False:raise ValueError('sampled depth unsuccessful/retained')
 for k,v in {'programs':30,'owners':30,'submissions':180,'finishes':60,'files':60,'completed_cases':60,'replays':30,'inflight_owner_checks':2592}.items():
  if type(result.get(k))is not int or result.get(k)!=v:raise ValueError('sampled depth count '+k)
 if any(result.get(k)is not False for k in ('private_depth_cpu_access','cpu_image_conversion','nvn_api','game_modules_executed','compare_sampler')):raise ValueError('scope changed')
 if result.get('same_job_replay')is not True or result.get('pixel_oracle_on_host')is not True or result.get('stage')!='complete' or result.get('chip_id')!=0x06050002:raise ValueError('scope/chip/stage mismatch')
 rows=result.get('cases');expected_rows=qualification['cases']if qualification else None
 if not isinstance(rows,list)or len(rows)!=30 or(expected_rows is not None and rows!=expected_rows):raise ValueError('normal catalogue differs')
 checks=0;worst=0;guards=0;readonly=0
 for i,name in enumerate(NAMES):
  c=case(i);row=rows[i//2]
  for k,v in c.items():
   if type(row.get(k))is not int or row.get(k)!=v:raise ValueError('case matrix '+k)
  raw=readbacks[name];raw=raw.read_bytes()if hasattr(raw,'read_bytes')else raw
  if len(raw)!=row['bo_bytes']:raise ValueError('raw length')
  a=np.frombuffer(raw,dtype=np.uint8);mask=np.zeros(len(a),dtype=bool)
  for y in range(c['height']):mask[4096+y*row['pitch']:4096+y*row['pitch']+c['width']*4]=True
  if np.any(a[~mask]!=0xa5):raise ValueError('target guard')
  actual=np.stack([a[4096+y*row['pitch']:4096+y*row['pitch']+c['width']*4].reshape(c['width'],4)for y in range(c['height'])]);expected=pixels(c,i%2);d=np.abs(actual.astype(np.int16)-expected.astype(np.int16));delta=int(d.max());worst=max(worst,delta)
  if delta>1:raise ValueError('sampled depth pixels '+str(i)+' delta '+str(delta))
  checks+=actual.size;guards+=int((~mask).sum());readonly+=row['readonly_bytes']
 if result.get('guard_byte_checks')!=guards or result.get('readonly_byte_checks')!=readonly:raise ValueError('byte checks mismatch')
 return dict(passed=True,channel_checks=checks,max_error=worst,guard_byte_checks=guards,readonly_byte_checks=readonly)
