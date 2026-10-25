#!/usr/bin/env python3
"""Independent synthetic protocol/oracle controls; never GPU evidence."""
import argparse,copy,json,math,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from front_facing_validation import validate,NAMES
p=argparse.ArgumentParser();p.add_argument('--check',type=Path,required=True);p.add_argument('--host-result',type=Path);p.add_argument('--host-raw',type=Path);args=p.parse_args();check=json.loads(args.check.read_text());page=lambda n:(n+4095)//4096*4096
uid=10234;nonce='a'*32;groups=[];captured={};guards=readonly=0
for g in range(3):
 v=check['targets'][g!=0];d=check['depth_view'];w=v['width'];h=v['height'];cp=v['pitch'];cb=page(v['required_span_bytes']+8192);dp=d['pitch']if g==2 else 0;db=page(d['bytes'])if g==2 else 0;ro=page(45060)-4+4*4096+(2*(page(40964)-4)if g==2 else 0)
 groups.append(dict(group=g,recipe_case=3 if g==2 else g,width=w,height=h,color_pitch=cp,color_bytes=cb,depth_pitch=dp,depth_bytes=db,readonly_bytes=ro))
 for t in range(64):
  origin=t//32;front=t//16%2;cull=t//4%4;winding=t//2%2
  for repeat in range(2):
   # Closed-form independent from validator's signed area implementation.
   isfront=bool(winding^repeat^origin)==(front==0);hidden=bool(cull&(1 if isfront else 2));color=bytearray([165])*cb;depth=bytearray([165])*db
   for y in range(h):
    for x in range(w):
     if hidden:rgba=[165]*4
     elif g==0:rgba=[255,0,0,255]if isfront else[0,255,0,255]
     elif g==1:rgba=[int(math.floor((x+.5)*.01*255+.5)),int(math.floor((y+.5)*.01*255+.5)),255 if isfront else 0,255]
     else:rgba=[255 if isfront else 0]*4
     color[4096+y*cp+x*4:4096+y*cp+x*4+4]=bytes(rgba)
     if g==2:depth[y*dp+x*4:y*dp+x*4+4]=struct.pack('<f',1. if hidden else .5)
   number=(g*64+t)*2+repeat;captured['front-color-%03u.raw'%number]=bytes(color)
   if g==2:captured['front-z-%03u.raw'%number]=bytes(depth)
   guards+=cb-w*h*4+(db-w*h*4 if g==2 else 0);readonly+=ro
result=dict(schema=1,operation='native_front_facing',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,submissions=640,finishes=640,files=512,jobs=320,programs=3,completed_cases=384,replays=192,inflight_owner_checks=3776,pixel_checks=0,bad_case=0xffffffff,bad_byte=0xffffffffffffffff,passed=True,resources_retained=False,pixel_oracle_on_host=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,same_job_replay=True,run_nonce=nonce,run_directory='pipeline-'+nonce,groups=groups,guard_byte_checks=guards,readonly_byte_checks=readonly)
accepted=validate(result,captured,check,nonce,uid);negative=0
for key,value in [('passed',False),('resources_retained',True),('submissions',639),('finishes',639),('completed_cases',383),('inflight_owner_checks',1),('readonly_byte_checks',1),('guard_byte_checks',1),('run_nonce','b'*32),('uid',True)]:
 bad=copy.deepcopy(result);bad[key]=value
 try:validate(bad,captured,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(key)
for field,value in [('recipe_case',3),('width',1),('color_pitch',1),('depth_pitch',1),('readonly_bytes',1)]:
 bad=copy.deepcopy(result);bad['groups'][0][field]=value
 try:validate(bad,captured,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError(field)
for name,off in [('front-color-000.raw',0),('front-color-000.raw',-1),('front-color-000.raw',4096),('front-color-128.raw',4098),('front-z-256.raw',0),('front-color-024.raw',4096)]:
 bad=dict(captured);blob=bytearray(bad[name]);blob[off]^=255;bad[name]=bytes(blob)
 try:validate(result,bad,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError((name,off))
bad=dict(captured);bad['front-color-000.raw']=bad['front-color-001.raw']
try:validate(result,bad,check,nonce,uid)
except RuntimeError:negative+=1
else:raise AssertionError('replay/facing swapped')
for short in (False,True):
 bad=dict(captured)
 if short:bad[NAMES[0]]=b'x'
 else:del bad[NAMES[-1]]
 try:validate(result,bad,check,nonce,uid)
 except RuntimeError:negative+=1
 else:raise AssertionError('inventory')
nonexecuting=False
if args.host_result:
 host=json.loads(args.host_result.read_text());host.update(uid=uid,run_nonce=nonce,run_directory='pipeline-'+nonce);actual={name:(args.host_raw/'success'/name).read_bytes()for name in NAMES}
 try:validate(host,actual,check,nonce,uid)
 except RuntimeError as error:
  if 'FACE/window/origin/cull/FragCoord'not in str(error):raise AssertionError('HOST RAM must fail pixels, not protocol '+str(error))
  nonexecuting=True
 else:raise AssertionError('unexecuted HOST bytes falsely accepted')
assert negative==24
print(json.dumps(dict(passed=True,host_only=True,gpu_execution=False,nonexecuting_HOST_pixels_rejected=nonexecuting,positive_synthetic_cases=384,negative_cases=negative,validator=accepted)))
