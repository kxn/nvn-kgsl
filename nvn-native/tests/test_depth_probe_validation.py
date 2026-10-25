#!/usr/bin/env python3
"""Synthetic protocol/oracle controls only. No GPU or simulated shader execution."""
import argparse,copy,json,math,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from depth_probe_validation import validate,NAMES
p=argparse.ArgumentParser();p.add_argument('--check',type=Path,required=True);p.add_argument('--host-result',type=Path);p.add_argument('--host-raw',type=Path);args=p.parse_args();check=json.loads(args.check.read_text());page=lambda n:(n+4095)//4096*4096
nonce='a'*32;uid=10234;ro=page(45060)-4+2*(page(40964)-4)+5*4096;captured={};cases=[];guards=0
# Independently enumerate the fixed shader-test controls. These arrays are
# deliberately not imported from the implementation/oracle module.
clears=[1,1,1,0,0,1,1,1,1,1,1,1]
zs=[(.25,1),(.25,1),(1,.25),(0,.75),(.75,0),(.25,1),(1,.25),(.25,1),(.25,1),(.25,1),(.25,1),(.25,1)]
passes=[(False,False),(True,False),(True,False),(True,False),(True,False),(True,False),(True,False),(True,True),(True,True),(True,True),(True,True),(True,True)]
for i in range(72):
 group=i//11 if i<66 else i-66;f=0x32+group//2;origin=group%2;mode=i%11 if i<66 else 11;zero_color=mode==11
 w,h=(73,41)if mode%2 else(64,64);tv=next(x for x in check['targets']if x['width']==w);dv=next(x for x in check['depth_views']if x['format']==f and x['width']==w);cb=0 if zero_color else page(tv['required_span_bytes']+8192);db=page(dv['bytes']);cp=0 if zero_color else tv['pitch'];dp=dv['pitch'];cpp=2 if f==0x32 else 4
 cases.append(dict(case=i,format=f,origin=origin,mode=mode,width=w,height=h,color_pitch=cp,color_bytes=cb,depth_pitch=dp,depth_bytes=db,readonly_bytes=ro,zero_color=zero_color,dummy_color=mode==10))
 for repeat in range(2):
  color=bytearray([0xa5])*cb;depth=bytearray([0xa5])*db
  for y in range(h):
   for x in range(w):
    inside=5<=x<w-7 and 3<=y<3+h//2;draw=inside and passes[mode][repeat];v=((h-y-.5)if origin else(y+.5))/h;u=(x+.5)/w
    rgba=[int(math.floor(c*255+.5))for c in (1-(u+v)/2,u/2,v/2,1)]if draw and mode!=10 else[17,29,43,59]
    if not zero_color:color[4096+y*cp+x*4:4096+y*cp+x*4+4]=bytes(rgba)
    d=zs[mode][repeat]if draw and mode not in(8,9)else clears[mode]
    value=struct.pack('<f',d)if f==0x34 else int(math.floor(d*(65535 if f==0x32 else 16777215)+.5)).to_bytes(cpp,'little')
    depth[y*dp+x*cpp:y*dp+x*cpp+cpp]=value
  if not zero_color:captured['depth-color-%03u.raw'%(i*2+repeat)]=bytes(color)
  captured['depth-z-%03u.raw'%(i*2+repeat)]=bytes(depth);guards+=(0 if zero_color else cb-w*h*4)+db-w*h*cpp
result=dict(schema=2,operation='native_depth',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=216,programs=13,submissions=432,finishes=432,files=276,completed_cases=144,replays=72,inflight_owner_checks=2580,pixel_checks=0,passed=True,resources_retained=False,pixel_oracle_on_host=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True,stencil=False,run_nonce=nonce,run_directory='pipeline-'+nonce,cases=cases,guard_byte_checks=guards,readonly_byte_checks=ro*144)
accepted=validate(result,captured,check,nonce,uid);negative=0
for key,value in [('passed',False),('resources_retained',True),('submissions',395),('finishes',395),('completed_cases',131),('inflight_owner_checks',2375),('readonly_byte_checks',1),('guard_byte_checks',1),('run_nonce','b'*32),('stencil',True)]:
 bad=copy.deepcopy(result);bad[key]=value
 try:validate(bad,captured,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(key)
for field,value in [('format',0x35),('origin',1),('color_pitch',1),('depth_pitch',1),('mode',10)]:
 bad=copy.deepcopy(result);bad['cases'][0][field]=value
 try:validate(bad,captured,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(field)
for name,offset,byte in [('depth-color-000.raw',0,0),('depth-z-002.raw',-1,0),('depth-color-020.raw',4096+5*cases[10]['color_pitch']+6*4,255),('depth-z-002.raw',5*cases[1]['depth_pitch']+6*2,255)]:
 bad=dict(captured);blob=bytearray(bad[name]);blob[offset]=byte;bad[name]=bytes(blob)
 try:validate(result,bad,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(name)
for change in ['short','missing']:
 bad=dict(captured)
 if change=='short':bad[NAMES[0]]=b'x'
 else:del bad[NAMES[-1]]
 try:validate(result,bad,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(change)
for name,field,value in [('zero-role','zero_color',False),('dummy-role','dummy_color',True),('zero-has-color','color_bytes',4096)]:
 bad=copy.deepcopy(result);bad['cases'][66][field]=value
 try:validate(bad,captured,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(name)
bad=dict(captured);blob=bytearray(bad['depth-z-132.raw']);blob[5*cases[66]['depth_pitch']+6*2]=0xff;bad['depth-z-132.raw']=bytes(blob)
try:validate(result,bad,check,nonce,uid)
except RuntimeError:negative+=1
else:raise AssertionError('true zero-COLOR depth write corruption')
nonexecuting=False
if args.host_result:
 host=json.loads(args.host_result.read_text());host.update(uid=uid,run_nonce=nonce,run_directory='pipeline-'+nonce)
 actual={name:(args.host_raw/name).read_bytes()for name in NAMES}
 try:validate(host,actual,check,nonce,uid)
 except RuntimeError as error:
  if 'D16/D24 actual depth'not in str(error):raise AssertionError('HOST no-op run failed protocol instead of pixel proof: '+str(error))
  nonexecuting=True
 else:raise AssertionError('nonexecuting HOST raw bytes falsely passed shader oracle')
print(json.dumps(dict(nonexecuting_HOST_pixels_rejected=nonexecuting,passed=True,host_only=True,gpu_execution=False,positive_synthetic_cases=144,negative_cases=negative,validator=accepted)))
