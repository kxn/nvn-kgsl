#!/usr/bin/env python3
"""HOST actual-public-API failure/ownership and oracle/transport; ARM64 strict link.
No ADB, CP instructions, or image kernel are executed by this checker.
"""
import argparse,contextlib,hashlib,importlib.util,io,json,os,subprocess,sys
from pathlib import Path
from types import SimpleNamespace

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,kernel,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored check directory required')
 out.mkdir(parents=True);r=root/'nvn-native';sys.path.insert(0,str(r/'tools'));bundle=module(r/'tools/texture_texels_bundle.py','texel_bundle');header,record,inputs=bundle.build(kernel,out/'bundle',root=root);commands=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise RuntimeError(p.stderr)
  return p.stdout
 fixture=r/'tests/d2r_bridge/texture_texels_probe_failure.c';defs=['-DNN_TEXELS_PROBE_SOURCE='+json.dumps(str(r/'tests/texture_texels_probe.c')),'-DNN_TEXELS_MEMORY_BACKEND='+json.dumps(str(r/'tests/d2r_bridge/memory_backend.c')),'-DNVN_TEXTURE_TEXELS_HEADER='+json.dumps(str(header))]
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(r/p)for p in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
 modules=['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries'];sources=[r/'src'/f'{m}.c'for m in modules];host=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('failure-'+cc);folder=out/('raw-'+cc);folder.mkdir();run([cc,*flags,*defs,*san,fixture,r/'src/nvn.c',*sources,'-Wl,--wrap=nn_queue_submit','-Wl,--wrap=nn_queue_finish','-lm','-o',exe]);actual=json.loads(run([exe,folder]));assert actual['passed']is True and actual['actual_GPU_submits']==0 and actual['ambiguous_owners_retained']==2 and actual['nonexecuting_queue_rejected_before_caller_invalidate']is True;host.append(actual)
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';binary=out/'texture-texels-real-api29.so';run([cc,*flags,*defs,'-fPIC','-shared','-Wl,--no-undefined','-Wl,--wrap=nn_queue_submit','-Wl,--wrap=nn_queue_finish',r/'tests/texture_texels_probe.c',r/'android/texture_texels_activity.c',r/'src/nvn.c',r/'src/kgsl.c',*sources,*[r/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles'])
 # These deliberately synthetic byte outputs only check the independent oracle
 # and transport rejection paths. They never qualify a GPU implementation.
 import texture_texels_validation as oracle
 import texture_texels_runner as transport
 oracle.validate_cases(record['cases']);captured={oracle.NAMES[n*2+replay]:oracle.expected(n,replay)[0]for n in range(156)for replay in range(2)};captured[oracle.NAMES[-1]]=oracle.busy_expected();total=sum(len(captured[name])for name in oracle.NAMES[:-1]);active=sum(oracle.expected(n,k)[1]for n in range(156)for k in range(2));reads=sum(oracle.expected(n,k)[2]for n in range(156)for k in range(2));nonce='a'*32;uid=10234
 data=dict(schema=1,operation='texture_texels',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=156,formats=37,completed=312,public_transfer_calls=312,replays=156,files=313,pool_imports_main=156,pool_releases_main=156,texture_initializations_main=156,texture_finalizations_main=156,buffer_initializations_main=234,noaccess_rejections=2,three_d_rejections=1,three_d_rejection_stage=1,busy_rejections=2,busy_GPU_word_verified=True,native_submit_attempts=313,native_submit_accepted=313,native_submit_failed=0,native_finish_calls=313,native_finish_completed=313,native_finish_failed=0,native_bad_arguments=0,bad_case=0xffffffff,bad_replay=0xffffffff,bad_pool=0xffffffff,bad_offset=0xffffffffffffffff,nvn_api=True,probe_raw_transfer_calls=0,probe_private_encode_calls=0,read_only_forwarding_observers=True,CPU_visibility_before_caller_invalidate=True,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,run_nonce=nonce,run_directory='texels-'+nonce,data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,post_cache_stability_byte_checks=total)
 assert oracle.validate(data,captured,nonce,uid,record)['passed']is True;tests=1
 def reject(fn):
  nonlocal tests
  try:fn()
  except(RuntimeError,ValueError):tests+=1
  else:raise AssertionError('negative was accepted')
 bad_cases=json.loads(json.dumps(record['cases']));bad_cases[0]['region'][0][0]=True;reject(lambda:oracle.validate_cases(bad_cases))
 for key,value in [('uid',0),('passed',1),('cleanup_errno',16),('public_transfer_calls',True),('native_finish_completed',312),('native_submit_accepted',312),('resources_retained',True),('run_nonce','b'*32),('post_cache_stability_byte_checks',0),('active_byte_checks',active-1),('BC_decode',True),('nvn_api',False),('probe_raw_transfer_calls',1),('CPU_visibility_before_caller_invalidate',False),('read_only_forwarding_observers',False),('busy_rejections',0),('busy_GPU_word_verified',False),('three_d_rejection_stage',True),('three_d_rejections',2)]:
  bad=dict(data);bad[key]=value;reject(lambda bad=bad:oracle.validate(bad,captured,nonce,uid,record))
 # Whole buffers include active output, source readonly bytes, neighbors,
 # inactive mip/layers and ceil-DWORD tails. Corrupt each class independently.
 for n,index in [(0,512),(3,0),(3,-1),(1,512),(2,1024),(4,-1),(156,512),(156,0),(156,-1)]:
  name=oracle.NAMES[n*2];bad=dict(captured);b=bytearray(bad[name]);b[index]^=1;bad[name]=bytes(b);reject(lambda bad=bad:oracle.validate(data,bad,nonce,uid,record))
 bad=dict(captured);bad.pop(oracle.NAMES[-1]);reject(lambda:oracle.validate(data,bad,nonce,uid,record));bad=dict(captured);bad['extra']=b'x';reject(lambda:oracle.validate(data,bad,nonce,uid,record));reject(lambda:oracle.parse_result('{"passed":true,"passed":false}'));reject(lambda:oracle.parse_result('{"x":NaN}'))
 for rc,stdout,stderr,want in [(1,'','',True),(1,'','transport',False),(0,'','',False),(255,'','',False),(1,'123','',False)]:
  assert transport.stopped(lambda*a,**k:SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr),'org.nvn.nativeprobe')[0]is want;tests+=1
 sourcekeys=['src/compute.c','src/texture_source.c','src/block_copy.c','src/nvn.c','src/texture_texels.inc','src/canonical_copy_region.inc','tests/public_copy_cases.h','tests/texture_texels_probe.c','android/texture_texels_activity.c'];minimal=dict(texture_texels_check={k:record[k]for k in ['schema','kind','gpu_execution','cases']},source={k:'fixture'for k in sourcekeys},artifacts={str(out/'fake-build/probe.apk'):'f'*64},texture_descriptor_flags=0)
 old_collect=transport.collect_copy_readbacks;old_uuid=transport.uuid.uuid4;transport.uuid.uuid4=lambda:SimpleNamespace(hex=nonce)
 try:
  for case in ['success','primary-failure','stop-transport-failure','apk-mismatch','inventory-extra']:
   evidence=out/(case+'.json');args=SimpleNamespace(evidence=evidence,install=False);calls=[];result=dict(data)
   if case=='primary-failure':result.update(passed=False,stage='public-write',errno=35,resources_retained=True,cleanup_errno=16,completed=0,files=0)
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
    elif argv[0]=='shell'and len(argv)==2 and 'texels-run-nonce'in argv[1]:stdout='';rc=0;stderr=''
    else:raise AssertionError('unexpected mocked transport '+str(argv))
    return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   def collect(run,adb,package,directory,names,path,ev):
    assert adb==['NO_REAL_ADB'];inventory=['files/'+directory+'/'+name for name in oracle.NAMES]
    if case=='inventory-extra':inventory.append('files/'+directory+'/extra')
    ev.update(copy_namespace_type='directory',copy_file_inventory=inventory,copy_regular_inventory=inventory,copy_file_inventory_after=inventory,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0)
    return{}if case=='primary-failure'else captured
   transport.collect_copy_readbacks=collect
   try:
    with contextlib.redirect_stdout(io.StringIO()):transport.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
   except RuntimeError as e:
    assert case!='success'
    if case=='primary-failure':assert 'stage=public-write errno=35'in str(e)
   else:assert case=='success'
   observed=json.loads(evidence.read_text());assert observed['texture_texels_verified']is(case=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);tests+=1
 finally:transport.collect_copy_readbacks=old_collect;transport.uuid.uuid4=old_uuid
 for mode in ['--nvn-api','--public-copy-check','--bc-sampled-check','--block-copy-check','--texture-conversion-check','--compute-indirect','--d2r-host-record']:
  argv=[sys.executable,str(r/'tools/build_android_probe.py'),'--ndk',str(ndk),'--sdk',str(ndk.parents[1]),'--out',str(out/'MUST_NOT_CREATE'),'--texture-texels-check',str(kernel),mode]
  if mode.endswith('check')or mode.endswith('record'):argv.append('missing')
  q=subprocess.run(argv,capture_output=True,text=True);assert q.returncode==2 and ('--texture-texels-check is an independent'in q.stderr or (mode=='--bc-sampled-check'and'--bc-sampled-check is an independent'in q.stderr)) and not(out/'MUST_NOT_CREATE').exists();tests+=1
 for p in sorted(r.rglob('*')):
  if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 inputs[str(kernel)]=sha(kernel);inputs[str(cc)]=sha(cc)
 for p,h in inputs.items():
  if sha(p)!=h:raise RuntimeError('input changed during check '+p)
 proof=dict(schema=1,passed=True,gpu_execution=False,android_execution=False,host_failure=host,synthetic_oracle_transport_checks=tests,synthetic_raw_bytes=total,scope='Actual public NVN texel API failure ownership in nonexecuting HOST backend, independent synthetic math/transport fixtures, ARM64 complete production backend/Activity link; no GPU or Android execution',inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});result=out/'check.json';result.write_text(json.dumps(proof,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(check(a.root.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))
