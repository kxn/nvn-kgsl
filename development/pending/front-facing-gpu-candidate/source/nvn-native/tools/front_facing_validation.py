"""Independent signed-window-area, authored FACE/FragCoord/.5 depth closed form.
Pure offline validation, not a CPU runtime rasterizer or shader interpreter.
"""
import re
NAMES=tuple(name for i in range(384)for name in (('front-color-%03u.raw'%i,'front-z-%03u.raw'%i)if i>=256 else('front-color-%03u.raw'%i,)))
def case(test):
 if type(test)is not int or not 0<=test<192:raise ValueError('case bounds')
 group=test//64;t=test%64
 return dict(group=group,indexed=t%2,winding=t//2%2,cull=t//4%4,front=t//16%2,origin=t//32)
def facing(c,repeat):
 # Actual post-viewport positions; signed area, not a face-value fit.
 points=[(-1.,-1.),(3.,-1.),(-1.,3.)]
 if c['winding']^repeat:points[1],points[2]=points[2],points[1]
 scale=-1. if c['origin']else 1.;p=[(x,y*scale)for x,y in points]
 cross=(p[1][0]-p[0][0])*(p[2][1]-p[0][1])-(p[1][1]-p[0][1])*(p[2][0]-p[0][0]);clockwise=cross<0
 front=clockwise==(c['front']==0)
 return front,bool(c['cull']&(1 if front else 2))
def expected(c,repeat,width,height):
 import numpy as np
 front,culled=facing(c,repeat);color=np.full((height,width,4),165,np.uint8)
 if not culled:
  if c['group']==0:color[:]=(255,0,0,255)if front else(0,255,0,255)
  elif c['group']==1:
   y,x=np.mgrid[:height,:width];color[:,:,0]=np.floor((x+.5)*.01*255+.5).astype(np.uint8);color[:,:,1]=np.floor((y+.5)*.01*255+.5).astype(np.uint8);color[:,:,2]=255 if front else 0;color[:,:,3]=255
  else:color[:]=255 if front else 0
 z=np.full((height,width),1. if culled else .5,dtype='<f4')
 return color,z

def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='native_front_facing',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,submissions=640,finishes=640,files=512,jobs=320,programs=3,completed_cases=384,replays=192,inflight_owner_checks=3776,pixel_checks=0,bad_case=0xffffffff,bad_byte=0xffffffffffffffff)
 if any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('FACE complete protocol/owner/matrix')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('FACE boundary '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('FACE fresh namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact512 full raw inventory required')
 if type(data.get('groups'))is not list or len(data['groups'])!=3:raise RuntimeError('three actual builtin programs')
 page=lambda n:(n+4095)//4096*4096
 guards=readonly=channels=zpixels=total=0;maxcoord=0;visible=culled=front_draws=back_draws=0
 for group,g in enumerate(data['groups']):
  v=check['targets'][group!=0];d=check['depth_view'];width=v['width'];height=v['height'];depth=group==2
  ro=page(45060)-4+4*4096+(2*(page(40964)-4)if depth else 0)
  fields=dict(group=group,recipe_case=3 if depth else group,width=width,height=height,color_pitch=v['pitch'],color_bytes=page(v['required_span_bytes']+8192),depth_pitch=d['pitch']if depth else 0,depth_bytes=page(d['bytes'])if depth else 0,readonly_bytes=ro)
  if type(g)is not dict or set(g)!=set(fields)or any(type(g[k])is not int or g[k]!=value for k,value in fields.items()):raise RuntimeError('actual FDL/group identity')
  for t in range(64):
   c=case(group*64+t)
   for repeat in range(2):
    number=(group*64+t)*2+repeat;front,hidden=facing(c,repeat);culled+=hidden;visible+=not hidden;front_draws+=front;back_draws+=not front
    color,z=expected(c,repeat,width,height);blob=captured['front-color-%03u.raw'%number]
    if type(blob)is not bytes or len(blob)!=g['color_bytes']:raise RuntimeError('full color raw size '+str(number))
    raw=np.frombuffer(blob,np.uint8);mask=np.zeros(raw.size,bool)
    for y in range(height):mask[4096+y*g['color_pitch']:4096+y*g['color_pitch']+width*4]=True
    if np.any(raw[~mask]!=165):raise RuntimeError('color guard '+str(number))
    actual=np.frombuffer(blob,np.uint8,offset=4096,count=g['color_pitch']*height).reshape(height,g['color_pitch'])[:,:width*4].reshape(height,width,4)
    delta=np.abs(actual.astype(np.int16)-color.astype(np.int16))
    # Only FragCoord x/y conversion permits the predeclared1 UNORM unit.
    allowed=1 if group==1 and not hidden else 0
    if np.any(delta>allowed)or(group==1 and np.any(delta[:,:,2:])):raise RuntimeError('FACE/window/origin/cull/FragCoord '+str(number))
    maxcoord=max(maxcoord,int(delta.max()));guards+=int((~mask).sum());channels+=width*height*4;total+=len(blob)
    if depth:
     blob=captured['front-z-%03u.raw'%number]
     if type(blob)is not bytes or len(blob)!=g['depth_bytes']:raise RuntimeError('full depth raw size')
     raw=np.frombuffer(blob,np.uint8);mask=np.zeros(raw.size,bool)
     for y in range(height):mask[y*g['depth_pitch']:y*g['depth_pitch']+width*4]=True
     if np.any(raw[~mask]!=165):raise RuntimeError('depth actual FDL padding guard')
     actual=np.frombuffer(blob,np.uint8,count=g['depth_pitch']*height).reshape(height,g['depth_pitch'])[:,:width*4].copy().view('<f4').reshape(height,width)
     if not np.array_equal(actual,z):raise RuntimeError('exact D32F FragDepth/front/cull '+str(number))
     guards+=int((~mask).sum());zpixels+=width*height;total+=len(blob)
    readonly+=ro
 if visible!=192 or culled!=192 or front_draws!=192 or back_draws!=192:raise RuntimeError('independent geometry matrix coverage')
 if type(data.get('guard_byte_checks'))is not int or data['guard_byte_checks']!=guards or type(data.get('readonly_byte_checks'))is not int or data['readonly_byte_checks']!=readonly:raise RuntimeError('complete guard/readonly coverage')
 return dict(passed=True,files=512,draws=384,submissions=640,visible_draws=visible,culled_draws=culled,front_draws=front_draws,back_draws=back_draws,color_channels=channels,depth_pixels=zpixels,max_fragcoord_error=maxcoord,face_error=0,D32F_exact=True,guard_bytes=guards,readonly_bytes=readonly,full_raw_bytes=total,oracle='independent signed postviewport triangle area + unchanged authored FACE/FragCoord/.5 FragDepth closed forms',nvn_api=False,game_modules_executed=False)
