#!/usr/bin/env python3
"""Normal public clear HOST ownership/failure/oracle/transport and real NDK.
Mock queues never execute PM4 or shaders. Only root performs actual GPU runs.
"""
import argparse,contextlib,copy,hashlib,importlib.util,io,json,os,subprocess,sys
from pathlib import Path
from types import SimpleNamespace
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve();ndk=a.ndk.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);sys.path.insert(0,str(n/'tools'));proof=dict(passed=False,GPU_execution=False,CPU_shader_execution=False,runs=[],inputs={})
 def run(argv,cwd=None):
  q=subprocess.run(list(map(str,argv)),cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['runs'].append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if q.returncode:raise ValueError(q.stdout[-1000:]+q.stderr[-5000:])
  return q.stdout
 try:
  header,c,inputs=module(n/'tools/clear_color_qualification.py','normal_clear_qualifier').build(a.check.resolve(),out/'bundle',root=root);proof['inputs'].update(inputs)
  constant=json.loads(Path(c['constant_check']).read_text());kernel=json.loads(Path(c['kernel_check']).read_text());records=[Path(x)for x in kernel['inputs']if Path(x).name=='build-record.json'and json.loads(Path(x).read_text())==kernel['compiler_build']];assert len(records)==1;host=records[0].parent/'build'
  entry=next(x for x in json.loads((host/'compile_commands.json').read_text())if x['file'].endswith('/format/u_format_other.c'));_,hf=module(n/'tools/check_clear_color.py','normal_clear_probe_flags').flags(entry);lib=host/'src/util/libmesa_util.a'
  flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_CLEAR_COLOR=1','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/x)for x in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  defs=['-DNN_CLEAR_PROBE_SOURCE='+json.dumps(str(n/'tests/clear_color_probe.c')),'-DNN_PUBLIC_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNVN_PUBLIC_COPY_HEADER='+json.dumps(str(header))]
  mods=[n/'src'/f'{x}.c'for x in ['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','clear_color']]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   wf=hf if cc=='gcc'else[x for x in hf if x not in ['-Wno-nonnull-compare','-Wno-error=maybe-uninitialized']];obj=out/(cc+'-constant.o');run([cc,*wf,*san,'-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-Werror','-c',n/'src/clear_color_pattern.c','-o',obj],host)
   binary=out/('failure-'+cc);raw=out/('raw-'+cc);raw.mkdir();run([cc,*flags,*defs,*san,n/'tests/d2r_bridge/clear_color_probe_failure.c',n/'src/nvn.c',*mods,obj,lib,'-lm','-ldl','-lpthread','-o',binary]);x=json.loads(run([binary,raw]));assert x['passed']and x['actual_GPU_submits']==0 and x['HOST_nonexecuting_submits']==536;proof.setdefault('HOST',[]).append(dict(compiler=cc,**x))
  cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';binary=out/'libclear-probe-real-api29.so';run([cc,*flags,*defs,'-fPIC','-shared','-Wl,--no-undefined',n/'tests/clear_color_probe.c',n/'android/clear_color_activity.c',n/'src/nvn.c',n/'src/kgsl.c',*mods,constant['static_library'],*[n/'android'/f'{x}.c'for x in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles'])
  import clear_color_validation as oracle
  import clear_color_runner as runner
  import pipeline_runner as transport
  from texture_conversion_validation import parse_result
  nonce='a'*32;uid=10234;size=c['expected_bytes'];data=dict(schema=1,operation='public_clear_color',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,cases=67,formats=22,origins=2,phases=2,repeats=2,submissions=536,finishes=536,files=536,completed=536,source_public_finalizations=268,CallCommands_calls=268,CopyCommands_calls=0,recorded_texture_finalize_rejections=268,inflight_texture_finalize_rejections=536,data_bytes=size,command_control_readonly_bytes=536*(131072+3*8192),bad_case=0xffffffff,bad_origin=0xffffffff,bad_phase=0xffffffff,bad_repeat=0xffffffff,passed=True,resources_retained=False,pixel_oracle_on_host=True,nvn_api=True,cpu_image_conversion=False,probe_raw_submit_calls=0,probe_private_encode_calls=0,game_modules_executed=False,run_nonce=nonce,run_directory='clear-color-'+nonce)
  captured={name:(Path(c['expected'])/name).read_bytes()for name in oracle.NAMES};assert oracle.validate(data,captured,c,nonce,uid)['passed'];negative=0
  def reject(fn):
   nonlocal negative
   try:fn()
   except(RuntimeError,ValueError):negative+=1
   else:raise AssertionError('negative accepted')
  for field,value in [('uid',0),('passed',1),('submissions',True),('finishes',535),('CallCommands_calls',True),('CopyCommands_calls',1),('resources_retained',True),('probe_private_encode_calls',1),('nvn_api',False),('run_nonce','b'*32),('data_bytes',size-1)]:
   bad=dict(data);bad[field]=value;reject(lambda bad=bad:oracle.validate(bad,captured,c,nonce,uid))
  for case,offset in [(0,0),(0,65536),(0,-1),(16,65536),(528,65536)]:
   bad=dict(captured);name=oracle.NAMES[case];b=bytearray(bad[name]);b[offset]^=1;bad[name]=bytes(b);reject(lambda bad=bad:oracle.validate(data,bad,c,nonce,uid))
  bad=dict(captured);bad.pop(oracle.NAMES[-1]);reject(lambda:oracle.validate(data,bad,c,nonce,uid));bad=dict(captured);bad['extra']=b'x';reject(lambda:oracle.validate(data,bad,c,nonce,uid))
  bad=copy.deepcopy(c['cases']);bad[0]['rects'][0][0]=True;reject(lambda:oracle.validate_cases(bad));reject(lambda:parse_result('{"passed":true,"passed":false}'));reject(lambda:parse_result('{"x":NaN}'))
  # The nonexecuting full public-caller output is always rejected as pixels.
  mock={name:(out/'raw-gcc'/name).read_bytes()for name in oracle.NAMES};reject(lambda:oracle.validate(data,mock,c,nonce,uid));del mock
  sources=['src/clear_color.c','src/clear_color_record.inc','tests/clear_color_probe.c','tests/clear_color_cases.h','android/clear_color_activity.c','android/public_copy_activity.c'];minimal=dict(clear_color_check=c,source={x:'synthetic-fixture'for x in sources},artifacts={str(out/'fake-build/probe.apk'):'f'*64});runner.profile(minimal)
  old_collect=transport.collect_copy_readbacks;old_uuid=transport.uuid.uuid4;transport.uuid.uuid4=lambda:SimpleNamespace(hex=nonce);transport_cases=0
  try:
   for mode in ['success','primary-failure','stop-transport-failure','apk-mismatch','inventory-extra']:
    evidence=out/(mode+'.json');args=SimpleNamespace(evidence=evidence,install=False);result=dict(data);calls=[];pidofs=0
    if mode=='primary-failure':result.update(passed=False,stage='public-finish',errno=35,cleanup_errno=16,resources_retained=True,completed=0,files=0)
    def fake(*argv,**kw):
     nonlocal pidofs
     calls.append(argv);stdout='';stderr='';rc=0
     if argv[:2]==('shell','pidof'):
      pidofs+=1;rc=1
      if mode=='stop-transport-failure'and pidofs>1:rc=255;stderr='transport failure'
     elif argv[:3]==('shell','pm','path'):stdout='package:/data/app/probe.apk\n'
     elif argv[:2]==('shell','sha256sum'):stdout=('e'if mode=='apk-mismatch'else'f')*64+'  /data/app/probe.apk\n'
     elif argv[:4]==('shell','cmd','package','list'):stdout='package:org.nvn.nativeprobe uid:'+str(uid)+'\n'
     elif 'cat'in argv:stdout=json.dumps(result)
     elif argv[:2]==('shell','getprop'):stdout='HOST mock fingerprint'
     elif argv[:2]==('shell','uname'):stdout='HOST mock kernel'
     return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
    def collect(run,adb,package,directory,names,evidence_path,ev):
     inventory=['files/'+directory+'/'+name for name in names]
     if mode=='inventory-extra':inventory.append('files/'+directory+'/extra')
     ev.update(copy_namespace_type='directory',copy_file_inventory=inventory,copy_regular_inventory=inventory,copy_file_inventory_after=inventory,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0);return captured
    transport.collect_copy_readbacks=collect
    try:
     with contextlib.redirect_stdout(io.StringIO()):runner.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
    except RuntimeError as e:
     assert mode!='success'
     if mode=='primary-failure':assert'stage=public-finish errno=35'in str(e)
    else:assert mode=='success'
    actual=json.loads(evidence.read_text());assert actual['native_pipeline_verified']is(mode=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);transport_cases+=1
  finally:transport.collect_copy_readbacks=old_collect;transport.uuid.uuid4=old_uuid
  for mode in ['--nvn-api','--block-copy-check','--public-copy-call','--native-color-target-check','--d2r-host-record']:
   argv=[sys.executable,str(n/'tools/build_android_probe.py'),'--sdk',str(ndk.parents[1]),'--ndk',str(ndk),'--out',str(out/'MUST_NOT_CREATE'),'--clear-color-check',str(a.check),mode]
   if mode.endswith('check')or mode.endswith('record'):argv.append('missing')
   q=subprocess.run(argv,text=True,capture_output=True);assert q.returncode==2 and '--clear-color-check is an independent'in q.stderr and not(out/'MUST_NOT_CREATE').exists();negative+=1
  proof.update(passed=True,synthetic_negative_cases=negative,synthetic_transport_cases=transport_cases,nonexecuting_HOST_outputs_rejected=True,normal_qualifier=a.check.resolve().as_posix())
 except Exception as e:proof['error']=str(e)
 for path in n.rglob('*'):
  if path.is_file()and'__pycache__'not in path.parts:proof['inputs'][str(path)]=sha(path)
 proof['artifacts']={str(path):sha(path)for path in out.rglob('*')if path.is_file()};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
