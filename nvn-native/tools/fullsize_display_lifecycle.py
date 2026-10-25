"""Raw KGSL full-size AHB lifecycle/oracle; no NVN Draw/game-frame claim."""
import hashlib,io,json,re,shlex,subprocess,time
from pathlib import Path
W,H=2880,1620
OP='kgsl_ahb_linear_copy'
PREFIX='fullsize'
SLOTS_KEY='fullsize_display_slots'
SESSIONS_KEY='fullsize_sessions'
VERIFIED_KEY='fullsize_verification'
SCOPE='Raw KGSL authored-copy AHB/SC qualification; no NVN Draw or game frame'

def parse(text):
 def pairs(values):
  out={}
  for k,v in values:
   if k in out:raise ValueError('duplicate JSON key')
   out[k]=v
  return out
 d=json.loads(text,object_pairs_hook=pairs,parse_constant=lambda v:(_ for _ in ()).throw(ValueError(v)))
 if type(d) is not dict:raise ValueError('fullsize result must be object')
 return d

def integer(v,lo=0,hi=(1<<64)-1):return type(v) is int and lo<=v<=hi

def validate_result(d,nonce,slots,uid):
 numbers=('slots','errno','generation','window_created','window_destroyed','started_sessions','completed_sessions','frames','current_frames','current_case','current_slot','current_case_mask','pauses','resumes')
 flags=('ready','worker_running','resources_retained','passed','held_final','hold_released_by_pause','activity_destroyed','cpu_image_copy','game_modules_executed')
 if (d.get('operation')!=OP or d.get('schema')!=1 or type(d.get('schema')) is not int or d.get('run_nonce')!=nonce or d.get('slots')!=slots or d.get('owner_thread')!='worker' or
     any(not integer(d.get(k)) for k in numbers) or any(type(d.get(k)) is not bool for k in flags) or
     any(not integer(d.get(k),-(1<<31),(1<<31)-1) for k in ('window_width','window_height','window_screen_x','window_screen_y')) or
     type(d.get('uid')) is not int or d['uid']!=uid or uid<10000 or type(d.get('directory')) is not str or type(d.get('phase')) is not str or
     d['cpu_image_copy'] is not False or d['game_modules_executed'] is not False):raise RuntimeError('fullsize owner/run metadata mismatch')
 if d['errno'] or d['resources_retained']:raise RuntimeError('fullsize GPU/window retained or failed')

def present_state(d,n,slots):
 want=dict(phase='presented',ready=True,worker_running=True,current_frames=slots*4,current_case=3,current_case_mask=(1<<(slots*4))-1,held_final=True,hold_released_by_pause=False,activity_destroyed=False,started_sessions=n,completed_sessions=n-1,window_created=n,window_destroyed=n-1,frames=slots*4*n,passed=False)
 if any(d.get(k)!=v for k,v in want.items()) or not 0<=d['current_slot']<slots:raise RuntimeError('not the held final authored-copy case')

def validate_probe(probe,data,slots):
 want=dict(schema=1,operation=OP,width=W,height=H,slots=slots,uid=data['uid'],chip_id=0x06050002,errno=0,cleanup_errno=0,completed_cases=slots*4,gpu_submissions=slots*4,window_presentations=slots*4,presented_callbacks=slots*4,channel_checks=slots*4*W*H*4,readback_locks=slots*4,readback_unlocks=slots*4,target_sentinel_locks=slots,target_sentinel_unlocks=slots,target_cpu_pattern_writes=0,owner_release_calls=slots,nvn_draw_calls=0,raw_draw_calls=0)
 if type(probe) is not dict or any(type(probe.get(k)) is not type(v) or probe[k]!=v for k,v in want.items()) or any(probe.get(k) is not True for k in ('same_ib_replay','session_retired','session_destroyed','passed')) or any(probe.get(k) is not False for k in ('resources_retained','game_modules_executed','cpu_image_copy')):raise RuntimeError('fullsize producer did not prove all copies/readonly readbacks/retirement')
 if not integer(probe.get('readonly_byte_checks'),slots*4*(W*H*4+8192)):raise RuntimeError('source/code/job readonly verification missing')
 sc=probe.get('sc_before_retire');want=dict(generation=data['generation'],shown=slots*4,releases=slots*4-1,current_index=data['current_slot'],first_error=0)
 if type(sc) is not dict or any(type(sc.get(k)) is not int or sc[k]!=v for k,v in want.items()):raise RuntimeError('fullsize SC proof does not match captured generation')
 targets=probe.get('targets');identities=set();guards=0
 if type(targets) is not list or len(targets)!=slots:raise RuntimeError('target inventory missing')
 for i,t in enumerate(targets):
  fields=('slot','fd_index','fd_at_import','fd_bytes','pixel_offset','payload_bytes','pitch','inode','device_major','device_minor')
  if type(t) is not dict or any(not integer(t.get(k)) for k in fields):raise RuntimeError('invalid target metadata type')
  extent=((t['pitch']*((H+3)//4*4)+4095)//4096)*4096
  if t['slot']!=i or not 0<=t['fd_index']<8 or not 0<=t['fd_at_import']<(1<<31) or not 0<t['inode']<(1<<64) or any(t[k]>=(1<<32) for k in ('device_major','device_minor')) or t['pitch']<W*4 or t['pitch']>1048576 or t['pitch']%64 or t['payload_bytes']!=extent or t['fd_bytes']%4096 or t['pixel_offset']%64 or t['pixel_offset']>t['fd_bytes'] or extent>t['fd_bytes']-t['pixel_offset']:raise RuntimeError('unqualified fullsize backing extent')
  if i and t['pitch']!=targets[0]['pitch']:raise RuntimeError('mixed fullsize slot geometry')
  identities.add((t['device_major'],t['device_minor'],t['inode']));guards+=extent-W*H*4
 if len(identities)!=slots or type(probe.get('guard_byte_checks')) is not int or probe['guard_byte_checks']!=4*guards or guards<=0:raise RuntimeError('backing alias or padding guard checks differ')
 return slots*4*W*H*4

def check_image(payload,case,width=W,height=H):
 if len(payload)!=width*height*4:raise RuntimeError('tight active-image byte count mismatch')
 if case==0:row=bytes((255,0,0,255))*width
 elif case==1:row=bytes((0,0,0,255))*(width//2)+bytes((0,255,0,255))*(width-width//2)
 elif case==2:row=bytes((0,0,0,255))*width
 elif case!=3:raise RuntimeError('unknown authored case')
 for y in range(height):
  if case==3:
   blue=y*127//(height-1);green=[x*127//(width-1) for x in range(width)];row=bytes(c for g in green for c in (255-g-blue,g,blue,255))
  if payload[y*width*4:(y+1)*width*4]!=row:raise RuntimeError('independent fullsize raw-byte oracle mismatch')
 return width*height*4

def exercise(run,adb,package,evidence,destination):
 from PIL import Image
 dest=Path(destination);nonce=evidence['run_nonce'];slots=evidence['build'][SLOTS_KEY]
 if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce) or type(slots) is not int or slots not in (2,3):raise RuntimeError('invalid fullsize identity')
 match=re.fullmatch(r'package:'+re.escape(package)+r' uid:([0-9]+)',evidence['package_uid'])
 if not match:raise RuntimeError('package uid not exact')
 uid=int(match[1]);features=run('features').stdout.replace(',',' ').split()
 if 'shell_v2' not in features:raise RuntimeError('fullsize collector requires shell-v2 exit-status protocol')
 history=[];captures=[];sessions=[];evidence.update(lifecycle=history,screenshots=captures,screen_and_lifecycle_verified=False);evidence[SESSIONS_KEY]=sessions
 def save():dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(evidence,indent=2)+'\n')
 def wait(predicate,stage):
  deadline=time.monotonic()+120
  while True:
   # adb shell uses shell-v2 exit status; never treat exec-out cat rc as proof.
   q=run('shell','run-as',package,'cat','files/result.json',check=False)
   if q.returncode==0:
    try:d=parse(q.stdout)
    except (TypeError,ValueError):d=None
    if d:
     evidence['result']=d;validate_result(d,nonce,slots,uid)
     if predicate(d):history.append(dict(stage=stage,result=d));save();return d
   if time.monotonic()>=deadline:raise RuntimeError('fullsize lifecycle deadline: '+stage)
   time.sleep(.1)
 def capture(d,label):
  width,height,left,top=[d[k] for k in ('window_width','window_height','window_screen_x','window_screen_y')]
  for attempt in range(3):
   raw=subprocess.run(adb+['exec-out','screencap','-p'],check=True,capture_output=True,timeout=30).stdout;screen=Image.open(io.BytesIO(raw)).convert('RGB');path=dest.with_name(dest.stem+'-'+label+f'-{attempt}.png');path.write_bytes(raw);samples=[]
   if width>0 and height>0 and left>=0 and top>=0 and left+width<=screen.width and top+height<=screen.height:
    for fy in (1,3,5,7):
     for fx in (1,3,5,7):
      x=int((fx/8)*W);y=int((fy/8)*H);g=x*127//(W-1);b=y*127//(H-1);expected=[255-g-b,g,b];xy=(left+int((x+.5)*width/W),top+int((y+.5)*height/H));actual=list(screen.getpixel(xy));samples.append(dict(source_xy=[x,y],screen_xy=xy,expected=expected,actual=actual,maximum_delta=max(abs(a-b) for a,b in zip(expected,actual))))
   passed=len(samples)==16 and all(z['maximum_delta']<=8 for z in samples);captures.append(dict(stage=label,generation=d['generation'],run_nonce=nonce,path=str(path.resolve()),sha256=hashlib.sha256(raw).hexdigest(),sample_checks=samples,passed=passed,scope='Actual screencap vs authored input pattern; same AHB written by GPU copy, not NVN/game'))
   save()
   if passed:return
   time.sleep(.2)
  raise RuntimeError('fullsize actual screen mismatch')
 def collect(d,label):
  channels=validate_probe(d['probe'],d,slots);directory=f'{PREFIX}-{nonce}-g{d["generation"]}'
  if Path(d['directory']).name!=directory:raise RuntimeError('fullsize directory not bound to generation/nonce')
  names=[f'{PREFIX}-slot-{i}-case-{j}.rgba' for i in range(slots) for j in range(4)];wanted={'files/'+directory+'/'+n for n in names}
  inventory=run('shell','run-as',package,'find','files/'+directory,'-mindepth','1').stdout.splitlines()
  if len(inventory)!=len(wanted) or set(inventory)!=wanted:raise RuntimeError('fullsize regular inventory has stale/missing/extra entries')
  images={}
  for name in names:
   source='files/'+directory+'/'+name
   st=run('shell',shlex.join(['run-as',package,'stat','-c','%F %s',source])).stdout.strip()
   if st!=f'regular file {W*H*4}':raise RuntimeError('nonregular/symlink or wrong-size fullsize readback')
   checksum=run('shell','run-as',package,'sha256sum',source).stdout.strip().split()
   if len(checksum)!=2 or checksum[1]!=source or not re.fullmatch('[0-9a-f]{64}',checksum[0]):raise RuntimeError('invalid fullsize device hash response')
   q=subprocess.run(adb+['exec-out','run-as',package,'cat',source],capture_output=True,timeout=180)
   actual=hashlib.sha256(q.stdout).hexdigest()
   if q.returncode or len(q.stdout)!=W*H*4 or actual!=checksum[0]:raise RuntimeError('fullsize binary read failed or differs from verified device file')
   target=dest.with_name(dest.stem+'-'+label+'-'+name);target.write_bytes(q.stdout);check_image(q.stdout,int(name.split('-case-')[1].split('.')[0]));images[name]=dict(path=str(target.resolve()),sha256=actual,bytes=len(q.stdout),device_sha256=checksum[0],regular=True)
  sessions.append(dict(generation=d['generation'],result=d['probe'],images=images,independent_channel_checks=channels,inventory=inventory));save()
 try:
  previous=0
  for n in (1,2):
   shown=wait(lambda d:d['ready'] and d['generation']>previous,f'present-{n}');present_state(shown,n,slots);capture(shown,f'present-{n}')
   confirmed=wait(lambda d:True,f'confirm-{n}');present_state(confirmed,n,slots)
   if any(confirmed[k]!=shown[k] for k in ('generation','directory','window_width','window_height','window_screen_x','window_screen_y')):raise RuntimeError('fullsize generation changed during screenshot')
   run('shell','input','keyevent','KEYCODE_HOME')
   final=wait(lambda d:d['completed_sessions']==n and not d['worker_running'] and d['window_destroyed']>=n,f'retired-{n}')
   if final['generation']!=shown['generation'] or final['directory']!=shown['directory'] or final['phase']!='retired' or final['ready'] or not final['held_final'] or not final['hold_released_by_pause'] or final['current_case_mask']!=(1<<(slots*4))-1 or final['current_frames']!=slots*4 or final['frames']!=slots*4*n or final['started_sessions']!=n or final['window_created']!=n or final['window_destroyed']!=n or final['pauses']<n or final['passed'] is not (n==2):raise RuntimeError('fullsize final retirement does not match captured session')
   collect(final,f'gen-{n}');previous=shown['generation']
   if n==1:run('shell','am','start','-W','--activity-reorder-to-front','-n',package+'/'+evidence['build']['activity'])
  evidence['screen_and_lifecycle_verified']=True;evidence[VERIFIED_KEY]=dict(passed=True,slots=slots,generations=2,channel_checks=slots*8*W*H*4,scope=SCOPE)
 except Exception as e:evidence['lifecycle_error']=str(e);raise
 finally:save()
