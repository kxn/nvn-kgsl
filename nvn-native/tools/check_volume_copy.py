#!/usr/bin/env python3
"""HOST true3D metadata/binding and actual Android backend strict link; no GPU."""
import argparse,hashlib,json,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
 root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 sys.path.insert(0,str(n/'tools'));from compile_volume_copy import verify
 package=verify(root,a.kernel_check.resolve());header=Path(package['header']);out.mkdir();commands=[];results=[]
 proof=dict(schema=1,passed=False,gpu_execution=False,scope='HOST true3D layout/region/actual compiled kernel binding only; no GPU/public NVN/frame claim')
 inputs={str(a.kernel_check.resolve()):sha(a.kernel_check),**package['inputs'],**package['artifacts']}
 def run(argv):
  q=subprocess.run(list(map(str,argv)),capture_output=True,text=True);commands.append(dict(argv=list(map(str,argv)),returncode=q.returncode,stdout=q.stdout,stderr=q.stderr))
  if q.returncode:raise ValueError(q.stdout+q.stderr)
  return q.stdout
 try:
  sources=[n/'src/texture_source.c',n/'src/block_copy.c'];flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-DNN_VOLUME_COPY_HEADER='+json.dumps(str(header))]
  for cc,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   compiler=Path(shutil.which(cc)).resolve();inputs[str(compiler)]=sha(compiler)
   for fixture,modules in [('volume_source.c',sources[:1]),('volume_copy.c',sources)]:
    binary=out/(fixture[:-2]+'-'+cc);run([compiler,*flags,*san,n/'tests'/fixture,*modules,'-o',binary]);value=json.loads(run([binary]));assert value['passed'];results.append(dict(compiler=cc,test=fixture,**value))
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';target=out/'volume-real-kgsl-api29.so';inputs[str(cc.resolve())]=sha(cc)
  run([cc,*flags,'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared',*sources,n/'src/kgsl.c','-Wl,--no-undefined','-landroid','-ldl','-lm','-o',target]);elf=run([cc.parent/'llvm-readelf','-h','-d',target]);assert'AArch64'in elf
  proof['passed']=True
 except Exception as error:proof['error']=str(error)
 finally:
  for q in [Path(__file__).resolve(),n/'src/texture_source.c',n/'src/texture_volume.inc',n/'src/block_copy.c',n/'src/volume_copy.inc',n/'src/kgsl.c',n/'tests/volume_source.c',n/'tests/volume_copy.c',*sorted((n/'include/nvn_native').glob('*.h'))]:inputs[str(q)]=sha(q)
  proof.update(results=results,commands=commands,inputs=inputs,artifacts={str(q):sha(q)for q in out.iterdir()if q.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
