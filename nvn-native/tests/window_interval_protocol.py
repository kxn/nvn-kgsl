#!/usr/bin/env python3
"""HOST pure protocol / mocked transport. Does not build, deploy or run a guest."""
import contextlib,copy,hashlib,importlib.util,io,json,os,shlex,subprocess,sys,tempfile
from pathlib import Path
from unittest.mock import patch
NATIVE=Path(__file__).resolve().parents[1];sys.path.insert(0,str(NATIVE/'tools'))
import window_interval_runtime as runtime
import run_window_source_intervals as batch
checks=0
def T(value):
 global checks
 checks+=1
 assert value

def rejects(fn):
 try:fn()
 except (RuntimeError,ValueError):T(True)
 else:T(False)

def record():return dict(window_source_check={'kind':'native-texture-consumer-bridge-v1'},window_source_slots=2,activity='org.nvn.nativeprobe.PresentActivity')
def probe(i,slots):
 effective=1 if i==0 and slots==2 else i
 return dict(present_interval_configured=i is not None,present_interval=1 if i is None else i,
  interval_getter_checks=0 if i is None else 3+slots*8,post_enqueue_interval_mutations=0 if i is None else slots*4,
  platform_snapshot_observations=0 if i is None else slots*4,cadence_measured=False,
  interval_platform=dict(last=effective,shown=[slots*4 if k==effective else 0 for k in range(6)]))
def main():
 for slots in (2,3):
  r=record();r['window_source_slots']=slots
  for i in range(6):T(runtime.requested(r,i)==i);T(runtime.validate(probe(i,slots),i,slots)['effective']==(1 if i==0 and slots==2 else i))
  T(runtime.validate(probe(None,slots),None,slots)['cadence_measured']is False)
  for i in range(6):
   good=probe(i,slots)
   for field in ('present_interval_configured','present_interval','interval_getter_checks','post_enqueue_interval_mutations','platform_snapshot_observations','cadence_measured','interval_platform'):
    bad=copy.deepcopy(good);bad.pop(field);rejects(lambda:runtime.validate(bad,i,slots))
   for k in range(6):
    bad=copy.deepcopy(good);bad['interval_platform']['shown'][k]+=1;rejects(lambda:runtime.validate(bad,i,slots))
   bad=copy.deepcopy(good);bad['interval_platform']['last']=True;rejects(lambda:runtime.validate(bad,i,slots))
   rejects(lambda:runtime.validate(good,(i+1)%6,slots))
 for value in (-1,6,255,True,'0',0.0):rejects(lambda:runtime.requested(record(),value))
 for key in ('nvn_texture_bridge_check','native_pipeline_check','texture_conversion_check','nvn_sampler_check','nvn_counter_check','nvn_window_check','fullsize_display_check','graphics_check','device_inspect'):
  bad=record();bad[key]={'kind':'other'};rejects(lambda:runtime.requested(bad,0))
 for bad in (None,[],{},{'window_source_check':{}},{'window_source_check':{'kind':'x'},'window_source_slots':True},dict(record(),activity='other')):rejects(lambda:runtime.requested(bad,0))
 nonce='0123456789abcdef0123456789abcdef'
 for i in range(6):T(runtime.payload(nonce,i).encode()==(nonce+' '+f'{i:03d}').encode())
 for n in ('',nonce+'0','G'*32,'../'+'a'*29):rejects(lambda:runtime.payload(n,0))
 with tempfile.TemporaryDirectory()as td:
  root=Path(td);(root/'files').mkdir();commands=[]
  def run(*argv):
   commands.append(argv)
   if argv[0]=='shell'and len(argv)==2:
    q=shlex.split(argv[1]);T(q[:4]==['run-as','org.nvn.nativeprobe','sh','-c']);return subprocess.run(['sh','-c',q[4]],cwd=root,text=True,capture_output=True,check=True)
   T(argv[:3]==('shell','run-as','org.nvn.nativeprobe'))
   return subprocess.run(list(argv[3:]),cwd=root,text=True,capture_output=True,check=True)
  for i in range(6):
   out=runtime.deploy_config(run,'org.nvn.nativeprobe',nonce,i);T(out['bytes']==36);T((root/'files/window-source-present-interval').read_text()==runtime.payload(nonce,i))
  runtime.deploy_config(run,'org.nvn.nativeprobe',nonce,None);T(not(root/'files/window-source-present-interval').exists())
  def badhash(*a):
   if 'sha256sum'in a:return subprocess.CompletedProcess(a,0,'00 files/window-source-present-interval\n','')
   return run(*a)
  rejects(lambda:runtime.deploy_config(badhash,'org.nvn.nativeprobe',nonce,0))
  # Batch invokes the normal runner, one APK, six fresh evidence files. All ADB
  # work is replaced; test children never execute a binary or platform code.
  build=root/'build';build.mkdir();(build/'build-record.json').write_text(json.dumps(record()));(build/'probe.apk').write_bytes(b'HOST mock APK - not deployable')
  original_sha=hashlib.sha256((build/'probe.apk').read_bytes()).hexdigest()
  def exercise(fault=None):
   dest=root/('batch-'+str(fault));calls=[]
   def child(argv,**kwargs):
    T(kwargs=={'capture_output':True});T(Path(argv[1])==NATIVE/'tools/run_android_probe.py');T(Path(argv[argv.index('--build')+1])==build)
    i=int(argv[argv.index('--window-source-present-interval')+1]);T(('--install'in argv)==(i==0));path=Path(argv[argv.index('--evidence')+1]);calls.append(i)
    interval=i if fault!='wrong-interval'else(i+1)%6
    d=dict(probe_stopped=fault!='stop',screen_and_lifecycle_verified=True,window_source_present_interval=interval,
           run_nonce='a'*32 if fault=='nonce'else f'{i+1:032x}',window_interval_verification=[dict(requested=i,platform_argument_snapshot_checked=True,cadence_measured=False)]*2)
    if fault=='verification':d['window_interval_verification']=[]
    if fault=='malformed-verification':d['window_interval_verification']=[None,None]
    if fault=='malformed-evidence':d=[]
    if fault!='missing':path.write_text(json.dumps(d))
    return subprocess.CompletedProcess(argv,1 if fault=='returncode'else 0,b'mocked child stdout',b'')
   argv=['batch','--serial','NO-ADB','--build',str(build),'--evidence',str(dest),'--install']
   with patch.object(sys,'argv',argv),patch.object(batch.subprocess,'run',child),contextlib.redirect_stdout(io.StringIO()):status=batch.main()
   d=json.loads((dest/'batch.json').read_text());T(d['passed']is(fault is None));T(status==(0 if fault is None else 1));T(calls==list(range(6)) if fault is None else len(calls)==(2 if fault=='nonce'else 1));T(d['cadence_measured']is False)
  for fault in (None,'returncode','stop','nonce','wrong-interval','verification','malformed-verification','malformed-evidence','missing'):exercise(fault)
  T(hashlib.sha256((build/'probe.apk').read_bytes()).hexdigest()==original_sha)
  # Reject unrelated mode before even resolving source inventory/ADB.
  b=root/'other';b.mkdir();(b/'build-record.json').write_text(json.dumps({'device_inspect':True}))
  trap=root/'bin';trap.mkdir();marker=root/'ADB-WAS-CALLED';adb=trap/'adb';adb.write_text('#!/bin/sh\ntouch '+shlex.quote(str(marker))+'\nexit 99\n');adb.chmod(0o700)
  q=subprocess.run([sys.executable,str(NATIVE/'tools/run_android_probe.py'),'--serial','NO-ADB','--build',str(b),'--evidence',str(root/'unused.json'),'--window-source-present-interval','0'],env=dict(os.environ,PATH=str(trap)+':'+os.environ['PATH']),capture_output=True,text=True)
  T(q.returncode==2);T('requires only the standard Window source qualifier'in q.stderr);T(not marker.exists())
 print(json.dumps(dict(passed=True,checks=checks,gpu_executes=0,scope=__doc__)))
if __name__=='__main__':main()
