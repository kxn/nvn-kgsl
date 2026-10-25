#!/usr/bin/env python3
"""HOST actual native BUFFER probe failures/oracle/transport plus strict NDK actual KGSL link. No GPU."""
import argparse,contextlib,copy,hashlib,importlib.util,io,json,os,shlex,subprocess,sys
from pathlib import Path
from types import SimpleNamespace
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,config,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local checker required')
 out.mkdir(parents=True);n=root/'nvn-native';initial={str(p):sha(p)for p in n.rglob('*')if p.is_file()and '__pycache__'not in p.parts};sys.path.insert(0,str(n/'tools'));header,c,inputs=load(n/'tools/buffer_texture_bundle.py','checked_texture_bundle').build(config,out/'bundle',root=root);commands=[];inputs.update(initial)
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 cfg=json.loads(config.read_text());f=json.loads(Path(cfg['runtime_fdl_check']).read_text());host=Path(f['normal_ir3'])/'build';normal=load(n/'tools/build_runtime_fdl.py','fdl_flags');entry=next(x for x in json.loads((host/'compile_commands.json').read_text())if x['file'].endswith('/fd6_view.cc'));_,ff=normal.flags(entry);link=shlex.split(run(['ninja','-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'tests'),'-I'+str(n/'third_party/nvn')];defs=['-DNN_PIPELINE_BUFFER_SAMPLED=1','-DNVN_BUFFER_TEXTURE_HEADER='+json.dumps(str(header)),'-DNN_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))];names=['texture_buffer_sample','order','pipeline','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','offline_graphics'];sources=[n/'src'/(s+'.c')for s in names];fixture=n/'tests/d2r_bridge/buffer_texture_failure.c';results=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/cc;folder.mkdir();objects=[]
  for i,p in enumerate([fixture,*sources]):
   o=folder/(str(i)+'.o');run([cc,*flags,*defs,*san,'-fno-pie','-c',p,'-o',o]);objects.append(o)
  use=ff if cpp=='g++'else[v for v in ff if v not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')];o=folder/'fdl.o';run([cpp,*use,'-I'+str(n/'include'),'-Werror',*san,'-fno-pie','-c',n/'src/runtime_fdl.cc','-o',o],host);objects.append(o);exe=folder/'failures';run([cpp,'-no-pie',*san,*objects,*libs,'-o',exe],host);r=json.loads(run([exe]));assert r['passed']and r['cases']==4 and r['gpu_execution']is False and r['unknown_owners_retained']and r['no_op_GPU_rejected'];results.append(r)
 assert results[0]==results[1]
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';binary=out/'volume-texture-real-api29.so';run([cc,*flags,*defs,'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared',n/'tests/buffer_texture_probe.c',n/'android/buffer_texture_activity.c',*sources,n/'src/kgsl.c',c['runtime_fdl_library'],'-landroid','-lm','-Wl,--no-undefined','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ('libvulkan','libegl','libgles'))
 import buffer_texture_validation as oracle
 import buffer_texture_runner as transport
 import public_copy_runner as shared
 cases=c['cases'];target=c['target'];captured={}
 pattern=out/'authored-input.bin';run([out/'bundle/actual-buffer-cases',pattern]);expected_input=b''.join(oracle.raw(case,r)for case in oracle.CASES for r in range(2));assert pattern.read_bytes()==expected_input
 for case in cases:
  for r in range(2):
   source=oracle.raw(case,r)
   for m in range(2):
    if not m and not case['float_result']:continue
    pixels=bytearray([0xa5])*target['bo_bytes']
    for y in range(64):
     for x in range(64):pixels[y*target['pitch']+x*4:y*target['pitch']+x*4+4]=bytes(oracle.pixel(case,r,m,x))
    captured[f'buffer-texture-{case["id"]:03d}-r{r}-m{m}.bin']=source+pixels
 nonce='a'*32;uid=10234
 data=dict(schema=1,operation='buffer_texture',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=43,formats=40,prepares=86,draws=132,submissions=218,finishes=132,files=132,inflight_destroy_rejections=264,gpu_timestamp_write_words=132,bad_case=0xffffffff,bad_repeat=0xffffffff,bad_mode=0xffffffff,nvn_api=False,sampling=True,game_shader=False,cpu_image_conversion=False,cpu_source_writes=0,integer_texel_fetch=False,passed=True,run_nonce=nonce,run_directory='buffer-texture-'+nonce,**oracle.counts(c))
 observed=oracle.validate(data,captured,nonce,uid,c);assert observed['passed'];tests=1
 def reject(fn):
  nonlocal tests
  try:fn()
  except(RuntimeError,ValueError):tests+=1
  else:raise AssertionError('negative accepted')
 for k,v in [('uid',0),('passed',1),('sampling',False),('draws',131),('submissions',True),('prepares',85),('finishes',131),('gpu_timestamp_write_words',131),('resources_retained',True),('run_nonce','b'*32),('source_readonly_byte_checks',0),('metadata_readonly_byte_checks',0),('integer_texel_fetch',True),('cpu_source_writes',1)]:
  d=dict(data);d[k]=v;reject(lambda d=d:oracle.validate(d,captured,nonce,uid,c))
 name=oracle.NAMES[0]
 for offset in [0,cases[0]['source_offset'],cases[0]['source_bytes'],len(captured[name])-1]:
  bad=dict(captured);v=bytearray(bad[name]);v[offset]^=0x80;bad[name]=bytes(v);reject(lambda bad=bad:oracle.validate(data,bad,nonce,uid,c))
 bad=dict(captured);bad.pop(oracle.NAMES[-1]);reject(lambda:oracle.validate(data,bad,nonce,uid,c));bad=dict(captured);bad['extra']=b'x';reject(lambda:oracle.validate(data,bad,nonce,uid,c))
 # TXS exact integer result, TXF has only the explicit1LSB quantization bound.
 name=next(x for x in oracle.NAMES if x.endswith('m1.bin'));bad=dict(captured);v=bytearray(bad[name]);v[cases[0]['source_bytes']]^=1;bad[name]=bytes(v);reject(lambda:oracle.validate(data,bad,nonce,uid,c))
 for rc,stdout,stderr,want in [(1,'','',True),(1,'','transport',False),(0,'','',False),(255,'','',False),(1,'123','',False)]:assert transport.stopped(lambda*a,**k:SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr),'package')[0]is want;tests+=1
 sourcekeys=['src/pipeline.c','src/pipeline_program.c','src/texture_buffer_sample.c','src/order.c','src/kgsl.c','tests/buffer_texture_probe.c','tests/buffer_texture_cases.h','android/buffer_texture_activity.c','android/public_copy_activity.c'];minimal=dict(buffer_texture_check={k:c[k]for k in ('schema','kind','gpu_execution','original_game_shader','cases','target','shader_metadata')},source={k:'fixture'for k in sourcekeys},artifacts={str(out/'fake-build/probe.apk'):'f'*64},texture_descriptor_flags=0);transport.profile(minimal)
 oldcollect=shared.collect_copy_readbacks;olduuid=shared.uuid.uuid4;shared.uuid.uuid4=lambda:SimpleNamespace(hex=nonce)
 try:
  for case in ('success','primary-failure','stop-transport-failure','apk-mismatch'):
   evidence=out/(case+'.json');args=SimpleNamespace(evidence=evidence,install=False);result=dict(data);calls=[];pidofs=0
   if case=='primary-failure':result.update(passed=False,stage='finish',errno=35,resources_retained=True,cleanup_errno=16,draws=0,files=0)
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
    elif argv[:2]==('shell','getprop'):stdout='synthetic'
    elif argv[:2]==('shell','uname'):stdout='synthetic'
    elif argv[0]=='shell'and len(argv)==2 and 'buffer-texture-run-nonce'in argv[1]:pass
    else:raise AssertionError('unexpected mock '+str(argv))
    return SimpleNamespace(returncode=rc,stdout=stdout,stderr=stderr)
   def collect(run,adb,package,directory,names,path,ev):
    assert adb==['NO_REAL_ADB'];inventory=['files/'+directory+'/'+name for name in oracle.NAMES];ev.update(copy_namespace_type='directory',copy_file_inventory=inventory,copy_regular_inventory=inventory,copy_file_inventory_after=inventory,copy_inventory_returncodes=[0,0,0],copy_inventory_after_returncode=0);return{}if case=='primary-failure'else captured
   shared.collect_copy_readbacks=collect
   try:
    with contextlib.redirect_stdout(io.StringIO()):transport.exercise(args,out/'fake-build',minimal,['NO_REAL_ADB'],'org.nvn.nativeprobe','component',fake)
   except RuntimeError as e:
    assert case!='success'
    if case=='primary-failure':assert'stage=finish errno=35'in str(e)
   else:assert case=='success'
   value=json.loads(evidence.read_text());assert value['buffer_texture_verified']is(case=='success');assert any(x[:3]==('shell','am','force-stop')for x in calls);tests+=1
 finally:shared.collect_copy_readbacks=oldcollect;shared.uuid.uuid4=olduuid
 for mode in ('--volume-texture-check','--bc-sampled-check','--nvn-api','--compute-indirect','--nvn-pipeline-bc-check'):
  argv=[sys.executable,str(n/'tools/build_android_probe.py'),'--ndk',str(ndk),'--sdk',str(ndk.parents[1]),'--out',str(out/'MUST_NOT_CREATE'),'--buffer-texture-check',str(config),mode]
  if mode.endswith('check'):argv.append('missing')
  p=subprocess.run(argv,capture_output=True,text=True);assert p.returncode==2 and'--buffer-texture-check is an independent'in p.stderr and not(out/'MUST_NOT_CREATE').exists();tests+=1
 own=[Path(__file__).resolve(),fixture,n/'tests/buffer_texture_probe.c',n/'tests/buffer_texture_cases.h',n/'tests/buffer_texture_layout.c',n/'android/buffer_texture_activity.c',n/'android/public_copy_activity.c',n/'src/kgsl.c',*sources,*sorted((n/'include/nvn_native').glob('*.h')),*sorted((n/'src').glob('*.inc')),*[n/'tools'/x for x in ('buffer_texture_bundle.py','buffer_texture_validation.py','buffer_texture_runner.py','build_android_probe.py','run_android_probe.py','public_copy_runner.py')],n/'CMakeLists.txt',cc.resolve()]
 for p in own:
  if str(p)in inputs and inputs[str(p)]!=sha(p):raise ValueError('source changed during checks '+str(p))
  inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 result=dict(schema=1,kind='native-buffer-texture-probe-host-v1',passed=True,gpu_execution=False,android_execution=False,results=results,synthetic_oracle_transport_checks=tests,synthetic_verification=observed,inputs=inputs,commands=commands,elf=elf,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.check.resolve(),a.ndk.resolve(),a.out.resolve()))
