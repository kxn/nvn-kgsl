#!/usr/bin/env python3
"""HOST private-config/semantic snapshot protocol and real NDK link. No display/GPU execution.

An explicit normal texture bridge producer header is required. This check locks
all sibling header bytes; it does not recompile or prove the normal producer.
"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--bundle-header',type=Path,required=True);a=p.parse_args()
 n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(n.parent/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
 cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 files=[f for d in ('src','include','android','third_party')for f in(n/d).rglob('*')if f.is_file()and f.suffix in('.h','.c','.inc','.S')]
 files += [n/f for f in ('tests/window_source_probe.c','tests/window_source_probe.h','tests/window_interval_config.h','tests/d2r_bridge/window_interval_config.c','tests/window_interval_protocol.py','tools/window_interval_runtime.py','tools/run_window_source_intervals.py','tools/run_android_probe.py','tools/window_source_lifecycle.py','tools/check_window_intervals.py','tools/check_window_platform.py','tests/d2r_bridge/window_session.c')]
 files+=list(a.bundle_header.resolve().parent.glob('*.h'))+[cc.resolve()]
 inputs={str(f.resolve()):sha(f)for f in files};proof=dict(schema=1,passed=False,scope=__doc__,gpu_executed=False,android_executed=False,game_modules_executed=False,cadence_measured=False,inputs=inputs,runs=[])
 def run(label,argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
  proof['runs'].append(dict(label=label,argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise RuntimeError(label+' failed: '+r.stderr[-2000:])
  return r.stdout
 try:
  for compiler,flags in [('gcc',[]),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   label='config-'+compiler+('-san'if flags else'-normal');binary=out/label
   run(label+' build',[compiler,'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,n/'tests/d2r_bridge/window_interval_config.c','-o',binary])
   value=json.loads(run(label,[binary]));
   if value.get('passed')is not True or value.get('gpu_executes')!=0:raise RuntimeError('config fixture result/scope')
  run('Python runtime/batch/transport boundaries',[sys.executable,n/'tests/window_interval_protocol.py'])
  run('deterministic session snapshot + pacer regressions',[sys.executable,n/'tools/check_window_platform.py','--out',out/'window-platform','--cc','gcc','--sanitizers','--ndk',a.ndk.resolve()])
  flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-shared','-Wl,--no-undefined',*['-I'+str(n/d)for d in('include','third_party/nvn','include/nvn_native','src','android','third_party/kgsl')]]
  sources=[n/'src'/f'{m}.c'for m in('nvn','kgsl','texture_bridge','texture_source','compute','order','graphics','render_state','sampler_state','counter','external_image')]
  sources += [n/'android'/f'{m}.c'for m in('window_session','window_android','present','present_pacer','window_display','ahb')]
  run('actual backend + runtime Window probe API29 strict link',[cc,*flags,'-DNVN_WINDOW_SOURCE_BRIDGE_HEADER='+json.dumps(str(a.bundle_header.resolve())),*sources,n/'tests/window_source_probe.c','-landroid','-ldl','-lm','-o',out/'libwindow-interval-api29.so'])
  for f in ('tools/window_interval_runtime.py','tools/run_window_source_intervals.py','tools/run_android_probe.py','tools/window_source_lifecycle.py'):run('Python syntax '+f,[sys.executable,'-m','py_compile',n/f])
  if any(sha(f)!=value for f,value in inputs.items()):raise RuntimeError('input changed during qualification')
  proof['artifacts']={str(f):sha(f)for f in out.iterdir()if f.is_file()};proof['passed']=True
 except (OSError,ValueError,RuntimeError)as e:proof['error']=str(e)
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'proof.json'),gpu_executed=False,cadence_measured=False)))
 return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
