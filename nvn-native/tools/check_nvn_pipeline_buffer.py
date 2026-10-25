#!/usr/bin/env python3
"""HOST oracle/collector/failure owners and normal CMake Android strict link, no GPU."""
import argparse,copy,hashlib,json,os,shlex,shutil,subprocess,sys
from pathlib import Path
from nvn_pipeline_buffer_qualification import build,sha
from nvn_pipeline_buffer_validation import NAMES,FORMATS,raw,validate

def run_check(root,check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];header,c,inputs=build(check,out,root=root);out.mkdir(parents=True);commands=[]
 def run(argv,cwd=None):
  r=subprocess.run(list(map(str,argv)),cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stderr+r.stdout)[-6000:])
  return r.stdout
 negatives=[]
 for extra in (['--cs-case','1'],['--nvn-pipeline-check',str(check)],['--buffer-texture-check',str(check)],['--nvn-pipeline-call']):
  a=[sys.executable,str(own/'tools/build_android_probe.py'),'--sdk',str(ndk.parent.parent),'--ndk',str(ndk),'--out',str(out/'forbidden'),'--nvn-pipeline-buffer-check',str(check),*extra];r=subprocess.run(a,text=True,capture_output=True)
  if r.returncode!=2 or 'independent public NVN BUFFER qualification'not in r.stderr or(out/'forbidden').exists():raise ValueError('incompatible public BUFFER builder mode accepted')
  negatives.append('CLI '+str(extra))
 nonce='a'*32;uid=10001
 data=dict(schema=1,operation='nvn_buffer_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=48,finishes=48,completed_cases=48,files=48,formats=12,recorded_handles=36,released_handles=36,copy_commands=12,call_commands=12,replays=24,shader_source_finalizes=1,texture_registrations=60,registered_cpu_object_finalizes=60,inflight_destroy_rejections=96,channel_checks=786432,guard_byte_checks=393216,readonly_byte_checks=7077888,index_domain=[0],full_c1_bytes=256,graphics_control_profile_bytes=2176,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False,passed=True,bad_case=0xffffffff,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,maximum_channel_error=0)
 captured={NAMES[4*i+p]:raw(f,p)for i,f in enumerate(FORMATS)for p in range(4)};positive=validate(data,captured,c,nonce,uid)
 def reject(label,d,b):
  try:validate(d,b,c,nonce,uid)
  except(RuntimeError,ValueError,KeyError,TypeError):negatives.append(label);return
  raise ValueError('collector accepted '+label)
 for k,v in [('submissions',47),('finishes',47),('files',47),('readonly_byte_checks',1),('registered_cpu_object_finalizes',59),('inflight_destroy_rejections',95),('resources_retained',True),('errno',35),('index_domain',[1]),('index_domain',[False]),('uid',True),('full_c1_bytes',0),('graphics_control_profile_bytes',2200),('public_nvn',False),('maximum_channel_error',2),('run_nonce','b'*32),('passed',False)]:
  d=copy.deepcopy(data);d[k]=v;reject(k,d,captured)
 for name in ('missing','extra','short','guard','pixel','Buffer source','index source'):
  b=dict(captured)
  if name=='missing':b.pop(NAMES[-1])
  elif name=='extra':b['unexpected']=b''
  elif name=='short':b[NAMES[0]]=b'error text'
  else:
   i={'guard':0,'pixel':4096,'Buffer source':49152+4097,'index source':24576+4096}[name];z=bytearray(b[NAMES[0]]);z[i]^=255;b[NAMES[0]]=bytes(z)
  reject(name,data,b)
 # Source inventory is a closed candidate tree, not a union of older trees.
 for folder in('src','include','android','third_party','tests'):
  for p in(own/folder).rglob('*'):
   if p.is_file()and p.suffix in('.c','.h','.cc','.inc','.S'):inputs[str(p)]=sha(p)
 for p in(own/'CMakeLists.txt',Path(__file__).resolve(),own/'tools/build_android_probe.py',own/'tools/run_android_probe.py'):inputs[str(p)]=sha(p)
 # Real FDL HOST library/flags from its normal Meson record, no replacement layout.
 f=json.loads(Path(c['runtime_fdl_check']).read_text());host=Path(f['normal_ir3'])/'build';normal=__import__('importlib.util',fromlist=['']).spec_from_file_location('public_buffer_host_fdl',root/'nvn-native/tools/build_runtime_fdl.py');m=__import__('importlib.util',fromlist=['']).module_from_spec(normal);normal.loader.exec_module(m)
 entry=next(x for x in json.loads((host/'compile_commands.json').read_text())if x['file'].endswith('/fd6_view.cc'));_,flags=m.flags(entry);cmd=shlex.split(run(['ninja','-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in cmd[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 modules=[own/'src'/(n+'.c')for n in('nvn','compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','block_copy','texture_buffer_sample')]
 common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-DNN_NVN_BUFFER_SAMPLED=1','-DNN_PIPELINE_BUFFER_SAMPLED=1','-DNVN_NVN_PIPELINE_BUFFER=1','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(own/'tests/d2r_bridge/memory_backend.c')),*['-I'+str(own/d)for d in('include','third_party/nvn','include/nvn_native','tests','tests/d2r_bridge')]]
 fixture=own/'tests/d2r_bridge/nvn_pipeline_buffer_failure.c';probe=own/'tests/nvn_pipeline_probe.c';failure_results=[]
 for cc,cpp,san in(('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])):
  folder=out/cc;folder.mkdir();objs=[]
  for i,s in enumerate([fixture,probe,own/'tests/d2r_bridge/timeline_backend.c',*modules]):
   o=folder/(str(i)+'.o');run([cc,*common,*san,'-fno-pie','-c',s,'-o',o]);objs.append(o)
  ff=flags if cpp=='g++'else[x for x in flags if x not in('-flifetime-dse=1','-Wno-error=maybe-uninitialized')];o=folder/'fdl.o';run([cpp,*ff,'-I'+str(own/'include'),'-Werror',*san,'-fno-pie','-c',own/'src/runtime_fdl.cc','-o',o],host);objs.append(o);binary=folder/'probe-failure';run([cpp,'-no-pie',*san,*objs,*libs,'-o',binary],host)
  for mode in('no-gpu','unknown'):
   directory=folder/mode;directory.mkdir();d=json.loads(run([binary,directory,mode]));failure_results.append(d)
 toolchain=ndk/'build/cmake/android.toolchain.cmake'
 run(['cmake','-S',own,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_NVN_PIPELINE_BUFFER=ON','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(header),'-DNVN_NVN_PIPELINE_BUFFER_FDL_LIBRARY='+c['runtime_fdl_library']]);run(['cmake','--build',out/'cmake','--target','nvn_kgsl_probe','-j','6'])
 elf=out/'cmake/libnvn_kgsl_probe.so';info=run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf','-h','-d',elf]);(out/'elf.txt').write_text(info)
 if 'AArch64'not in info or any(v in info.lower()for v in('libvulkan','libgles','libegl')):raise ValueError('unexpected Android ELF dependency')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('changed HOST input '+p)
 result=dict(schema=1,kind='public-nvn-buffer-sampled-host-v1',passed=True,gpu_execution=False,android_execution=False,synthetic_validator_positive=positive,negative_checks=negatives,failure_results=failure_results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in('.so','.o','.a','.json','.txt')});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(run_check(a.root.resolve(),a.check.resolve(),a.ndk.resolve(),a.out.resolve()))
