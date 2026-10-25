#!/usr/bin/env python3
"""HOST no-GPU failure/owner/oracle/transport checks plus actual API29 strict link."""
import argparse,contextlib,copy,hashlib,importlib.util,io,json,os,shlex,shutil,subprocess,sys
from pathlib import Path
from types import SimpleNamespace

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,config,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local checker required')
 out.mkdir(parents=True);n=root/'nvn-native';sys.path.insert(0,str(n/'tools'));bundle=load(n/'tools/volume_shadow_bundle.py','checked_shadow_bundle');header,c,inputs=bundle.build(config,out/'bundle',root=root);commands=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise RuntimeError(p.stdout+p.stderr)
  return p.stdout
 f=json.loads(Path(c['runtime_fdl_check']).read_text());host=Path(f['normal_ir3'])/'build';normal=load(n/'tools/build_runtime_fdl.py','normal_fdl_flags');entry=next(x for x in json.loads((host/'compile_commands.json').read_text())if x['file'].endswith('/fd6_view.cc'));_,ff=normal.flags(entry);link=shlex.split(run(['ninja','-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'tests')];defs=['-DNVN_VOLUME_SHADOW_HEADER='+json.dumps(str(header)),'-DNN_VOLUME_SHADOW_PROBE_SOURCE='+json.dumps(str(n/'tests/volume_shadow_probe.c'))];src=[n/'tests/d2r_bridge/volume_shadow_probe_failure.c',*[n/'src'/s for s in ['texture_volume_shadow.c','compute.c','block_copy.c','texture_source.c']]];results=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/cc;folder.mkdir();objects=[]
  for i,p in enumerate(src):
   obj=folder/(str(i)+'.o');run([cc,*flags,*defs,*san,'-fno-pie','-c',p,'-o',obj]);objects.append(obj)
  use=ff if cpp=='g++'else[v for v in ff if v not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')];obj=folder/'fdl.o';run([cpp,*use,'-I'+str(n/'include'),'-Werror',*san,'-fno-pie','-c',n/'src/runtime_fdl.cc','-o',obj],host);objects.append(obj)
  binary=folder/'failures';run([cpp,'-no-pie',*san,*objects,*libs,'-o',binary],host);raw=folder/'raw';raw.mkdir();result=json.loads(run([binary,raw]));assert result['passed']is True and result['actual_GPU_submits']==0 and result['ambiguous_owners_retained']==2 and result['nonexecuting_queue_rejected']is True;results.append(result)
 assert results[0]==results[1]
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';binary=out/'volume-shadow-real-api29.so';run([cc,*flags,*defs,'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared',n/'tests/volume_shadow_probe.c',n/'android/volume_shadow_activity.c',*[n/'src'/p for p in ['texture_volume_shadow.c','compute.c','block_copy.c','texture_source.c','kgsl.c']],c['runtime_fdl_library'],'-landroid','-lm','-Wl,--no-undefined','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ('libvulkan','libegl','libgles'))
 import volume_shadow_validation as oracle
 import public_copy_runner as shared
 import volume_shadow_runner as transport
 cases=c['cases'];captured={};total=active=reads=init=0
 for v in cases:
  for repeat in range(2):
   b,a,r,i=oracle.expected(v,repeat);captured[oracle.NAMES[v['id']*2+repeat]]=b;total+=len(b);active+=a;reads+=r;init+=i
 nonce='a'*32;uid=10234;metadata=sum(2*(((c['kernel_binary_bytes']+4095)//4096)*4096+v['levels']*(12288-4))for v in cases)
 data=dict(schema=1,operation='volume_shadow',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=18,formats=6,completed=36,submissions=96,finishes=36,replays=18,files=36,owners=18,inflight_destroy_rejections=72,sequence_word_checks=96,data_byte_checks=total,active_byte_checks=active,canonical_readonly_byte_checks=reads,metadata_readonly_byte_checks=metadata,native_cpu_guard_initialization_bytes=init,bad_case=0xffffffff,bad_repeat=0xffffffff,nvn_api=False,sampling=False,game_shader=False,cpu_image_conversion=False,passed=True,run_nonce=nonce,run_directory='volume-shadow-'+nonce)
 assert oracle.validate(data,captured,nonce,uid,c)['passed'];tests=1
 def reject(fn):
  nonlocal tests
  try:fn()
  except(RuntimeError,ValueError):tests+=1
  else:raise AssertionError('negative accepted')
 for key,value in [('uid',0),('passed',1),('sampling',True),('completed',35),('submissions',True),('finishes',35),('resources_retained',True),('metadata_readonly_byte_checks',metadata-4),('active_byte_checks',active-1),('run_nonce','b'*32),('native_cpu_guard_initialization_bytes',0)]:
  d=dict(data);d[key]=value;reject(lambda d=d:oracle.validate(d,captured,nonce,uid,c))
 for case_id,offset in [(0,0),(0,512),(3,-1),(4,cases[4]['canonical_bo_bytes']),(5,cases[5]['canonical_bo_bytes']+cases[5]['native_levels'][2][4])]:
  name=oracle.NAMES[case_id*2];bad=dict(captured);b=bytearray(bad[name]);b[offset]^=1;bad[name]=bytes(b);reject(lambda bad=bad:oracle.validate(data,bad,nonce,uid,c))
 # A no-op queue's real HOST output cannot pass the independent GPU oracle.
 nonexecuted=next((out/'gcc/raw').glob('*.bin')).read_bytes();assert nonexecuted!=captured[oracle.NAMES[0]];tests+=1
 bad=copy.deepcopy(cases);bad[0]['levels']=True;reject(lambda:oracle.validate_cases(bad));bad=copy.deepcopy(cases);bad[1]['native_levels'][1][4]=bad[1]['native_levels'][0][4];reject(lambda:oracle.validate_cases(bad))
 for key in [oracle.NAMES[-1],'extra']:
  bad=dict(captured)
  if key=='extra':bad[key]=b'x'
  else:bad.pop(key)
  reject(lambda bad=bad:oracle.validate(data,bad,nonce,uid,c))
 reject(lambda:oracle.parse_result('{"passed":true,"passed":false}'));reject(lambda:oracle.parse_result('{"x":NaN}'))
 for rc,stdout,stderr,want in [(1,'','',True),(1,'','transport',False),(0,'','',False),(255,'','',False),(1,'123','',False)]:assert transport.stopped(lambda*a,**k:SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr),'org.nvn.nativeprobe')[0]is want;tests+=1
 sourcekeys=['src/texture_volume_shadow.c','include/nvn_native/texture_volume_shadow.h','src/texture_source.c','src/volume_copy.inc','src/compute.c','src/kgsl.c','tests/volume_shadow_probe.c','tests/volume_shadow_cases.h','android/volume_shadow_activity.c','android/public_copy_activity.c']
 minimal=dict(volume_shadow_check={k:c[k]for k in ('schema','kind','gpu_execution','cases','kernel_binary_bytes')},source={k:'fixture'for k in sourcekeys},artifacts={str(out/'fake-build/probe.apk'):'f'*64},texture_descriptor_flags=0)
 oldcollect=shared.collect_copy_readbacks;olduuid=shared.uuid.uuid4;shared.uuid.uuid4=lambda:SimpleNamespace(hex=nonce)
 try:
  for case in ('success','primary-failure','stop-transport-failure','apk-mismatch','inventory-extra'):
   evidence=out/(case+'.json');args=SimpleNamespace(evidence=evidence,install=False);calls=[];result=dict(data)
   if case=='primary-failure':result.update(passed=False,stage='finish',errno=35,resources_retained=True,cleanup_errno=16,completed=0,files=0)
   pidofs=0
   def fake(*argv,**kwargs):
    nonlocal pidofs
    calls.append(argv);rc=0;stderr='';stdout=''
    if argv[:3]==('shell','pm','path'):stdout='package:/fake/base.apk\n'
    elif argv[:2]==('shell','sha256sum'):stdout=('e'if case=='apk-mismatch'else'f')*64+'  /fake/base.apk\n'
    elif argv[:3]==('shell','am','force-stop'):pass
    elif argv[:2]==('shell','pidof'):pidofs+=1;rc=1;stderr='transport failed'if case=='stop-transport-failure'and pidofs>1 else''
    elif argv[:4]==('shell','cmd','package','list'):stdout='package:org.nvn.nativeprobe uid:10234\n'
    elif argv[:3]==('shell','am','start'):stdout='Status: ok\n'
    elif argv[:2]==('shell','run-as'):stdout=json.dumps(result)
    elif argv[:2]==('shell','getprop'):stdout='synthetic-fingerprint'
    elif argv[:2]==('shell','uname'):stdout='synthetic-kernel'
    elif argv[0]=='shell'and len(argv)==2 and 'volume-shadow-run-nonce'in argv[1]:pass
    else:raise AssertionError('unexpected mock '+str(argv))
    return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   def collect(run,adb,package,directory,names,path,ev):
    assert adb==['NO_REAL_ADB'];inv=['files/'+directory+'/'+name for name in oracle.NAMES]
    if case=='inventory-extra':inv.append('files/'+directory+'/extra')
    ev.update(copy_namespace_type='directory',copy_file_inventory=inv,copy_regular_inventory=inv,copy_file_inventory_after=inv,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0)
    return{}if case=='primary-failure'else captured
   shared.collect_copy_readbacks=collect
   try:
    with contextlib.redirect_stdout(io.StringIO()):transport.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
   except RuntimeError as e:
    assert case!='success'
    if case=='primary-failure':assert'stage=finish errno=35'in str(e)
   else:assert case=='success'
   observed=json.loads(evidence.read_text());assert observed['volume_shadow_verified']is(case=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);tests+=1
 finally:shared.collect_copy_readbacks=oldcollect;shared.uuid.uuid4=olduuid
 for mode in ('--public-volume-copy-check','--bc-sampled-check','--nvn-api','--compute-indirect','--nvn-pipeline-bc-check'):
  argv=[sys.executable,str(n/'tools/build_android_probe.py'),'--ndk',str(ndk),'--sdk',str(ndk.parents[1]),'--out',str(out/'MUST_NOT_CREATE'),'--volume-shadow-check',str(config),mode]
  if mode.endswith('check'):argv.append('missing')
  p=subprocess.run(argv,capture_output=True,text=True);assert p.returncode==2 and '--volume-shadow-check is an independent'in p.stderr and not(out/'MUST_NOT_CREATE').exists();tests+=1
 for p in n.rglob('*'):
  if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise RuntimeError('changed checked source '+p)
 proof=dict(schema=1,passed=True,gpu_execution=False,android_execution=False,HOST_failure=results,synthetic_oracle_transport_checks=tests,synthetic_raw_bytes=total,expected_GPU_submissions=96,expected_GPU_finishes=36,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},scope=__doc__);(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','check','ndk','out'):p.add_argument('--'+k,required=True,type=Path)
 a=p.parse_args();print(check(a.root.resolve(),a.check.resolve(),a.ndk.resolve(),a.out.resolve()))
