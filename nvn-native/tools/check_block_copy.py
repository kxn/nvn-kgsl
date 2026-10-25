#!/usr/bin/env python3
"""HOST raw-block address/argument/owner and actual ARM64 link checks. No GPU."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):
 s=importlib.util.spec_from_file_location('nn_block_package',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,kernel,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 producer=root/'nvn-native/tools/compile_block_copy.py';compiler=load(producer);package=compiler.verify(root,kernel)
 inputs={str(kernel):sha(kernel),**package['inputs'],**package['artifacts']}
 image=Path(package['header']);fixture=root/'nvn-native/tests/d2r_bridge/block_copy.c'
 modules=[root/'nvn-native/src'/n for n in ['block_copy.c','texture_source.c','compute.c']]
 headers=[root/'nvn-native/include/nvn_native'/n for n in ['block_copy.h','texture_source.h','compute.h','compute_image.h','kgsl.h','pm4.h']]
 extra=[root/'nvn-native/tests/block_copy_cases.h',root/'nvn-native/src/kgsl.c',root/'nvn-native/third_party/kgsl/msm_kgsl.h',Path(__file__).resolve(),producer]
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 for p in [fixture,*modules,*headers,*extra,cc]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1'));commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError('command failed '+str(r.returncode)+' '+r.stderr[-4000:])
  return r.stdout
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'nvn-native/include')];defines=['-DNN_BLOCK_COPY_IMAGE='+json.dumps(str(image))]
 for tool,san in [('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  binary=out/('block-copy-'+tool);run([tool,*flags,*defines,*san,fixture,*modules,'-o',binary]);r=json.loads(run([binary]));assert r['passed']is True and r['gpu_execution']is False and r['cases']==1776 and r['allocations']==r['destroys']==3552;results.append(dict(compiler=tool,**r));toolpath=Path(__import__('shutil').which(tool));inputs[str(toolpath)]=sha(toolpath)
 # Actual Android ELF linking with real KGSL backend, not the HOST fake BOs.
 run([cc,*flags,'-fPIC','-shared',*modules,root/'nvn-native/src/kgsl.c','-I'+str(root/'nvn-native/third_party/kgsl'),'-Wl,--no-undefined','-o',out/'block-copy-real-kgsl-arm64.so'])
 # Compile the typed fixture against the exact generated artifact on ARM64 too.
 run([cc,*flags,*defines,'-fPIC','-shared',fixture,*modules,'-Wl,--no-undefined','-o',out/'block-copy-typed-arm64.so'])
 readelf=cc.parent/'llvm-readelf';report=run([readelf,'-h','-d',out/'block-copy-real-kgsl-arm64.so']);(out/'elf.txt').write_text(report);inputs[str(readelf)]=sha(readelf)
 if 'AArch64'not in report or any(x in report.lower()for x in ['libvulkan','libegl','libgles']):raise ValueError('unexpected Android ELF contract')
 # Fresh-output rejection must happen before creating files/executing compilers.
 try:check(root,kernel,ndk,out)
 except ValueError as e:
  if 'fresh ignored'not in str(e):raise
 else:raise ValueError('existing-output guard did not reject')
 # Package corruption/unknown kinds reject before any compilation. Test copies
 # change only JSON claims, never the frozen compiler/kernel artifacts.
 negatives=[]
 for kind in ['unknown-kind','input-sha','artifact-sha']:
  bad=json.loads(kernel.read_text())
  if kind=='unknown-kind':bad['kind']='unknown'
  else:
   section='inputs'if kind=='input-sha'else'artifacts';bad[section][next(iter(bad[section]))]='0'*64
  badpath=out/(kind+'.json');badpath.write_text(json.dumps(bad))
  before=len(commands)
  try:compiler.verify(root,badpath)
  except ValueError:pass
  else:raise ValueError('corrupt package accepted '+kind)
  assert before==len(commands);negatives.append(kind)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during checks '+p)
 result=dict(schema=1,kind='native-general-block-copy-host-v1',passed=True,gpu_execution=False,android_execution=False,public_nvn_copy=False,scope='authored raw-block address/arguments/true-owner admission and actual ARM64 KGSL link only; no GPU behavior qualification',inputs=inputs,results=results,negative_packages=negatives,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});p=out/'check.json';p.write_text(json.dumps(result,indent=2)+'\n');return p
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(check(a.root.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))
