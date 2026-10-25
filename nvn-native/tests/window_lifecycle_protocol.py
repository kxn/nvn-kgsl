"""HOST ONLY synthetic protocol tests for the independent Window evidence checker."""
from pathlib import Path
import sys
import importlib.util,subprocess,math,json,io,hashlib
from unittest.mock import patch
from PIL import Image
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
out=args.out.resolve()
if not out.is_relative_to(root/'.local') or out.exists():parser.error('choose a new ignored output directory')
out.mkdir(parents=True)
checker=root/'nvn-native/tools/window_lifecycle.py'
spec=importlib.util.spec_from_file_location('window_lifecycle',checker);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
nonce='1234abcd'*4

def pixels(case):
 b=bytearray()
 for y in range(64):
  for x in range(64):
   if case==0:p=(255,0,0,255)
   elif case==1:p=(0,255,0,255) if x>=32 else (0,0,0,255)
   elif case==2:p=(0,0,0,255)
   else:
    g,h=(x+.5)/128,(y+.5)/128;p=tuple(math.floor(v*255+.5) for v in (1-g-h,g,h,1))
   b.extend(p)
 return bytes(b)
image=Image.frombytes('RGBA',(64,64),pixels(3));png=io.BytesIO();image.save(png,format='PNG');png=png.getvalue()
results=[]
for slots in (2,3):
 for mode in ('normal','stale','alias','badpixel','extra-file','falsefinal','under-count','cpu-copy',
              'types-target','negative-inode','bad-generation','missing-top','counter-float',
              'hold-mask','phase-lie','uid-mismatch','device-mismatch','uid-other-package','false-schema','sc-generation','sc-shown','readback-count','missing-guard'):
  phase=[1,True];folder=out/('lifecycle-host-'+str(slots)+'-'+mode);folder.mkdir(exist_ok=True);path=folder/'proof.json'
  evidence=dict(run_nonce=nonce,build=dict(window_slots=slots,activity='MockOnly'),scope='HOST ONLY synthetic checker protocol; zero ADB/GPU',package_uid='package:MOCK uid:10234')
  def data():
   n,ready=phase
   j=dict(operation='nvn_window',run_nonce=nonce,slots=slots,owner_thread='worker',uid=10234,errno=0,resources_retained=False,
    generation=n,ready=ready,current_frames=slots*4,current_case=3,window_width=64,window_height=64,window_screen_x=0,window_screen_y=0,
    window_created=n,window_destroyed=n-int(ready),started_sessions=n,completed_sessions=n-int(ready),worker_running=ready,
    directory='/data/mock/files/window-'+nonce+'-g'+str(n),frames=slots*4*n,passed=not ready and n==2,
    schema=1,phase='presented' if ready else 'retired',cpu_image_copy=False,game_modules_executed=False,
    current_slot=slots-1,current_case_mask=(1<<(slots*4))-1,held_final=True,hold_released_by_pause=not ready,
    activity_destroyed=False,resumes=n,pauses=n-int(ready))
   j['probe']=dict(operation='nvn_window',schema=1,slots=slots,errno=0,cleanup_errno=0,completed_cases=slots*4,nvn_submissions=slots*4,
    uid=10234,chip_id=0x06050002,image_schema=3,window_presentations=slots*4,channel_checks=slots*65536,owner_release_calls=slots,target_cpu_writes=0,raw_draw_calls=0,
    window_acquire_calls=slots*2,queue_acquire_calls=slots*2,resources_retained=False,game_modules_executed=False,cpu_image_copy=False,
    presented_callbacks=slots*4,readback_locks=slots*4,readback_unlocks=slots*4,
    external_pools_imported=slots,external_pools_released=slots,readonly_byte_checks=1,guard_byte_checks=1,guarded_object_checks=1,
    sc_before_retire=dict(generation=n,acquires=slots*4,enqueued=slots*4,shown=slots*4,releases=slots*4-1,
      slots=slots,current_index=slots-1,first_error=0,show_completed=slots*4,completion_callbacks=slots*4),
    passed=True,window_finalized=True,targets=[dict(slot=i,pitch=256,inode=100+i,fd_bytes=16384,pixel_offset=0,device_major=0,device_minor=7,fd_at_import=7+i,fd_index=0) for i in range(slots)])
   if mode=='sc-generation':j['probe']['sc_before_retire']['generation']+=100
   if mode=='sc-shown':j['probe']['sc_before_retire']['shown']-=1
   if mode=='readback-count':j['probe']['readback_locks']-=1
   if mode=='missing-guard':del j['probe']['guard_byte_checks']
   if mode=='types-target':
    for t in j['probe']['targets']:
     for key in ('slot','pitch','fd_bytes','pixel_offset','inode'):t[key]=float(t[key])
     t['device_major']='0'
   if mode=='negative-inode':j['probe']['targets'][0]['inode']=-1
   if mode=='bad-generation' and not ready:
    j['generation']=n+100;j['directory']='/data/mock/files/window-'+nonce+'-g'+str(n+100)
   if mode=='missing-top':
    del j['errno'];del j['resources_retained']
   if mode=='counter-float':
    j['completed_sessions']=float(j['completed_sessions']);j['frames']=float(j['frames'])
   if mode=='hold-mask':j['current_case_mask']=0;j['held_final']=False
   if mode=='phase-lie':j['phase']='drawing'
   if mode=='uid-mismatch':j['probe']['uid']=12345
   if mode=='device-mismatch':j['probe']['chip_id']=0
   if mode=='uid-other-package':j['uid']=10235;j['probe']['uid']=10235
   if mode=='false-schema':j['schema']=True
   if mode=='stale':j['run_nonce']='0'*32
   if mode=='alias':j['probe']['targets'][1]['inode']=100
   if mode=='falsefinal':j['probe']['window_finalized']=False
   if mode=='under-count':j['probe']['completed_cases']-=1
   if mode=='cpu-copy':j['probe']['cpu_image_copy']=True
   return j
  def run(*argv,check=True,timeout=20):
   if argv[:3]==('shell','input','keyevent'):phase[1]=False
   elif argv[:3]==('shell','am','start'):phase[:]=[2,True]
   elif argv[:4]==('shell','run-as','MOCK','cat'):
    return subprocess.CompletedProcess(argv,0,json.dumps(data()),'')
   elif argv[:4]==('shell','run-as','MOCK','find'):
    names=[f'{argv[4]}/window-slot-{i}-case-{j}.rgba' for i in range(slots) for j in range(4)]
    if mode=='extra-file':names.append(argv[4]+'/unexpected')
    return subprocess.CompletedProcess(argv,0,'\n'.join(names)+'\n','')
   else:raise AssertionError(argv)
   return subprocess.CompletedProcess(argv,0,'','')
  def command(argv,**kwargs):
   if 'screencap' in argv:raw=png
   else:
    name=argv[-1];raw=pixels(int(name.split('-case-')[1].split('.')[0]))
    if mode=='badpixel' and 'case-0' in name:raw=b'\x00'+raw[1:]
   return subprocess.CompletedProcess(argv,0,raw,b'')
  error=None
  with patch.object(m.subprocess,'run',command):
   try:m.exercise(run,['MOCK-ADB'],'MOCK',evidence,path)
   except Exception as e:error=type(e).__name__+': '+str(e)
  assert (error is None)==(mode=='normal'),(slots,mode,error)
  results.append(dict(slots=slots,mode=mode,rejected=bool(error),error=error))
(out/'proof.json').write_text(json.dumps(dict(scope='HOST ONLY synthetic checker protocol; zero ADB/GPU',passed=True,cases=results,checker_sha256=hashlib.sha256(checker.read_bytes()).hexdigest()),indent=2)+'\n')
print('PASS',len(results),'synthetic Window evidence cases')
