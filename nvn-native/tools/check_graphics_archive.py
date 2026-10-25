#!/usr/bin/env python3
"""HOST source-matching and NDK link check for the immutable stage-only archive.
No command interpreter, GPU or original game execution.
"""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--source-root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--check',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();r=a.root.resolve();src=a.source_root.resolve();out=a.out.resolve()
 if out.exists() or not out.is_relative_to(r/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);path=src/'tools/package_graphics_stages.py';s=importlib.util.spec_from_file_location('archive_packager',path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 header,manifest,_=m.build(a.check.resolve(),out/'archive',root=r);m.build(a.check.resolve(),out/'archive',root=r,verify=True);commands=[]
 common=['-std=c11','-Wall','-Wextra','-Werror','-O1','-g','-I'+str(src/'include'),'-DNN_EXPECTED_GRAPHICS_PAIR_COUNT='+str(manifest['pair_count']),'-DNN_OFFLINE_GRAPHICS_ARCHIVE_HEADER="'+str(header)+'"',str(src/'src/offline_graphics.c'),str(src/'tests/offline_graphics_test.c')]
 def run(argv,env=None):
  x=subprocess.run(argv,text=True,capture_output=True,env=env);commands.append({'argv':argv,'returncode':x.returncode,'stdout':x.stdout,'stderr':x.stderr});x.check_returncode();return x
 for cc,flags,name in [('clang',['-fsanitize=function','-fsanitize-trap=function'],'clang-function'),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer'],'gcc-asan')]:
  run([cc,*common,*flags,'-o',str(out/name)]);print(run([str(out/name)],dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1')).stdout.strip())
 clang=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';run([str(clang),*common,'-fPIC','-shared','-Wl,--no-undefined','-o',str(out/'libarchive-test.so')])
 negative=[]
 def tamper(path,offset,verify=False):
  before=path.read_bytes();changed=bytearray(before);changed[offset]^=1
  try:
   path.write_bytes(changed)
   try:m.build(a.check.resolve(),out/('archive' if verify else 'reject-'+str(len(negative))),root=r,verify=verify)
   except ValueError as e:negative.append({'path':str(path),'offset':offset,'rejection':str(e)})
   else:raise AssertionError('tampered stage dependency accepted')
  finally:path.write_bytes(before)
 tamper(header,-1,True)
 c=json.loads(a.check.read_text());e=next(x for x in c['stages'].values() if x['envelope']['inputs']['c1']['size']);fixture=Path(e['fixture']);d=json.loads(fixture.read_text())
 for name in ['code','control','c1']:tamper((fixture.parent/d['blobs'][name]['path']).resolve(),-1)
 pair=next(x for x in c['pairs'] if x['passed']);tamper(Path(pair['prefix']+'.vs.bin'),-1)
 m.build(a.check.resolve(),out/'archive',root=r,verify=True)
 proof={'schema':1,'scope':'HOST source-matching/ownership only; NDK link only','gpu_execution':False,'original_game_execution':False,'shader_stages':len(manifest['stages']),'native_pairs':manifest['pair_count'],'commands':commands,'artifact_rejections':negative,'inputs':manifest['inputs'],'tool_sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [Path(__file__).resolve(),path,src/'include/nvn_native/offline_graphics.h',src/'src/offline_graphics.c',src/'tests/offline_graphics_test.c']},'outputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [header,out/'libarchive-test.so']},'passed':True}
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
if __name__=='__main__':main()
