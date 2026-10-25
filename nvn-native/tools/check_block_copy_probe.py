#!/usr/bin/env python3
"""HOST failure/transport/oracle tests + actual ARM64 block qualifier link. No ADB."""
import argparse,contextlib,hashlib,importlib.util,io,json,os,subprocess,sys
from pathlib import Path
from types import SimpleNamespace

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,kernel,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored check directory required')
 out.mkdir(parents=True);r=root/'nvn-native';sys.path.insert(0,str(r/'tools'));bundle=module(r/'tools/block_copy_bundle.py','block_bundle');header,record,inputs=bundle.build(kernel,out/'bundle',root=root)
 commands=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1'));commands.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise RuntimeError(p.stderr)
  return p.stdout
 fixture=r/'tests/d2r_bridge/block_copy_probe_failure.c';defs=['-DNN_BLOCK_PROBE_SOURCE='+json.dumps(str(r/'tests/block_copy_probe.c')),'-DNVN_BLOCK_COPY_HEADER='+json.dumps(str(header))];flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'include')];sources=[r/'src'/name for name in ['block_copy.c','texture_source.c','compute.c']];host=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('failure-'+cc);run([cc,*flags,*defs,*san,fixture,*sources,'-o',exe]);actual=json.loads(run([exe]));assert actual['passed']is True and actual['HOST_failure_checks']==44 and actual['actual_GPU_submits']==0 and actual['ambiguous_owners_retained']==2;host.append(actual)
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';run([cc,*flags,*defs,'-fPIC','-shared',r/'tests/block_copy_probe.c',r/'android/block_copy_activity.c',*sources,r/'src/kgsl.c','-I'+str(r/'third_party/kgsl'),'-landroid','-Wl,--no-undefined','-o',out/'block-probe-real-arm64.so'])
 # CPU oracle/collector fixtures are synthetic, not actual GPU output.
 import block_copy_validation as oracle
 import block_copy_runner as transport
 oracle.validate_cases(record['cases']);captured={name:oracle.expected(n,repeat)[0]for n in range(148)for repeat,name in enumerate(oracle.NAMES[n*2:n*2+2])};total=sum(map(len,captured.values()));active=sum(oracle.expected(n,repeat)[1]for n in range(148)for repeat in range(2));reads=sum(oracle.expected(n,repeat)[2]for n in range(148)for repeat in range(2));nonce='a'*32;uid=10234
 data=dict(schema=1,operation='block_copy',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submission='accepted',cases=148,formats=37,completed=296,submissions=296,finishes=296,replays=148,files=296,inflight_destroy_rejections=296,sequence_word_checks=296,nvn_api=False,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,bad_case=0xffffffff,bad_repeat=0xffffffff,bad_bo=0xffffffff,bad_offset=0xffffffffffffffff,run_nonce=nonce,run_directory='block-'+nonce,data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,aux_readonly_byte_checks=(record['code_bo_bytes']+4096+8192-4)*296)
 assert oracle.validate(data,captured,nonce,uid,record)['passed']is True;tests=1
 def reject(fn):
  nonlocal tests
  try:fn()
  except (RuntimeError,ValueError):tests+=1
  else:raise AssertionError('negative was accepted')
 bad_cases=json.loads(json.dumps(record['cases']));bad_cases[0]['groups'][0]=True;reject(lambda:oracle.validate_cases(bad_cases))
 for key,value in [('uid',0),('passed',1),('cleanup_errno',16),('submissions',True),('finishes',295),('resources_retained',True),('run_nonce','b'*32),('aux_readonly_byte_checks',0),('active_byte_checks',active-1),('BC_decode',True)]:
  bad=dict(data);bad[key]=value;reject(lambda bad=bad:oracle.validate(bad,captured,nonce,uid,record))
 # Corrupt source data, a partial destination neighbor, destination active data,
 # and allocation tail. All must be independently rejected at exact bytes.
 for n,index in [(0,512),(2,0),(2,-1),(3,512)]:
  name=oracle.NAMES[n*2];bad=dict(captured);b=bytearray(bad[name]);b[index]^=1;bad[name]=bytes(b);reject(lambda bad=bad:oracle.validate(data,bad,nonce,uid,record))
 bad=dict(captured);bad.pop(oracle.NAMES[-1]);reject(lambda:oracle.validate(data,bad,nonce,uid,record));bad=dict(captured);bad['extra']=b'x';reject(lambda:oracle.validate(data,bad,nonce,uid,record));reject(lambda:oracle.parse_result('{"passed":true,"passed":false}'));reject(lambda:oracle.parse_result('{"x":NaN}'))
 for rc,stdout,stderr,want in [(1,'','',True),(1,'','transport',False),(0,'','',False),(255,'','',False),(1,'123','',False)]:
  assert transport.stopped(lambda *a,**k:SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr),'org.nvn.nativeprobe')[0]is want;tests+=1
 minimal=dict(block_copy_check={k:record[k]for k in ['schema','kind','gpu_execution','cases','code_bo_bytes']},source={k:'fixture'for k in ['src/compute.c','src/texture_source.c','src/block_copy.c','tests/block_copy_probe_cases.h','tests/block_copy_probe.c','android/block_copy_activity.c']},artifacts={str(out/'fake-build/probe.apk'):'f'*64},texture_descriptor_flags=0)
 # All run/transport/collection calls are mocks. Any unexpected call aborts.
 old_collect=transport.collect_copy_readbacks;old_uuid=transport.uuid.uuid4
 transport.uuid.uuid4=lambda:SimpleNamespace(hex=nonce)
 try:
  for case in ['success','primary-failure','stop-transport-failure','apk-mismatch','inventory-extra']:
   evidence=out/(case+'.json');args=SimpleNamespace(evidence=evidence,install=False);calls=[];result=dict(data)
   if case=='primary-failure':result.update(passed=False,stage='finish',errno=35,resources_retained=True,cleanup_errno=16,completed=0,files=0)
   pidofs=0
   def fake(*argv,**kwargs):
    nonlocal pidofs
    calls.append(argv)
    if argv[:3]==('shell','pm','path'):stdout='package:/fake/base.apk\n';rc=0;stderr=''
    elif argv[:2]==('shell','sha256sum'):stdout=('e'if case=='apk-mismatch'else'f')*64+'  /fake/base.apk\n';rc=0;stderr=''
    elif argv[:3]==('shell','am','force-stop'):stdout='';rc=0;stderr=''
    elif argv[:2]==('shell','pidof'):
     pidofs+=1;stdout='';rc=1;stderr='transport failed'if case=='stop-transport-failure'and pidofs>1 else''
    elif argv[:4]==('shell','cmd','package','list'):stdout='package:org.nvn.nativeprobe uid:10234\n';rc=0;stderr=''
    elif argv[:3]==('shell','am','start'):stdout='Status: ok\n';rc=0;stderr=''
    elif argv[:2]==('shell','run-as'):stdout=json.dumps(result);rc=0;stderr=''
    elif argv[:2]==('shell','getprop'):stdout='synthetic-fingerprint';rc=0;stderr=''
    elif argv[:2]==('shell','uname'):stdout='synthetic-kernel';rc=0;stderr=''
    elif argv[0]=='shell'and len(argv)==2 and 'block-run-nonce'in argv[1]:stdout='';rc=0;stderr=''
    else:raise AssertionError('unexpected mocked transport '+str(argv))
    return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   def collect(run,adb,package,directory,names,path,ev):
    assert adb==['NO_REAL_ADB'];inventory=['files/'+directory+'/'+name for name in oracle.NAMES]
    if case=='inventory-extra':inventory.append('files/'+directory+'/extra')
    ev.update(copy_namespace_type='directory',copy_file_inventory=inventory,copy_regular_inventory=inventory,copy_file_inventory_after=inventory,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0)
    return {}if case=='primary-failure'else captured
   transport.collect_copy_readbacks=collect
   try:
    with contextlib.redirect_stdout(io.StringIO()):transport.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
   except RuntimeError as error:
    assert case!='success'
    if case=='primary-failure':assert 'stage=finish errno=35'in str(error)
   else:assert case=='success'
   observed=json.loads(evidence.read_text());assert observed['block_copy_verified']is(case=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);tests+=1
 finally:transport.collect_copy_readbacks=old_collect;transport.uuid.uuid4=old_uuid
 for mode in ['--nvn-api','--texture-conversion-check','--compute-indirect','--d2r-host-record']:
  argv=[sys.executable,str(r/'tools/build_android_probe.py'),'--ndk',str(ndk),'--sdk',str(ndk.parents[1]),'--out',str(out/'MUST_NOT_CREATE'),'--block-copy-check',str(kernel),mode]
  if mode.endswith('check')or mode.endswith('record'):argv.append('missing')
  q=subprocess.run(argv,capture_output=True,text=True);assert q.returncode==2 and '--block-copy-check is an independent'in q.stderr and not(out/'MUST_NOT_CREATE').exists();tests+=1
 # Hash every source in this isolated candidate; later runner/source changes
 # correctly invalidate this proof and any normal APK built before them.
 for p in sorted(r.rglob('*')):
  if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 inputs[str(kernel)]=sha(kernel);inputs[str(cc)]=sha(cc)
 for p,h in inputs.items():
  if sha(p)!=h:raise RuntimeError('input changed during check '+p)
 proof=dict(schema=1,passed=True,gpu_execution=False,android_execution=False,host_failure=host,synthetic_oracle_transport_checks=tests,synthetic_raw_bytes=total,scope='HOST failure ownership, independent math/transport fixtures, actual ARM64 complete backend/probe/Activity link; no GPU output or deployment',inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});result=out/'check.json';result.write_text(json.dumps(proof,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(check(a.root.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))
