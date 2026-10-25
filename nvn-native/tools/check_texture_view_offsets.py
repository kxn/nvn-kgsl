#!/usr/bin/env python3
"""HOST original-layout ViewOffset query + native ABI link; no GPU/game."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);commands=[]
 files=[q for d in ('src','include','third_party/nvn','third_party/kgsl','android')for q in(n/d).rglob('*')if q.is_file()]+[Path(__file__).resolve(),n/'tests/d2r_bridge/texture_view_offset.c',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc']
 inputs={str(q):sha(q)for q in files}
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr or r.stdout)
  return r.stdout
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')];defs=['-DNN_NVN_CORE_SOURCE='+json.dumps(str(n/'src/nvn.c'))]
 modules=[n/'src'/(q+'.c')for q in ('graphics','sampler_state','texture_source','texture_bridge','compute','external_image','order','render_state','counter')]
 wraps=['-Wl,--wrap='+q for q in ('nn_bo_alloc','nn_bo_map','nn_bo_cache','nn_queue_submit')]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  compiler=Path(shutil.which(cc)).resolve();inputs[str(compiler)]=sha(compiler);binary=out/('view-offset-'+cc);run([compiler,*flags,*defs,*san,n/'tests/d2r_bridge/texture_view_offset.c',n/'tests/d2r_bridge/memory_backend.c',*modules,*wraps,'-o',binary]);r=json.loads(run([binary]).strip().splitlines()[-1]);assert r['passed']and r['gpu_submits']==0
 cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc)]=sha(cc)
 android=[n/'android'/(q+'.c')for q in ('window_session','window_android','present','present_pacer','window_display','ahb')]
 run([cc,*flags,'-O2','-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*modules,*android,'-landroid','-ldl','-o',out/'libview-offset-api29.so'])
 for q,h in inputs.items():
  if sha(q)!=h:raise ValueError('source/tool changed '+q)
 proof=dict(schema=1,passed=True,scope='HOST typed CPU layout/query/ownership only and actual ARM64 KGSL/Android link; no GPU/game',gpu_execution=False,game_modules_executed=False,inputs=inputs,commands_sha256=sha(out/'commands.json'),artifacts={str(q):sha(q)for q in out.iterdir()if q.is_file()});(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
if __name__=='__main__':main()
