#!/usr/bin/env python3
"""Actual FDL typed buffer binder HOST/ARM64 contract checks. No GPU execution."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(root,views,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 d=json.loads(views.read_text());inputs={str(views):sha(views),str(Path(__file__).resolve()):sha(__file__)}
 if d.get('schema')!=1 or d.get('kind')!='native-typed-buffer-recipes-host-v1' or d.get('gpu_execution')is not False or d.get('actual_game_bindings')is not False:raise ValueError('unknown actual FDL metadata contract')
 for section in ['inputs','artifacts']:
  for p,h in d[section].items():
   if sha(p)!=h:raise ValueError('metadata source/artifact changed '+p)
   inputs[p]=h
 producer=root/'nvn-native/tools/texture_buffer_views.py';metadata=root/'nvn-native/tests/texture_buffer_metadata.cc';header=root/'nvn-native/include/nvn_native/texture_buffer.h'
 for p in [producer,metadata,header]:
  if d['inputs'].get(str(p))!=sha(p):raise ValueError('metadata came from a different current source '+str(p))
 if len(d['recipes'])!=2 or len(d['views'])!=50 or d['negative_checks']!=8:raise ValueError('unknown FDL comparison matrix')
 recipes=next(Path(p)for p in d['artifacts']if Path(p).name=='texture_buffer_recipes.h');exe=next(Path(p)for p in d['artifacts']if Path(p).name=='buffer-view')
 out.mkdir(parents=True);commands=[]
 def run(argv):
  r=subprocess.run(list(map(str,argv)),text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1'))
  commands.append(dict(argv=list(map(str,argv)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise ValueError(r.stderr)
  return r.stdout
 for r in d['recipes']:
  if json.loads(run([exe,'rgba8'if r['format']==37 else 'r32f']))!=r:raise ValueError('FDL recipe no longer reproducible')
 for row in d['views']:
  if json.loads(run([exe,'rgba8'if row['format']==37 else 'r32f',row['request_bytes'],row['request_address']]))!=row:raise ValueError('FDL descriptor no longer reproducible')
 expected=out/'expected.h';expected.write_text('struct expected_view{unsigned recipe;uint64_t address;size_t bytes;uint32_t words[16];};\nstatic const struct expected_view expected[]={\n'+',\n'.join('{'+','.join([str(0 if r['format']==37 else 1),str(r['request_address'])+'ull',str(r['request_bytes'])+'ull','{'+','.join(str(x)+'u'for x in r['descriptor'])+'}'])+'}'for r in d['views'])+'\n};\n')
 fixture=root/'nvn-native/tests/d2r_bridge/texture_buffer.c';module=root/'nvn-native/src/texture_buffer.c';kgsl=root/'nvn-native/src/kgsl.c'
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'nvn-native/include')]
 defines=['-DNN_TEX_BUFFER_RECIPES='+json.dumps(str(recipes)),'-DNN_TEX_BUFFER_EXPECTED='+json.dumps(str(expected))]
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  output=out/('texture-buffer-'+cc);run([cc,*flags,*defines,*san,fixture,module,'-o',output]);run([output])
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 run([cc,*flags,*defines,'-fPIC','-shared',fixture,module,'-Wl,--no-undefined','-o',out/'typed-test-arm64.so'])
 run([cc,*flags,'-I'+str(root/'nvn-native/third_party/kgsl'),'-fPIC','-shared',module,kgsl,'-Wl,--no-undefined','-o',out/'typed-real-kgsl-arm64.so'])
 for p in [fixture,module,kgsl,header,root/'nvn-native/include/nvn_native/kgsl.h',root/'nvn-native/include/nvn_native/compute_image.h',root/'nvn-native/third_party/kgsl/msm_kgsl.h',cc]:inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed during checks '+p)
 result=dict(schema=1,passed=True,gpu_execution=False,public_nvn_buffer_api=False,scope='HOST actual FDL descriptor/borrowed BO range and real ARM64 KGSL link only',inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 p=out/'check.json';p.write_text(json.dumps(result,indent=2)+'\n');return p
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--views-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(check(a.root.resolve(),a.views_check.resolve(),a.ndk.resolve(),a.out.resolve()))
