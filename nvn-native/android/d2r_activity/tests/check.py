from pathlib import Path
import subprocess,json,hashlib,os,shutil
root=Path.cwd();p=Path(__file__).resolve().parents[1];out=root/'.local/nvn-native/parallel/d2r-activity-controller-host-check';out.mkdir(exist_ok=True)
results=[]
for compiler,flags,label in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all'],'asan-ubsan'),('clang',['-fsanitize=function','-fsanitize-trap=function'],'function-abi')]:
 cmd=[compiler,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(p/'tests/include'),'-I'+str(root/'nvn-native/adapters/d2r/activity'),str(p/'tests/controller_host.c'),'-lpthread','-ldl','-o',str(out/label)]
 c=subprocess.run(cmd,text=True,capture_output=True);print(c.returncode,c.stderr);assert c.returncode==0
 for case in [0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24]:
  d=out/(label+'-case'+str(case));shutil.rmtree(d,ignore_errors=True);d.mkdir()
  for leaf in ['d2r-activity/runs','d2r-activity/modules','d2r-host/game/root','d2r-host/game/romfs','d2r-host/game/save','d2r-host/game/temp']:(d/leaf).mkdir(parents=True,exist_ok=True)
  config='0123456789abcdef0123456789abcdef 1\n'
  if case==8:config='!'+config[1:]
  if case==9:config=config[:33]+'9\n'
  (d/'d2r-activity/run.cfg').write_text(config)
  directory=d/'d2r-activity/runs/0123456789abcdef0123456789abcdef'
  if case==10:directory.mkdir(exist_ok=True)
  r=subprocess.run([str(out/label),str(case),str(d)],text=True,capture_output=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1'})
  expected=79 if case in (8,9,10,13,14,16,18,19,20,21) else 0
  print(label,case,r.returncode)
  if r.returncode!=expected:print(r.stdout,r.stderr);raise RuntimeError('fixture failed')
  result=None
  if case not in (8,9,10,13,14,16,18,19,20,21):
   start=json.loads((directory/'start.json').read_text());result=json.loads((directory/'result.json').read_text())
   assert result['first_frame'] is False and result['controller_links_core'] is False and result['uid']==10017
   assert result['errno']=={3:95,5:2,6:38,7:11,22:38}.get(case,0),(case,result)
   assert not list(directory.glob('*.pending'))
   if case==11:assert 'stage_returned' in (directory/'result.json').read_text()
   assert result['selinux_context_source']==('/proc/self/task/gettid/attr/current' if case==17 else '/proc/thread-self/attr/current')
  elif case in (13,14,16,18,19,20,21):assert not (directory/'start.json').exists() and not list(directory.glob('*.pending'))
  if case in (13,14,16,18,19,20,21):
   diagnostic=(directory/'stderr.log').read_text()
   phase={13:'publication-fsync',14:'publication-rename-noreplace',19:'process-stat-open',20:'publication-rename-noreplace',21:'publication-rename-noreplace'}.get(case,'selinux-context-open')
   error={13:5,14:13,18:5,20:38,21:95}.get(case,13)
   assert 'configuration phase='+phase+' errno='+str(error) in diagnostic
  results.append({'compiler':label,'case':case,'returncode':r.returncode,'expected':expected,'result':result,'stdout':r.stdout,'stderr':r.stderr})
record={'host_only':True,'mock_android_registry_host':True,'gpu_executed':False,'game_modules_executed':False,'lifecycle_cases':len(results),'results':results,'sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [p/'controller.c',p/'tests/controller_host.c',p/'tests/include/android/native_activity.h',p/'tests/include/android/native_window.h']}}
(out/'proof.json').write_text(json.dumps(record,indent=2)+'\n')
