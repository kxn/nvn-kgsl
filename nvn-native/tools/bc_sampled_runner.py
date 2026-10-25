"""Ordinary-UID NativeActivity BC qualification, standard verified APK transport."""
import hashlib,json,shlex,subprocess,time,uuid
from pathlib import Path
from run_android_probe import copy_package_uid,validate_copy_inventory
from validate_bc_sampled import compare,verify
def stopped(run,package):
 r=run('shell','pidof',package,check=False)
 return r.returncode==1 and not r.stdout.strip()and not r.stderr.strip(),dict(returncode=r.returncode,stdout=r.stdout,stderr=r.stderr)
def profile(record):
 c=record.get('bc_sampled_check')
 if not isinstance(c,dict)or c.get('kind')!='raw-bc-sampled-qualification-v1'or c.get('gpu_execution')is not False:raise RuntimeError('unknown BC qualifier build')
 verify(Path(c['normal_check']))
 for name in ('src/texture_bc_shadow.c','src/block_copy.c','src/compute.c','src/pipeline.c','tests/bc_sampled_probe.c','tests/bc_sampled_cases.h','android/bc_sampled_activity.c'):
  if name not in record['source']:raise RuntimeError('BC actual source closure incomplete '+name)
 return c
def exercise(args,build,record,adb,package,component,run):
 c=profile(record);names=[x['file']for x in c['cases']];sizes={x['file']:x['bytes']for x in c['cases']}
 folder=args.evidence.parent/(args.evidence.stem+'-readbacks')
 if folder.exists()or folder.is_symlink():raise RuntimeError('fresh BC readback directory required')
 args.evidence.parent.mkdir(parents=True,exist_ok=True);folder.mkdir()
 evidence=dict(scope='ordinary application UID raw-native BC1/2/3 GPU sampling; no NVN/game/display claim',build=record,bc_sampled_verified=False,stopped=False,images={})
 def save():args.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
 with args.evidence.open('x')as f:f.write(json.dumps(evidence,indent=2)+'\n')
 try:
  if args.install:
   r=run('install','--no-incremental','-r',str(build/'probe.apk'),timeout=180)
   if 'Success'not in r.stdout:raise RuntimeError('APK install not confirmed')
  installed=run('shell','pm','path',package).stdout.strip().splitlines()
  if len(installed)!=1 or not installed[0].startswith('package:'):raise RuntimeError('one installed probe APK required')
  installed_hash=run('shell','sha256sum',installed[0][8:]).stdout.split()[0]
  if installed_hash!=record['artifacts'][str(build/'probe.apk')]:raise RuntimeError('installed APK differs from verified build')
  evidence['installed_apk_sha256']=installed_hash
  run('shell','am','force-stop',package);ok,detail=stopped(run,package);evidence['initial_stop']=detail
  if not ok:raise RuntimeError('old process stop not confirmed')
  uid=copy_package_uid(run('shell','cmd','package','list','packages','-U',package).stdout,package)
  nonce=uuid.uuid4().hex;directory='bc-'+nonce;pending='bc-run-nonce.'+nonce+'.pending';evidence.update(run_nonce=nonce,run_directory=directory,package_uid=uid)
  script=f'set -eu; umask 077; test ! -e files/{directory}; test ! -L files/{directory}; rm -f files/result.json; set -C; printf %s {nonce} > files/{pending}; mv -f files/{pending} files/bc-run-nonce'
  run('shell',shlex.join(['run-as',package,'sh','-c',script]));evidence['launch']=run('shell','am','start','-W','-n',component).stdout
  deadline=time.monotonic()+180
  while True:
   r=run('shell','run-as',package,'cat','files/result.json',check=False)
   if r.returncode==0:result=json.loads(r.stdout);break
   if time.monotonic()>=deadline:raise RuntimeError('bounded BC qualification deadline expired')
   time.sleep(.1)
  evidence['result']=result
  if result.get('run_nonce')!=nonce or result.get('run_directory')!=directory or result.get('uid')!=uid:raise RuntimeError('result nonce/directory/ordinary UID mismatch')
  base='files/'+directory
  shape=run('shell','run-as',package,'stat','-c','%F',base,check=False)
  inv=run('shell','run-as',package,'find',base,'-mindepth','1','-print',check=False)
  regular=run('shell','run-as',package,'find',base,'-mindepth','1','-type','f','-print',check=False)
  evidence.update(copy_namespace_type=shape.stdout.strip(),copy_file_inventory=inv.stdout.splitlines(),copy_regular_inventory=regular.stdout.splitlines(),copy_inventory_returncodes=[shape.returncode,inv.returncode,regular.returncode])
  available=set(inv.stdout.splitlines())&set(regular.stdout.splitlines())
  for name in names:
   remote=base+'/'+name
   if remote not in available:continue
   meta=run('shell','run-as',package,'stat','-c','%s',remote,check=False)
   if meta.returncode or meta.stdout.strip()!=str(sizes[name]):
    evidence.setdefault('readback_errors',[]).append(dict(file=name,reason='stat/size mismatch',returncode=meta.returncode,stdout=meta.stdout));continue
   r=subprocess.run(adb+['exec-out','run-as',package,'cat',remote],capture_output=True,timeout=20)
   if r.returncode or len(r.stdout)!=sizes[name]:
    evidence.setdefault('readback_errors',[]).append(dict(file=name,reason='cat/size mismatch',returncode=r.returncode,bytes=len(r.stdout)));continue
   local=folder/name
   with local.open('xb')as f:f.write(r.stdout)
   evidence['images'][name]=dict(path=str(local.resolve()),bytes=len(r.stdout),sha256=hashlib.sha256(r.stdout).hexdigest())
  after=run('shell','run-as',package,'find',base,'-mindepth','1','-print',check=False);evidence.update(copy_file_inventory_after=after.stdout.splitlines(),copy_inventory_after_returncode=after.returncode)
  evidence['fingerprint']=run('shell','getprop','ro.build.fingerprint').stdout.strip();evidence['kernel']=run('shell','uname','-r').stdout.strip();save()
  validate_copy_inventory(evidence,directory,names)
  raw=folder/'result.json';raw.write_text(json.dumps(result,indent=2)+'\n')
  evidence['verification']=compare(Path(c['normal_check']),raw,folder);evidence['bc_sampled_verified']=True
 except BaseException as error:
  evidence['error']=str(error);save();raise
 finally:
  try:
   r=run('shell','am','force-stop',package,check=False);ok,detail=stopped(run,package)
   evidence.update(force_stop=dict(returncode=r.returncode,stdout=r.stdout,stderr=r.stderr),final_pidof=detail,stopped=r.returncode==0 and ok)
  except BaseException as error:evidence.update(stop_error=str(error),stopped=False)
  if not evidence['stopped']:evidence['bc_sampled_verified']=False
  save()
 if not evidence['stopped']:raise RuntimeError('BC process stop not confirmed')
 print(json.dumps(evidence['result'],indent=2))
