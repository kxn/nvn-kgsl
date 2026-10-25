#!/usr/bin/env python3
"""HOST public native ClearColor/Call inherited owners; actual NDK strict0/1.
No CPU shader execution or GPU evidence. Normal original4CS input is reused.
"""
import argparse,hashlib,importlib.util,json,os,shlex,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','constant-check','kernel-check','compute-check','ir3','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(passed=False,GPU_execution=False,CPU_shader_execution=False,inputs={},runs=[])
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['runs'].append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-6000:]+r.stdout[-1000:])
  return r.stdout
 try:
  constants=json.loads(a.constant_check.read_text())
  if not constants.get('passed')or constants.get('kind')!='native-clear-color-constants-v1'or constants.get('gpu_execution')is not False:raise ValueError('normal constant package required')
  for f,h in {**constants['inputs'],**constants['artifacts']}.items():
   if sha(f)!=h:raise ValueError('changed normal constant input '+f)
  for rel in ['include/nvn_native/clear_color.h','src/clear_color_pattern.c']:
   hashes=[v for k,v in constants['inputs'].items()if k.endswith('/nvn-native/'+rel)]
   if hashes!=[sha(n/rel)]:raise ValueError('constant module differs from current source')
  archive=Path(constants['static_library']);proof['inputs'].update(constants['inputs']);proof['inputs'].update(constants['artifacts']);proof['inputs'][str(a.constant_check.resolve())]=sha(a.constant_check)
  kernel=module(n/'tools/compile_clear_color.py','normal_clear_kernel').verify(root,a.kernel_check.resolve());kh=Path(kernel['header']);proof['inputs'].update(kernel['inputs']);proof['inputs'].update(kernel['artifacts']);proof['inputs'][str(a.kernel_check.resolve())]=sha(a.kernel_check)
  cache=module(n/'tools/offline_compute_cache.py','normal_clear_compute_cache');h,m,locked=cache.build(a.compute_check.resolve(),out/'compute-cache',root=root);proof['inputs'].update(m['inputs']);proof['inputs'].update(locked)
  for folder in ['src','include','android','third_party/nvn','third_party/kgsl','tests/d2r_bridge']:
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in ['.c','.h','.inc','.py']:proof['inputs'][str(f)]=sha(f)
  proof['inputs'][str(Path(__file__).resolve())]=sha(__file__);proof['inputs'][str(n/'tools/check_clear_color.py')]=sha(n/'tools/check_clear_color.py')
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  defs=['-DNN_NVN_CLEAR_COLOR=1','-DNN_COMPUTE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_COMPUTE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_COMPUTE_CACHE_HEADER='+json.dumps(str(h)),'-DNN_CLEAR_COLOR_HEADER='+json.dumps(str(kh))]
  mods=[n/'src'/f'{v}.c'for v in ['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries']]
  host=a.ir3.resolve()/'build';entries=json.loads((host/'compile_commands.json').read_text());entry=next(e for e in entries if e['file'].endswith('/format/u_format_other.c'));helper=module(n/'tools/check_clear_color.py','normal_clear_flags');_,hf=helper.flags(entry);lib=host/'src/util/libmesa_util.a';proof['inputs'][str(lib)]=sha(lib)
  for mode in [0,1]:
   flags=common+['-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode)]
   for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
    # Original Mesa-generated HOST config is used ONLY to compile the ONE
    # constant adapter. It is not copied into native code or used for Android.
    wf=hf if cc=='gcc'else[x for x in hf if x not in ['-Wno-nonnull-compare','-Wno-error=maybe-uninitialized']]
    obj=out/(cc+'-pattern.o');run([cc,*wf,*san,'-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-Werror','-c',n/'src/clear_color_pattern.c','-o',obj],host)
    binary=out/(cc+'-graphics'+str(mode));run([cc,*flags,*defs,*san,n/'tests/d2r_bridge/clear_color_call.c',*mods,n/'src/clear_color.c',obj,lib,'-lm','-ldl','-lpthread','-o',binary]);x=json.loads(run([binary]));assert x['passed']and x['Call_ClearColor_replays']==2 and not x['GPU_execution'];proof.setdefault('results',[]).append(dict(compiler=cc,graphics_source_cache=mode,**x))
   cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc.resolve())]=sha(cc.resolve())
   for enabled in [0,1]:
    binary=out/('libclear-api29-graphics'+str(mode)+'-clear'+str(enabled)+'.so');extra=[n/'src/clear_color.c',archive]if enabled else[]
    run([cc,*flags,'-DNN_NVN_CLEAR_COLOR='+str(enabled),'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*mods,*[n/'android'/f'{v}.c'for v in ['window_session','window_android','present','present_pacer','window_display','ahb']],*extra,'-landroid','-ldl','-lm','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles']);proof.setdefault('NDK_real_KGSL',[]).append(dict(graphics_source_cache=mode,clear_enabled=enabled,path=str(binary),sha256=sha(binary)))
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise ValueError('changed source '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 proof['artifacts']={str(f):sha(f)for f in out.rglob('*')if f.is_file()};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
