#!/usr/bin/env python3
"""Public canonical-copy API/common encoder ownership checks; HOST execution only."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from compile_block_copy import verify

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--kernel-check',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve();ndk=a.ndk.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 package=verify(root,a.kernel_check.resolve());image=Path(package['header']);out.mkdir(parents=True)
 proof=dict(schema=1,passed=False,gpu_execution=False,android_execution=False,scope='HOST actual public NVN canonical-copy/CopyCommands/replay API, mocked nonexecuting retained queue, NDK real backend link; no image-data GPU qualification',commands=[])
 def run(label,argv):
  r=subprocess.run(list(map(str,argv)),cwd=root,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['commands'].append(dict(label=label,argv=list(map(str,argv)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise ValueError(label+': '+r.stderr[-4000:])
  return r.stdout
 try:
  sources=[f for folder in ['include','third_party/nvn','third_party/kgsl','src','android'] for f in (n/folder).rglob('*')if f.is_file()]
  sources += [Path(__file__).resolve(),a.kernel_check.resolve(),image,n/'tests/d2r_bridge/canonical_copy.c',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc']
  proof['sources']={str(f):sha(f)for f in sources}
  modules=[n/'src'/f'{x}.c'for x in ['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge']]
  flags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'third_party/kgsl'),'-I'+str(n/'src')]
  defs=['-DNN_RESOURCE_CORE="'+str(n/'src/nvn.c')+'"','-DNN_RESOURCE_MEMORY_BACKEND="'+str(n/'tests/d2r_bridge/memory_backend.c')+'"','-DNN_BLOCK_COPY_IMAGE="'+str(image)+'"']
  proof['results']=[]
  for tool,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   binary=out/('canonical-copy-'+tool);run(tool+' compile',[tool,*flags,*defs,*san,n/'tests/d2r_bridge/canonical_copy.c',*modules,'-o',binary]);r=json.loads(run(tool+' execute',[binary]));assert r['passed']is True and r['formats']==37 and r['mock_submits']==151 and r['texture_copy_cases']==38 and r['gpu_execution']is False;proof['results'].append(r)
  cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['ndk_clang_sha256']=sha(cc)
  binary=out/'libcanonical-copy-real-api29.so';run('NDK29 actual backend strict link',[cc,*flags,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*modules,*[n/'android'/f'{x}.c'for x in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-o',binary])
  report=run('NDK29 ELF dependencies',[cc.parent/'llvm-readelf','-h','-d',binary]);assert 'AArch64'in report and not any(x in report.lower()for x in ['libvulkan','libegl','libgles'])
  if any(sha(f)!=h for f,h in proof['sources'].items()):raise ValueError('source closure changed during check')
  proof['artifacts']={str(binary):sha(binary)};proof['passed']=True
 except Exception as e:proof['error']=str(e)
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'proof.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
