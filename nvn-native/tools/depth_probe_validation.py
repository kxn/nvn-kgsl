"""Independent scalar depth comparisons/quantization + authored triangle algebra.
No CPU runtime depth image conversion. NumPy verifies saved LINEAR CP_BLIT bytes.
"""
import re
NAMES=tuple(name for i in range(144)for name in (('depth-color-%03u.raw'%i,'depth-z-%03u.raw'%i)if i<132 else('depth-z-%03u.raw'%i,)))
def clear(mode):return 0. if mode in(3,4)else 1.
def value(mode,repeat):
 if mode in(2,6):return .25 if repeat else 1.
 if mode==3:return .75 if repeat else 0.
 if mode==4:return 0. if repeat else .75
 return 1. if repeat else .25

def accepts(mode,z,c):
 if mode>=7:return True
 return (False,z<c,z==c,z<=c,z>c,z!=c,z>=c)[mode]

def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID/nonce required')
 want=dict(schema=2,operation='native_depth',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=216,programs=13,submissions=432,finishes=432,files=276,completed_cases=144,replays=72,inflight_owner_checks=2580,pixel_checks=0)
 if any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('depth completion/owner matrix')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True,stencil=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('depth lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('depth namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact color+depth full raw inventory')
 if type(data.get('cases'))is not list or len(data['cases'])!=72:raise RuntimeError('depth case matrix')
 page=lambda n:(n+4095)//4096*4096
 guards=readonly=channels=depthpixels=0;maxcolor=maxdepth=0;total=0
 ro=page(45060)-4+2*(page(40964)-4)+5*4096
 for test,case in enumerate(data['cases']):
  group=test//11 if test<66 else test-66;format=0x32+group//2;origin=group%2;mode=test%11 if test<66 else 11;zero_color=mode==11;width,height=(73,41)if mode&1 else(64,64)
  tv=next(v for v in check['targets']if v['width']==width and v['height']==height);dv=next(v for v in check['depth_views']if v['format']==format and v['width']==width)
  fields=dict(case=test,format=format,origin=origin,mode=mode,width=width,height=height,color_pitch=0 if zero_color else tv['pitch'],color_bytes=0 if zero_color else page(tv['required_span_bytes']+8192),depth_pitch=dv['pitch'],depth_bytes=page(dv['bytes']),readonly_bytes=ro)
  if type(case)is not dict or any(type(case.get(k))is not int or case[k]!=v for k,v in fields.items()):raise RuntimeError('depth actual FDL/case identity')
  if type(case.get('zero_color'))is not bool or case['zero_color']is not zero_color or type(case.get('dummy_color'))is not bool or case['dummy_color']is not(mode==10):raise RuntimeError('actual zero/dummy color role')
  y,x=np.mgrid[:height,:width];scissor=(x>=5)&(x<width-7)&(y>=3)&(y<3+height//2)
  u=(x+.5)/width;v=(y+.5)/height
  if origin:v=1-v
  rgba=np.stack((1-(u+v)*.5,u*.5,v*.5,np.ones_like(u)),axis=-1);rgba=np.floor(np.clip(rgba,0,1)*255+.5).astype(np.uint8)
  for repeat in range(2):
   number=test*2+repeat;cz=clear(mode);z=value(mode,repeat);passes=accepts(mode,z,cz);written=scissor&passes&(mode!=8)&(mode!=9)
   expected_color=np.empty((height,width,4),dtype=np.uint8);expected_color[:]=(17,29,43,59)
   if mode!=10 and passes:expected_color[scissor]=rgba[scissor]
   if not zero_color:
    cp=case['color_pitch'];blob=captured['depth-color-%03u.raw'%number]
    if type(blob)is not bytes or len(blob)!=case['color_bytes']:raise RuntimeError('color full raw size')
    raw=np.frombuffer(blob,dtype=np.uint8);mask=np.zeros(raw.size,dtype=bool)
    for row in range(height):mask[4096+row*cp:4096+row*cp+width*4]=True
    if np.any(raw[~mask]!=0xa5):raise RuntimeError('color full guard')
    actual=np.frombuffer(blob,dtype=np.uint8,offset=4096,count=cp*height).reshape(height,cp)[:,:width*4].reshape(height,width,4)
    delta=np.abs(actual.astype(np.int16)-expected_color.astype(np.int16));error=int(delta.max());maxcolor=max(maxcolor,error)
    if error>1:raise RuntimeError('depth-test/origin/scissor color '+str(number))
    guards+=int((~mask).sum());channels+=width*height*4;total+=len(blob)
   blob=captured['depth-z-%03u.raw'%number];pitch=case['depth_pitch'];cpp=2 if format==0x32 else 4
   if type(blob)is not bytes or len(blob)!=case['depth_bytes']:raise RuntimeError('depth full LINEAR raw size')
   raw=np.frombuffer(blob,dtype=np.uint8);mask=np.zeros(raw.size,dtype=bool)
   for row in range(height):mask[row*pitch:row*pitch+width*cpp]=True
   if np.any(raw[~mask]!=0xa5):raise RuntimeError('depth LINEAR padding/tail guard')
   pixels=np.frombuffer(blob,dtype=np.uint8,count=pitch*height).reshape(height,pitch)[:,:width*cpp].copy()
   expected_depth=np.where(written,z,cz)
   if format==0x34:
    actual=pixels.view('<f4').reshape(height,width)
    if not np.array_equal(actual,expected_depth.astype(np.float32)):raise RuntimeError('exact D32F export/test '+str(number))
   else:
    maxvalue=65535 if format==0x32 else 16777215;actual=pixels.view('<u2'if format==0x32 else'<u4').reshape(height,width).astype(np.int64)
    if format==0x33:actual&=0xffffff
    expected=np.floor(expected_depth*maxvalue+.5).astype(np.int64);delta=np.abs(actual-expected);error=int(delta.max());maxdepth=max(maxdepth,error)
    # The original Gallium float->UNORM clear/write routes may differ by one
    # quantization unit. Clear0/1 and all untouched pixels must be exact.
    if np.any(delta[~written])or np.any(delta[written]>1):raise RuntimeError('D16/D24 actual depth '+str(number))
   guards+=int((~mask).sum());depthpixels+=width*height;total+=len(blob);readonly+=ro
 if type(data.get('guard_byte_checks'))is not int or data['guard_byte_checks']!=guards or type(data.get('readonly_byte_checks'))is not int or data['readonly_byte_checks']!=readonly:raise RuntimeError('full guard/readonly coverage')
 return dict(passed=True,files=276,submissions=432,depth_writes_and_tests=144,true_zero_color_draws=12,dummy_color_draws=12,color_channels=channels,depth_pixels=depthpixels,max_channel_error=maxcolor,max_unorm_depth_error=maxdepth,guard_bytes=guards,readonly_bytes=readonly,full_raw_bytes=total,oracle='independent authored scalar depth compare/quantization + fulltriangle barycentrics; actualFDL pitch only',nvn_api=False,game_modules_executed=False)
