from pathlib import Path
import importlib.util,json,hashlib,tempfile,sys,subprocess,shlex
from unittest import mock
root=Path.cwd();p=Path(__file__).resolve().parents[1];out=root/'.local/nvn-native/parallel/d2r-activity-runner-check';out.mkdir(exist_ok=True)
def load(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
r=load('activity_runner_candidate',root/'nvn-native/tools/run_d2r_activity.py');cli=load('cli_classify',root/'nvn-native/tools/run_d2r_host.py')
good={'stage_returned':True,'return_code':0,'errno':0};terminal='[native NVN D2R] nvnDeviceGetImageHandle: unavailable; terminating execution (exit 78)\n'
fixtures=[
 ('sdk-return','sdk-init',good,True,False,'{"stage":"sdk-initialized"}','',True,False),
 ('ctors-return','constructors',good,True,False,'{"stage":"constructors-completed"}','',True,False),
 ('run-return','run',good,True,False,'{"stage":"nnMain-returned"}','[dm] native >>> nnMain\n',True,False),
 ('resource-exit0','run',good,True,False,'{"stage":"nnMain-returned"}','[guest] [tact]: Failed to load build config. Error(7) : not available\n[guest] [Init]: Failed to initialize data (corrupted?)',False,False),
 ('dead-boundary','run',None,False,False,'',terminal,False,True),
 ('alive-marker','run',None,True,False,'',terminal,False,False),
 ('unknown-death','run',None,None,False,'',terminal,False,False),
 ('return-with-terminal','run',good,False,False,'{"stage":"nnMain-returned"}',terminal,False,True),
 ('sdk-return-with-terminal','sdk-init',good,False,False,'{"stage":"sdk-initialized"}',terminal,False,True),
 ('return-with-terminal-alive','run',good,True,False,'{"stage":"nnMain-returned"}',terminal,False,False),
 ('missing-result','sdk-init',None,False,False,'{"stage":"sdk-initialized"}','',False,False),
 ('timeout','run',None,False,True,'',terminal,False,False),
 ('controller-error','constructors',{'stage_returned':True,'return_code':0,'errno':95},True,False,'{"stage":"constructors-completed"}','',False,False),
 ('guest-error','run',good,True,False,'{"stage":"nnMain-returned"}','[guest] Failed unknown initialization Error(9)',False,False),
 ('no-stage-marker','sdk-init',good,True,False,'','',False,False),
 ('controller-death79','run',None,False,False,'','[D2R Activity controller] configuration errno=22\n',False,False),
]
results=[]
for name,stage,status,alive,timeout,stdout,stderr,passed,boundary in fixtures:
 result=r.classify_activity(stage,status,alive,timeout,stdout,stderr,cli.classify)
 assert result['passed'] is passed and result['unsupported_boundary'] is boundary,(name,result)
 assert result['process_exit_code'] is None and result['first_frame'] is False
 results.append({'name':name,'result':result})
# Exercise the actual shell-v2 reader, including the observed exec-out rc0
# error-in-stdout trap. No ADB binary is called by these transport fixtures.
relative='files/d2r-activity/runs/'+'a'*32+'/start.json'
missing=('cat: '+relative+': No such file or directory\n').encode()
permission=('cat: '+relative+': Permission denied\n').encode()
for name,code,stdout,stderr,expected in [
 ('json',0,b'{"schema":1}\n',b'','bytes'),
 ('empty-log',0,b'',b'','bytes'),
 ('missing',1,b'',missing,'missing'),
 ('permission',1,b'',permission,'fatal'),
 ('legacy-exec-out-missing',0,missing,b'','fatal'),
 ('permission-with-zero-exit',0,b'',permission,'fatal'),
 ('not-directory',1,b'',b'cat: Not a directory\n','fatal'),
 ('run-as-denial',1,b'',b'run-as: package not debuggable\n','fatal'),
 ('partial-file-failed',1,b'{"schema":',b'cat: I/O error\n','fatal'),
]:
 calls=[]
 def transport(argv,**kw):
  calls.append((argv,kw));return subprocess.CompletedProcess(argv,code,stdout,stderr)
 try:
  raw=r.read_app_file(transport,['MOCK-ADB','-s','SERIAL'],relative)
  assert expected!='fatal'
  assert (raw is None) is (expected=='missing')
  if expected=='bytes':assert raw==stdout
 except ValueError:assert expected=='fatal'
 assert len(calls)==1 and calls[0][0]==['MOCK-ADB','-s','SERIAL','shell','-T',shlex.join(['run-as',r.PACKAGE,'cat',relative])]
 results.append({'read_transport':name,'expected':expected,'adb_invoked':False})
for broken in ('sources','tools','compiled_inputs','artifacts'):
 with tempfile.TemporaryDirectory(dir=out) as temp:
  temp=Path(temp);build=temp/'build';build.mkdir();inventory={}
  for key in ('sources','tools','compiled_inputs','artifacts'):
   f=temp/key;f.write_text('stable'+key);inventory[key]={str(f):hashlib.sha256(f.read_bytes()).hexdigest()}
  (temp/broken).write_text('corrupted')
  record={'operation':'d2r_activity_apk','options':{'host_abi':2,'package':'org.nvn.nativeprobe','controller_lib':'nvn_d2r_activity_controller'},**inventory}
  (build/'build-record.json').write_text(json.dumps(record));evidence=temp/'evidence.json'
  with mock.patch.object(sys,'argv',['run_d2r_activity.py','--serial','MOCK-NO-ADB','--build',str(build),'--evidence',str(evidence),'--stage','1']),mock.patch.object(r.subprocess,'run',side_effect=AssertionError('No command/ADB allowed before failed provenance')) as calls:
   code=r.main();assert code==1 and calls.call_count==0
  report=json.loads(evidence.read_text());assert report['adb_invoked'] is False and report['passed'] is False and 'integrity mismatch' in report['error']
  results.append({'early_rejection':broken,'subprocess_calls':0,'adb_invoked':False})
proof={'host_only':True,'adb_invoked':False,'gpu_executed':False,'game_modules_executed':False,'checks':len(results),'results':results,'sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [root/'nvn-native/tools/run_d2r_activity.py',Path(__file__),root/'nvn-native/tools/run_d2r_host.py']}}
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('PASS',len(results))
