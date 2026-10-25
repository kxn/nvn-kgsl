from pathlib import Path
import hashlib,importlib.util,json,subprocess,sys,tempfile
from unittest import mock
root=Path(__file__).resolve().parents[4]
p=root/'.local/nvn-native/activity-stop-host-check'
p.mkdir(parents=True,exist_ok=True)
runner=root/'nvn-native/tools/run_d2r_activity.py'
spec=importlib.util.spec_from_file_location('actual_runner',runner);r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
results=[]
for label,code,stdout,stderr,wanted in [
 ('absent',1,b'',b'',[]),('one',0,b'123\n',b'',[123]),('multiple',0,b'123 456\n',b'',[123,456]),
 ('transport-empty',255,b'',b'',None),('transport-message',255,b'',b'error: device offline\n',None),
 ('zero-empty',0,b'',b'',None),('permission',1,b'',b'pidof: Permission denied\n',None),
 ('contradictory-no-match',1,b'123\n',b'',None),('text-in-stdout',0,b'error: device offline\n',b'',None),
 ('zero-pid',0,b'0\n',b'',None),('duplicate-pid',0,b'123 123\n',b'',None),('stderr-with-pid',0,b'123\n',b'warning\n',None)]:
 try:
  observed=r.parse_pidof_result(subprocess.CompletedProcess([],code,stdout,stderr));assert wanted is not None and observed==wanted
 except ValueError:assert wanted is None
 results.append({'parser':label,'passed':True})
# Exercise the real candidate main/finally, not a mirror of its stopping logic.
# Local build/provenance and APK contents are explicit mocks to reach finally;
# no original modules, compiler, Android process or ADB binary are executed.
for label,code,stdout,stderr,stopped in [('no-match',1,b'',b'',True),('offline',255,b'',b'error: device offline\n',False),('empty-transport-error',255,b'',b'',False),('empty-success',0,b'',b'',False),('permission',1,b'',b'permission denied\n',False),('remained',0,b'123\n',b'',False)]:
 with tempfile.TemporaryDirectory(dir=p) as directory:
  d=Path(directory);build=d/'build';build.mkdir();modules=d/'modules';modules.mkdir();(modules/'fake.fixture').write_bytes(b'data');evidence=d/'evidence.json'
  digest=hashlib.sha256(b'data').hexdigest();hostlib=d/'libdmhost_activity_kgsl.so';controller=d/'libnvn_d2r_activity_controller.so';apk=d/'fake.apk'
  host={'sources':{'mock':'mock'},'tools':{'mock':'mock'},'artifacts':{str(modules/'fake.fixture'):digest},'options':{'python':'MOCK-PYTHON'},'shared_library':str(hostlib),'modules':str(modules),'inputs':{},'rewrite_manifest':'MOCK-MANIFEST'}
  record={'operation':'d2r_activity_apk','options':{'host_abi':2,'package':r.PACKAGE,'controller_lib':'nvn_d2r_activity_controller','host_record':'MOCK-HOST','sdk':str(d/'sdk'),'ndk':'MOCK-NDK','keystore':'MOCK-KEY'},'sources':{},'tools':{},'compiled_inputs':{},'artifacts':{str(controller):digest},'d2r_activity_host':host,'host_verify_argv':[],'apk':str(apk)}
  (build/'build-record.json').write_text(json.dumps(record));calls=[]
  def process(argv,**kw):
   calls.append(argv)
   if 'features' in argv:return subprocess.CompletedProcess(argv,0,b'shell_v2\n',b'')
   if 'shell' in argv:
    text=argv[-1]
    if text=='pidof '+r.PACKAGE:return subprocess.CompletedProcess(argv,code,stdout,stderr)
    if text.startswith('cmd package path '):return subprocess.CompletedProcess(argv,1,b'',b'fixture stops after install\n')
   return subprocess.CompletedProcess(argv,0,b'',b'')
  with mock.patch.object(sys,'argv',['run_d2r_activity.py','--serial','MOCK-NO-ADB','--build',str(build),'--evidence',str(evidence),'--stage','1','--install']),mock.patch.object(r,'checked'),mock.patch.object(r,'sha',return_value=digest),mock.patch.object(r.subprocess,'run',side_effect=process),mock.patch.object(r.time,'sleep'),mock.patch.object(r.zipfile,'ZipFile') as archive:
   archive.return_value.__enter__.return_value.namelist.return_value=['lib/arm64-v8a/'+hostlib.name,'lib/arm64-v8a/'+controller.name]
   archive.return_value.__enter__.return_value.read.return_value=b'data'
   result=r.main()
  report=json.loads(evidence.read_text());assert result==1 and report.get('finally_force_stopped',False) is stopped and report['passed'] is False
  if not stopped:assert 'finally_stop_error' in report
  assert any(c[-1]=='pidof '+r.PACKAGE for c in calls)
  results.append({'candidate_main_finally':label,'finally_force_stopped':report.get('finally_force_stopped',False),'finally_stop_error':report.get('finally_stop_error'),'passed':True})
record={'scope':'Actual runner, HOST-only. ADB/build/APK inputs explicitly mocked only in main/finally fixture; no real subprocess invoked.','passed':True,'checks':len(results),'results':results,'sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [runner,Path(__file__)]}}
(p/'proof.json').write_text(json.dumps(record,indent=2)+'\n');print('PASS',len(results))
