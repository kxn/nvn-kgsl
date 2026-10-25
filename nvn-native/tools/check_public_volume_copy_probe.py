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

def check(root,kernel,ndk,out,*,call_commands=False):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored check directory required')
 out.mkdir(parents=True);r=root/'nvn-native';sys.path.insert(0,str(r/'tools'));bundle=module(r/'tools/public_volume_copy_bundle.py','public_volume_bundle');header,record,inputs=bundle.build(kernel,out/'bundle',root=root);commands=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise RuntimeError(p.stderr)
  return p.stdout
 fixture=r/'tests/d2r_bridge/public_copy_probe_failure.c';defs=['-DNN_PUBLIC_PROBE_SOURCE='+json.dumps(str(r/'tests/public_copy_probe.c')),'-DNN_PUBLIC_MEMORY_BACKEND='+json.dumps(str(r/'tests/d2r_bridge/memory_backend.c')),'-DNVN_PUBLIC_COPY_HEADER='+json.dumps(str(header))]
 defs.append('-DNVN_PUBLIC_COPY_CALL='+str(int(call_commands)))
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(r/p)for p in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
 modules=['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries'];sources=[r/'src'/f'{m}.c'for m in modules];host=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('failure-'+cc);folder=out/('raw-'+cc);folder.mkdir();run([cc,*flags,*defs,*san,fixture,r/'src/nvn.c',*sources,'-lm','-o',exe]);actual=json.loads(run([exe,folder]));assert actual['passed']is True and actual['actual_GPU_submits']==0 and actual['ambiguous_owners_retained']==2 and actual['nonexecuting_queue_rejected_by_byte_oracle']is True;host.append(actual)
 typed=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('typed-'+cc);run([cc,*flags,*defs,'-DNN_PUBLIC_CORE_SOURCE='+json.dumps(str(r/'src/nvn.c')),*san,r/'tests/d2r_bridge/public_volume_copy_probe_failure.c',*sources,'-lm','-o',exe]);t=json.loads(run([exe]));assert t['passed']is True and t['actual_GPU_submits']==0 and t['missing_volume_registration_rejected']is True and t['mip_Z_view_layer_bounds_rejected']is True and t['typed_UBO_and_Call_owner_checked'if call_commands else'typed_UBO_and_copied_owner_checked']is True;typed.append(t)
 geometry=[]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('geometry-'+cc);run([cc,*flags,*defs,*san,r/'tests/public_volume_copy_geometry.c',r/'src/texture_source.c','-o',exe]);g=json.loads(run([exe]));assert g['passed']is True and g['cases']==228 and g['GPU_execution']is False;geometry.append(g)
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';binary=out/'public-volume-copy-real-api29.so';run([cc,*flags,*defs,'-fPIC','-shared','-Wl,--no-undefined',r/'tests/public_copy_probe.c',r/'android/public_volume_copy_activity.c',r/'src/nvn.c',r/'src/kgsl.c',*sources,*[r/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles'])
 # These deliberately synthetic byte outputs only check the independent oracle
 # and transport rejection paths. They never qualify a GPU implementation.
 import public_volume_copy_validation as oracle
 import public_copy_runner as transport
 import public_volume_copy_runner as volume_transport
 oracle.validate_cases(record['cases']);captured={oracle.NAMES[n*2+replay]:oracle.expected(n,replay)[0]for n in range(228)for replay in range(2)};total=sum(map(len,captured.values()));active=sum(oracle.expected(n,k)[1]for n in range(228)for k in range(2));reads=sum(oracle.expected(n,k)[2]for n in range(228)for k in range(2));nonce='a'*32;uid=10234
 data=dict(schema=1,operation='public_volume_copy',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=228,formats=37,families=3,completed=456,submissions=456,finishes=456,replays=228,files=456,CopyCommands_calls=228,recorded_texture_finalize_rejections=304,pool_imports=570,pool_releases=570,texture_initializations=304,texture_finalizations=304,buffer_initializations=456,bad_case=0xffffffff,bad_replay=0xffffffff,bad_pool=0xffffffff,bad_offset=0xffffffffffffffff,nvn_api=True,probe_raw_submit_calls=0,probe_private_encode_calls=0,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,run_nonce=nonce,run_directory='public-volume-copy-'+nonce,data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,command_control_readonly_byte_checks=131072*456)
 if call_commands:data.update(CopyCommands_calls=0,CallCommands_calls=228,call_commands=True)
 def validate(*values):return oracle.validate(*values,call_commands=call_commands)
 assert validate(data,captured,nonce,uid,record)['passed']is True;tests=1
 def reject(fn):
  nonlocal tests
  try:fn()
  except(RuntimeError,ValueError):tests+=1
  else:raise AssertionError('negative was accepted')
 reject(lambda:oracle.validate(data,captured,nonce,uid,record,call_commands=not call_commands))
 bad=dict(data);bad['call_commands']=1 if call_commands else True;reject(lambda:validate(bad,captured,nonce,uid,record))
 bad=dict(data);bad['CallCommands_calls']=True;reject(lambda:validate(bad,captured,nonce,uid,record))
 bad_cases=json.loads(json.dumps(record['cases']));bad_cases[0]['region'][0][0]=True;reject(lambda:oracle.validate_cases(bad_cases))
 for key,value in [('uid',0),('passed',1),('cleanup_errno',16),('submissions',True),('finishes',455),('resources_retained',True),('run_nonce','b'*32),('command_control_readonly_byte_checks',0),('active_byte_checks',active-1),('BC_decode',True),('nvn_api',False),('probe_raw_submit_calls',1),('CopyCommands_calls',227)]:
  bad=dict(data);bad[key]=value;reject(lambda bad=bad:validate(bad,captured,nonce,uid,record))
 # Whole buffers include active output, source readonly bytes, neighbors,
 # inactive mip/layers and ceil-DWORD tails. Corrupt each class independently.
 for n,index in [(0,512),(3,0),(3,-1),(1,512),(2,1024),(4,-1)]:
  name=oracle.NAMES[n*2];bad=dict(captured);b=bytearray(bad[name]);b[index]^=1;bad[name]=bytes(b);reject(lambda bad=bad:validate(data,bad,nonce,uid,record))
 bad=dict(captured);bad.pop(oracle.NAMES[-1]);reject(lambda:validate(data,bad,nonce,uid,record));bad=dict(captured);bad['extra']=b'x';reject(lambda:validate(data,bad,nonce,uid,record));reject(lambda:oracle.parse_result('{"passed":true,"passed":false}'));reject(lambda:oracle.parse_result('{"x":NaN}'))
 for rc,stdout,stderr,want in [(1,'','',True),(1,'','transport',False),(0,'','',False),(255,'','',False),(1,'123','',False)]:
  assert transport.stopped(lambda*a,**k:SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr),'org.nvn.nativeprobe')[0]is want;tests+=1
 sourcekeys=['src/compute.c','src/texture_source.c','src/block_copy.c','src/nvn.c','src/canonical_copy_record.inc','src/canonical_texture_copy.inc','tests/public_copy_cases.h','tests/public_copy_probe.c','android/public_copy_activity.c','src/volume_copy.inc','src/texture_volume.inc','tests/public_volume_copy_layout.c','android/public_volume_copy_activity.c'];minimal=dict(public_volume_copy_check={k:record[k]for k in ['schema','kind','gpu_execution','cases']},source={k:'fixture'for k in sourcekeys},artifacts={str(out/'fake-build/probe.apk'):'f'*64},texture_descriptor_flags=0)
 minimal['public_copy_call']=call_commands
 if call_commands:
  for k in ['src/nvn_command_copy_types.inc','src/nvn_command_copy_cp_record.inc','src/nvn_command_copy_canonical_record.inc','src/nvn_command_copy_bound_clear.inc','src/nvn_command_copy_projection.inc','src/nvn_command_sequence.inc','src/nvn_command_projection.inc']:minimal['source'][k]='fixture'
 old_collect=transport.collect_copy_readbacks;old_uuid=transport.uuid.uuid4;transport.uuid.uuid4=lambda:SimpleNamespace(hex=nonce)
 try:
  for case in ['success','primary-failure','stop-transport-failure','apk-mismatch','inventory-extra']:
   evidence=out/(case+'.json');args=SimpleNamespace(evidence=evidence,install=False);calls=[];result=dict(data)
   if case=='primary-failure':result.update(passed=False,stage='public-finish',errno=35,resources_retained=True,cleanup_errno=16,completed=0,files=0)
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
    elif argv[0]=='shell'and len(argv)==2 and 'public-volume-copy-run-nonce'in argv[1]:stdout='';rc=0;stderr=''
    else:raise AssertionError('unexpected mocked transport '+str(argv))
    return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   def collect(run,adb,package,directory,names,path,ev):
    assert adb==['NO_REAL_ADB'];inventory=['files/'+directory+'/'+name for name in oracle.NAMES]
    if case=='inventory-extra':inventory.append('files/'+directory+'/extra')
    ev.update(copy_namespace_type='directory',copy_file_inventory=inventory,copy_regular_inventory=inventory,copy_file_inventory_after=inventory,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0)
    return{}if case=='primary-failure'else captured
   transport.collect_copy_readbacks=collect
   try:
    with contextlib.redirect_stdout(io.StringIO()):volume_transport.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
   except RuntimeError as e:
    assert case!='success'
    if case=='primary-failure':assert 'stage=public-finish errno=35'in str(e)
   else:assert case=='success'
   observed=json.loads(evidence.read_text());assert observed['public_volume_copy_verified']is(case=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);tests+=1
 finally:transport.collect_copy_readbacks=old_collect;transport.uuid.uuid4=old_uuid
 for mode in ['--nvn-api','--public-copy-check','--texture-texels-check','--bc-sampled-check','--block-copy-check','--texture-conversion-check','--compute-indirect','--d2r-host-record']:
  argv=[sys.executable,str(r/'tools/build_android_probe.py'),'--ndk',str(ndk),'--sdk',str(ndk.parents[1]),'--out',str(out/'MUST_NOT_CREATE'),'--public-volume-copy-check',str(kernel),mode]
  if mode.endswith('check')or mode.endswith('record'):argv.append('missing')
  q=subprocess.run(argv,capture_output=True,text=True);assert q.returncode==2 and '--public-volume-copy-check is an independent'in q.stderr and not(out/'MUST_NOT_CREATE').exists();tests+=1
 for p in sorted(r.rglob('*')):
  if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 inputs[str(kernel)]=sha(kernel);inputs[str(cc)]=sha(cc)
 for p,h in inputs.items():
  if sha(p)!=h:raise RuntimeError('input changed during check '+p)
 proof=dict(schema=1,passed=True,call_commands=call_commands,gpu_execution=False,android_execution=False,host_failure=host,host_typed=typed,host_geometry=geometry,synthetic_oracle_transport_checks=tests,synthetic_raw_bytes=total,scope='Actual public true3D NVN API/CopyCommands failure ownership in nonexecuting HOST backend, independent synthetic math/transport fixtures, ARM64 complete production backend/Activity link; no GPU or Android execution',inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});result=out/'check.json';result.write_text(json.dumps(proof,indent=2)+'\n');return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--call-commands',action='store_true');a=p.parse_args();print(check(a.root.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve(),call_commands=a.call_commands))
