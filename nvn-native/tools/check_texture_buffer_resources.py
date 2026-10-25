#!/usr/bin/env python3
"""HOST public TARGET_BUFFER front end + actual FDL recipes; no GPU/game."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--views-check',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);spec=importlib.util.spec_from_file_location('foundation',n/'tools/check_texture_buffers.py');f=importlib.util.module_from_spec(spec);spec.loader.exec_module(f)
 foundation=f.check(root,a.views_check.resolve(),a.ndk.resolve(),out/'foundation')
 d=json.loads(a.views_check.read_text());header=next(Path(s)for s in d['artifacts']if Path(s).name=='texture_buffer_recipes.h')
 inputs={str(q):sha(q)for folder in ('src','include','third_party/nvn','third_party/kgsl','android')for q in(n/folder).rglob('*')if q.is_file()};inputs.update({str(q):sha(q)for q in [Path(__file__).resolve(),n/'tests/d2r_bridge/texture_buffer_resource.c',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',header,foundation,a.views_check.resolve()]})
 commands=[]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr or r.stdout)
  return r.stdout
 defs=['-DNN_NVN_CORE_SOURCE='+json.dumps(str(n/'src/nvn.c')),'-DNN_NVN_MEMORY_BACKEND_SOURCE='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_TEX_BUFFER_RECIPES='+json.dumps(str(header))]
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
 modules=[n/'src'/(x+'.c')for x in ('texture_buffer','graphics','sampler_state','texture_source','texture_bridge','compute','external_image','order','render_state','counter','pipeline_resource_queries','pipeline_frontend')]
 wraps=['-Wl,--wrap='+x for x in ('nn_bo_alloc','nn_bo_cache','nn_queue_submit','nn_queue_submit_waits','calloc')]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  binary=out/('public-buffer-'+cc);run([cc,*flags,*defs,*san,n/'tests/d2r_bridge/texture_buffer_resource.c',n/'tests/d2r_bridge/memory_backend.c',*modules,*wraps,'-lm','-o',binary]);result=json.loads(run([binary]).strip().splitlines()[-1]);assert result['passed']and result['gpu_submits']==0
 cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc)]=sha(cc)
 android=[n/'android'/(x+'.c')for x in ('window_session','window_android','present','present_pacer','window_display','ahb')]
 run([cc,*flags,'-O2','-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*modules,*android,'-landroid','-ldl','-o',out/'libpublic-buffer-api29.so'])
 for q,h in inputs.items():
  if sha(q)!=h:raise ValueError('source/tool changed '+q)
 proof=dict(schema=1,passed=True,scope='HOST public resource/canonical ownership and actual FDL plus ARM64 KGSL link only',gpu_execution=False,game_modules_executed=False,inputs=inputs,foundation=str(foundation),commands_sha256=sha(out/'commands.json'),artifacts={str(q):sha(q)for q in out.iterdir()if q.is_file()})
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
if __name__=='__main__':main()
