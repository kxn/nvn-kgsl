"""Mocked runner orchestration only; never invokes adb, host or game modules."""
import hashlib, importlib.util, json, shlex, subprocess, sys, tempfile, zipfile
from pathlib import Path
from unittest.mock import patch
root=Path(__file__).resolve().parents[2]; runner=root/'nvn-native/tools/run_d2r_host.py'; out=root/'.local/nvn-native/d2r-runner-regression'
out.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('runner',runner); mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
sha=mod.sha
real_run=subprocess.run
proof={'scope':'Mocked command/cleanup/integrity behavior only; no ADB or host/game CPU execution','runner_sha256':sha(runner),'cases':[]}
assert mod.classify('sdk-init',0,False,'{"stage":"sdk-initialized"}','')['stage_completed']
assert not mod.classify('sdk-init',0,False,'','')['stage_completed']
assert not mod.classify('run',78,False,'','')['unsupported_boundary']
observed='[native NVN D2R] nvnDeviceBuilderSetFlags: builder=0x1234 flags=0x00000100\n'
assert not mod.classify('run',78,False,'',observed)['unsupported_boundary']
assert mod.classify('run',78,False,'',observed)['first_named_nvn_error'] is None
assert mod.classify('run',78,False,'',observed)['observed_device_flags']==[0x100]
assert mod.classify('run',78,False,'','[dm] native >>> nnMain\n[native NVN D2R] unsupported call: nvnDeviceBuilderSetFlags; errno=38; phase=call; terminating execution (exit 78)')['unsupported_boundary']
with tempfile.TemporaryDirectory(dir=out) as temporary:
 base=Path(temporary); apkbuild=base/'apk';apkbuild.mkdir();hostout=base/'host';modules=hostout/'modules';modules.mkdir(parents=True)
 shell_fixture=base/'command==.sh';shell_fixture.write_text('#!/bin/sh\nprintf \'%s\\n\' "$DM_ROOT" "$1" "$2"\n');shell_fixture.chmod(0o700)
 exe=hostout/'host';exe.write_text('MOCK HOST - NOT EXECUTABLE\n')
 inputs={}
 for name in ('main','sdk','subsdk0'):
  original=base/(name+'.fixture');original.write_text('MOCK INPUT '+name)
  inputs[name]={'path':str(original),'bytes':original.stat().st_size,'sha256':sha(original)}
  for suffix in ('.patched.bin','.fixups.tsv','.gates.tsv'):(modules/(name+suffix)).write_text('MOCK MODULE '+name+suffix)
 manifest={'modules':{k:{'recipe':{'input':{'sha256':v['sha256']}}} for k,v in inputs.items()}}
 (modules/'rewrite-manifest.json').write_text(json.dumps(manifest))
 host={'options':{'host_abi':1,'rewrite_abi':'elf','rules':'native','private_gates':'1','abi':'arm64-v8a','execute':False,'deploy':False,'python':sys.executable,'exefs':str(base),'ndk':str(base),'jobs':4},'modules':str(modules),'executable':str(exe),'inputs':inputs,'sources':{str(runner):sha(runner)},'tools':{str(Path(sys.executable).resolve()):sha(Path(sys.executable).resolve())},'artifacts':{str(p):sha(p) for p in [exe,*modules.iterdir()]}}
 (hostout/'build-record.json').write_text(json.dumps(host))
 apk=apkbuild/'probe.apk'
 with zipfile.ZipFile(apk,'w') as z:z.write(exe,mod.HOST_MEMBER)
 record={'d2r_host':host,'source':{'tools/run_d2r_host.py':sha(runner)},'tools':host['tools'],'artifacts':{str(apk):sha(apk)}}
 (apkbuild/'build-record.json').write_text(json.dumps(record))
 for mode in ('local-romfs','device-romfs','device-source-failure','sdk-success','sdk-legacy','resource-exit0','constructors-failure','nvn-boundary','timeout',
              'reuse-match','reuse-missing','reuse-extra','reuse-symlink','reuse-root-symlink','reuse-hash-mismatch'):
  stage='sdk-init' if mode in ('sdk-success','sdk-legacy','device-romfs','reuse-match','local-romfs') else 'constructors' if mode=='constructors-failure' else 'run'
  ep=out/(mode+'.json');ep.unlink(missing_ok=True)
  state={'alive':False,'remote_calls':0,'kills':[],'resource_pipes':0,
         'lock_held':False,'host_exec_calls':0,'romfs_mutations':[],'romfs_hashes':0,'reuse_inventory_checks':0}
  resources=base/'romfs';(resources/'data').mkdir(parents=True,exist_ok=True)
  (resources/'data/.build.config').write_text('MOCK PRIVATE CONFIG')
  (resources/'data/sample.data').write_text('MOCK PRIVATE RESOURCE')
  resource_hashes={p.name:sha(p) for p in resources.rglob('*') if p.is_file()}
  def result(argv,code=0,stdout='',stderr=''):
   return subprocess.CompletedProcess(argv,code,stdout.encode(),stderr.encode())
  def mocked(argv,**kwargs):
   if argv[0]==sys.executable:
    assert '--verify' in argv
    return result(argv,stdout='MOCK canonical verification\n')
   assert argv[0]=='/bin/true' and argv[1:3]==['-s','MOCK-NOT-A-DEVICE'],argv
   if argv[3]=='install':return result(argv,stdout='Success\n')
   assert argv[3]=='shell';a=shlex.split(argv[4]);state['remote_calls']+=1
   ordinary=a[:2]==['run-as',mod.PACKAGE]
   if ordinary:a=a[2:]
   if a[:2]==['pm','path']:return result(argv,stdout='package:/data/app/~~mock==/org.nvn.nativeprobe-test==/base.apk\n')
   if a[:2]==['dumpsys','package']:return result(argv,stdout='legacyNativeLibraryDir=/data/app/~~mock==/org.nvn.nativeprobe-test==/lib\nprimaryCpuAbi=arm64-v8a\n' if mode=='sdk-legacy' else 'nativeLibraryDir=/data/app/~~mock==/org.nvn.nativeprobe-test==/lib/arm64\n')
   if a[:3]==['cmd','package','list']:return result(argv,stdout='package:org.nvn.nativeprobe uid:10123\n')
   if a[0]=='getprop':return result(argv,stdout='MOCK fingerprint\n')
   if a[0]=='uname':return result(argv,stdout='MOCK kernel\n')
   if a[0]=='sha256sum':
    path=Path(a[1]);expected=sha(apk) if path.name=='base.apk' else sha(exe) if path.name=='libnvn_d2r_host.so' else resource_hashes[path.name] if path.name in resource_hashes else host['artifacts'][str(modules/path.name)]
    if path.name in resource_hashes:
     state['romfs_hashes']+=1
     if mode.startswith('reuse-'):assert state['lock_held']
     if mode=='reuse-hash-mismatch' and path.name=='sample.data':expected='0'*64
    return result(argv,stdout=expected+'  '+a[1]+'\n')
   if a[0]=='find':
    assert ordinary and state['lock_held'] and a[1].endswith('/game/romfs')
    state['reuse_inventory_checks']+=1
    if a[2:]==['!', '-type', 'f', '!', '-type', 'd', '-print0']:
     return result(argv,stdout=a[1]+'/data/link\0' if mode=='reuse-symlink' else '')
    assert a[2:]==['-type','f','-print0']
    names=['data/.build.config','data/sample.data']
    if mode=='reuse-missing':names.pop()
    if mode=='reuse-extra':names.append('unexpected.data')
    return result(argv,stdout=''.join(a[1]+'/'+name+'\0' for name in names))
   if a[:2]==['id','-u']:assert ordinary;return result(argv,stdout='10123\n')
   if a==['pwd']:return result(argv,stdout='/data/user/0/org.nvn.nativeprobe\n')
   if a[:2]==['sh','-c'] and a[2].startswith('set -o pipefail; '):
    assert not ordinary and 'run-as com.d2rmobile.nativeport cat files/runtime/RomFS/data/' in a[2] and '| run-as org.nvn.nativeprobe sh -c ' in a[2]
    assert kwargs['timeout']==180
    state['resource_pipes']+=1
    return result(argv,code=1 if mode=='device-source-failure' else 0)
   if a[:2]==['sh','-c']:
    if a[2]=='id; cat /proc/self/attr/current':return result(argv,stdout='uid=10123\nu:r:runas_app:s0\n')
    assert 'child=$!' in a[2] and '--'+stage in a[2] and 'DM_ROMFS=' in a[2]
    state['host_exec_calls']+=1
    if mode.startswith('reuse-'):
     assert mode=='reuse-match' and state['lock_held'] and state['romfs_hashes']==2 and state['reuse_inventory_checks']==4
    commandline=a[2].split('|| exit 125; ',1)[1].split(' >',1)[0]
    tokens=shlex.split(commandline)
    assert all(tokens[i].startswith(k+'=') for i,k in enumerate(('DM_ROOT','DM_ROMFS','DM_SAVE','DM_TEMP','DM_IMPORTS','DM_BOOTPROF','DM_FS')))
    assert tokens[7]=='/data/app/~~mock==/org.nvn.nativeprobe-test==/lib/arm64/libnvn_d2r_host.so'
    assert tokens[8].endswith('/modules') and tokens[9]=='--'+stage
    localcommand=commandline.replace(shlex.quote(tokens[7]),shlex.quote(str(shell_fixture)),1)
    checked=real_run(['/bin/sh','-c',localcommand],capture_output=True,text=True)
    assert checked.returncode==0 and checked.stdout.splitlines()==[tokens[0].split('=',1)[1],tokens[8],tokens[9]],checked
    if mode=='timeout':state['alive']=True;raise subprocess.TimeoutExpired(argv,20)
    return result(argv,code=0 if mode in ('sdk-success','sdk-legacy','resource-exit0','device-romfs','reuse-match','local-romfs') else 78 if mode=='nvn-boundary' else 134)
   if a[:2]==['readlink','-f']:return result(argv,stdout='/data/app/~~mock==/org.nvn.nativeprobe-test==/lib/arm64/libnvn_d2r_host.so\n')
   if a[0]=='readlink':return result(argv,stdout='/data/app/~~mock==/org.nvn.nativeprobe-test==/lib/arm64/libnvn_d2r_host.so\n') if state['alive'] else result(argv,code=1)
   if a[0]=='kill':
    if a[1]=='-0':return result(argv,code=0 if state['alive'] else 1)
    assert a[-1]=='12345';state['kills'].append(a[1]);state['alive']=False;return result(argv)
   if a[0]=='cat':
    name=Path(a[1]).name
    if name=='pid':return result(argv,stdout='12345\n')
    if name=='start-stat' or a[1]=='/proc/12345/stat':return result(argv,stdout='12345 (MOCK) '+' '.join(['S']+['0']*18+['777'])+'\n')
    if name=='child-context':return result(argv,stdout='u:r:runas_app:s0\n')
    if name=='exit':return result(argv,stdout=('0' if mode in ('sdk-success','sdk-legacy','resource-exit0','device-romfs','reuse-match','local-romfs') else '78' if mode=='nvn-boundary' else '137' if mode=='timeout' else '134')+'\n')
    if name=='stdout' and mode=='resource-exit0':return result(argv,stdout='{"backend":"native-arm64","stage":"nnMain-returned"}\n')
    if name=='stdout':return result(argv,stdout='{"backend":"native-arm64","stage":"sdk-initialized"}\n' if mode in ('sdk-success','sdk-legacy','device-romfs','reuse-match','local-romfs') else '')
    if name=='stderr' and mode=='resource-exit0':return result(argv,stdout=(root/'docs/research/nvn-turnip-study-20261023/a650-d2r-run-stderr.log').read_text())
    if name=='stderr':return result(argv,stdout='[dm] native >>> nnMain\n[native NVN D2R] unsupported call: nvnDeviceBuilderSetFlags; errno=38; phase=call; terminating execution (exit 78)\n' if mode=='nvn-boundary' else '[dm] FATAL MOCK platform gap\n' if mode=='constructors-failure' else '')
   if a[0] in ('mkdir','rmdir','cp','mv','rm','test'):
    if a[0]=='mkdir' and any(value.endswith('/runner.lock') for value in a[1:]):state['lock_held']=True
    if a[0]=='rmdir' and any(value.endswith('/runner.lock') for value in a[1:]):state['lock_held']=False
    if a[0] in ('mkdir','cp','mv','rm') and any('/game/romfs' in value for value in a[1:]):
     state['romfs_mutations'].append(a)
     assert not mode.startswith('reuse-'),a
    if a[:3]==['test','!','-L'] and a[-1].endswith('/game/romfs') and mode=='reuse-root-symlink':return result(argv,code=1)
    return result(argv)
   raise AssertionError(a)
  # adb push is a binary transfer request, also mocked; handle before shell parsing.
  base_mock=mocked
  def fake(argv,**kw):
   if len(argv)>3 and argv[3]=='push':
    if 'romfs-' in argv[-1]:
     state['romfs_mutations'].append(list(argv))
     assert not mode.startswith('reuse-'),argv
    return result(argv,stdout='MOCK pushed\n')
   return base_mock(argv,**kw)
  with patch.object(sys,'argv',[str(runner),'--serial','MOCK-NOT-A-DEVICE','--build',str(apkbuild),'--evidence',str(ep),'--stage',stage,'--install']+(['--romfs',str(resources),'--device-romfs-source','com.d2rmobile.nativeport:files/runtime/RomFS'] if mode.startswith('device-') else ['--romfs',str(resources),'--reuse-romfs'] if mode.startswith('reuse-') else ['--romfs',str(resources)] if mode=='local-romfs' else [])),patch.object(mod.shutil,'which',return_value='/bin/true'),patch.object(mod.subprocess,'run',side_effect=fake):
   code=mod.main()
  evidence=json.loads(ep.read_text())
  if mode.startswith('reuse-'):
   assert not state['romfs_mutations'] and not state['resource_pipes']
   if mode!='reuse-match':
    assert evidence['failed_phase']=='romfs-reuse-verification' and not evidence['host_execution_attempted'] and code==1
    assert state['host_exec_calls']==0 and not evidence['romfs']['reused'] and not state['lock_held']
    proof['cases'].append({'name':mode,'rejected_before_host':True,'host_exec_calls':0,
      'romfs_unchanged':True,'error':evidence['error'],'verified_files':evidence['romfs']['verified_files']})
    continue
   assert evidence['romfs']['reused'] and evidence['romfs']['reuse_requested']
   assert evidence['romfs']['verified_files']==2 and evidence['romfs']['verified_bytes']==sum(p.stat().st_size for p in resources.rglob('*') if p.is_file())
   assert state['host_exec_calls']==1 and state['reuse_inventory_checks']==4
   proof['reuse_matching']={'verified_files':evidence['romfs']['verified_files'],'verified_bytes':evidence['romfs']['verified_bytes'],
     'under_runner_lock':True,'romfs_unchanged':True,'host_exec_calls':state['host_exec_calls']}
  if mode=='local-romfs':
   assert not evidence['romfs']['reuse_requested'] and not evidence['romfs']['reused']
   assert evidence['romfs']['verified_files']==2 and evidence['romfs']['verified_bytes']==sum(p.stat().st_size for p in resources.rglob('*') if p.is_file())
   assert len([a for a in state['romfs_mutations'] if len(a)>3 and a[3]=='push'])==2
   assert len([a for a in state['romfs_mutations'] if a[0]=='cp'])==2
   assert any(a[:2]==['rm','-rf'] for a in state['romfs_mutations'])
   proof['default_local_copy']={'copied_files':2,'verified_files':2,'verified_bytes':evidence['romfs']['verified_bytes'],'reused':False}
  if mode=='device-source-failure':
   assert evidence['failed_phase']=='isolated-deployment' and not evidence['host_execution_attempted'] and code==1 and state['resource_pipes']==1
   proof['cases'].append({'name':mode,'source_failure_rejected_before_host':True});continue
  if mode=='device-romfs':assert state['resource_pipes']==2 and evidence['romfs']['device_source']['package']=='com.d2rmobile.nativeport'
  assert evidence.get('error') is None,evidence.get('error')
  assert evidence['cleanup']['confirmed_exited']
  assert code==(0 if mode in ('sdk-success','sdk-legacy','device-romfs','reuse-match','local-romfs') else 1)
  assert evidence['first_frame'] is False
  if mode=='resource-exit0':assert evidence['stage_completed'] and evidence['resource_failure'] and evidence['guest_initialization_error'] and not evidence['passed']
  assert evidence['outcome']=={'local-romfs':'stage-completed','reuse-match':'stage-completed','device-romfs':'stage-completed','sdk-success':'stage-completed','sdk-legacy':'stage-completed','resource-exit0':'guest-resource-failure','constructors-failure':'failed','nvn-boundary':'unsupported-nvn-boundary','timeout':'timeout'}[mode]
  assert state['kills']==(['-TERM'] if mode=='timeout' else [])
  assert not state['alive']
  proof['cases'].append({'name':mode,'equals_install_path_shell_assignments_verified':True,'runner_exit':code,'outcome':evidence['outcome'],'cleanup':evidence['cleanup'],'mock_remote_calls':state['remote_calls']})
 # Invalid reuse options must reject before any subprocess, including mocked host verification.
 for label,extra in (('reuse-without-local',['--reuse-romfs']),
                     ('reuse-conflicting-source',['--reuse-romfs','--romfs',str(resources),'--device-romfs-source','com.source:files/RomFS'])):
  ep=out/(label+'.json');ep.unlink(missing_ok=True)
  with patch.object(sys,'argv',[str(runner),'--serial','MOCK-NOT-A-DEVICE','--build',str(apkbuild),'--evidence',str(ep),'--stage','run']+extra),patch.object(mod.subprocess,'run',side_effect=AssertionError('invalid options cannot execute any subprocess')):
   try:mod.main()
   except SystemExit as error:assert error.code==2
   else:raise AssertionError('invalid reuse options accepted')
  assert not ep.exists()
  proof['cases'].append({'name':label,'rejected_before_subprocess':True,'host_execution_attempted':False})
 # A corrupted APK/source must stop before any ADB request or host execution.
 ep=out/'integrity-rejection.json';ep.unlink(missing_ok=True);apk.write_bytes(apk.read_bytes()+b'corrupt')
 with patch.object(sys,'argv',[str(runner),'--serial','MOCK-NOT-A-DEVICE','--build',str(apkbuild),'--evidence',str(ep),'--stage','run']),patch.object(mod.subprocess,'run',side_effect=AssertionError('preflight must reject before any subprocess')):
  assert mod.main()==1
 rejected=json.loads(ep.read_text());assert rejected['failed_phase']=='local-preflight' and not rejected['host_execution_attempted']
 proof['cases'].append({'name':'corrupt-apk-before-ADB','outcome':'rejected','host_execution_attempted':False})
for invalid in ('org.nvn.nativeprobe:files/runtime/RomFS','com.source:files/\x7fsecret','bad;pkg:files/runtime/RomFS','com.source:/absolute','com.source:../save','com.source:files/../save','com.source:files/\nsecret'):
 try:mod.device_romfs_source(invalid)
 except Exception:pass
 else:raise AssertionError(invalid)
proof['invalid_device_sources_rejected']=True
actual_stderr=root/'docs/research/nvn-turnip-study-20261023/a650-d2r-run-stderr.log'
actual_stdout=root/'docs/research/nvn-turnip-study-20261023/a650-d2r-run-stdout.log'
classified=mod.classify('run',0,False,actual_stdout.read_text(),actual_stderr.read_text())
assert classified['stage_completed'] and classified['resource_failure'] and classified['guest_initialization_error'] and not classified['passed'] and classified['outcome']=='guest-resource-failure' and classified['first_frame'] is False
unknown=mod.classify('run',0,False,'{"stage":"nnMain-returned"}','[guest] Unknown subsystem failed with Error(9)')
assert not unknown['passed'] and unknown['outcome']=='guest-error'
proof['actual_log_classification']={'stderr_sha256':sha(actual_stderr),'stdout_sha256':sha(actual_stdout),'classification':classified,'changed_original_evidence':False}
proof['unknown_guest_error_rejected']=True
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
