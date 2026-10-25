#!/usr/bin/env python3
"""Normal full-source catalogue/index/ownership HOST checks, API29 not executed."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def check(root,archive,ndk,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 d=json.loads(archive.read_text());inputs={str(archive):sha(archive)}
 if d.get('kind')!='certified-original-graphics-source-catalogue-v1'or d.get('passed')is not True or d.get('shader_compilation')is not False or d.get('native_variants')!=0:raise ValueError('normal original source-only archive required')
 for group in('inputs','artifacts'):
  for p,h in d[group].items():
   if sha(p)!=h:raise ValueError('changed normal source/product '+p)
   inputs[p]=h
 for rel in('tools/check_shader_sources.py','src/shader_source.c','include/nvn_native/shader_source.h','tests/d2r_bridge/shader_sources.c'):inputs[str(n/rel)]=sha(n/rel)
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';android=tools/'aarch64-linux-android29-clang'
 for t in[Path(shutil.which(k)).resolve()for k in('gcc','clang')]+[android.resolve(),tools/'ld.lld',tools/'llvm-nm',tools/'llvm-readelf']:inputs[str(t)]=sha(t)
 out.mkdir(parents=True);runs=[]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='abort_on_error=1:detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-8000:])
  return r.stdout
 inc=['-I'+str(n/'include')];defs=['-DNN_SHADER_SOURCE_HEADER='+json.dumps(d['header']),'-DNN_SHADER_SOURCE_MODULE='+json.dumps(str(n/'src/shader_source.c'))];results=[]
 for cc,san in[('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  o=out/(cc+'-blob.o');run([cc,'-c',d['assembly'],'-o',o]);binary=out/cc
  run([cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror',*san,'-fno-pie','-no-pie',*inc,*defs,n/'tests/d2r_bridge/shader_sources.c',d['source'],o,'-o',binary]);results.append(json.loads(run([binary])))
 if results[0]!=results[1]or results[0]['entries']!=len(d['entries'])or results[0]['program_deep_copies']!=len(d['entries']):raise ValueError('complete HOST catalogue ownership mismatch')
 obj=out/'blob-api29.o';run([android,'-c',d['assembly'],'-o',obj]);exe=out/'shader-sources-api29-not-run';run([android,'-std=c11','-O2','-Wall','-Wextra','-Werror',*inc,*defs,n/'tests/d2r_bridge/shader_sources.c',d['source'],obj,'-Wl,--no-undefined','-o',exe]);elf=run([tools/'llvm-readelf','-h','-d',exe]);(out/'api29-elf.txt').write_text(elf)
 if 'AArch64'not in elf or any(x in elf.lower()for x in('vulkan','gles','libegl')):raise ValueError('wrong native target/runtime')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 proof=dict(schema=1,kind='certified-source-catalogue-host-v1',passed=True,gpu_execution=False,android_execution=False,HOST=results[0],catalogue_blob_bytes=d['blob_bytes'],NDK_strict_link=True,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','archive','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.archive.resolve(),a.ndk.resolve(),a.out.resolve()))
