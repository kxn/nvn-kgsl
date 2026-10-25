#!/usr/bin/env python3
"""Actual NVN texel APIs: HOST argument/owner/cache boundaries and NDK link.
The mock queue never executes CP/image instructions; no GPU/Android claim.
"""
import argparse,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(root,kernel,ndk,out):
 n=root/'nvn-native'
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 out.mkdir(parents=True);sys.path.insert(0,str(n/'tools'));import public_copy_bundle
 header,bundle,inputs=public_copy_bundle.build(kernel,out/'inputs',root=root);proof=dict(schema=1,passed=False,GPU_execution=False,Android_execution=False,scope='Authored actual NVN texel API/metadata/owner/cache argument tests, nonexecuting HOST queues, ARM64 real backend link only',commands=[])
 for folder in ['src','include','third_party/nvn','third_party/kgsl','android']:
  for p in(n/folder).rglob('*'):
   if p.is_file():inputs[str(p)]=sha(p)
 for p in [Path(__file__).resolve(),n/'tests/d2r_bridge/texture_texels.c',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',n/'tests/d2r_bridge/resource_copy.c']:inputs[str(p)]=sha(p)
 proof['inputs']=inputs
 def run(label,argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['commands'].append(dict(label=label,argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if p.returncode:raise ValueError(label+': '+p.stderr[-4000:])
  return p.stdout
 modules=['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries'];sources=[n/'src'/f'{m}.c'for m in modules]
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/p)for p in ['include','third_party/nvn','third_party/kgsl','src','tests']]];defs=['-DNN_TEXELS_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_TEXELS_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_TEXELS_IMAGE='+json.dumps(str(header))]
 try:
  proof['results']=[]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   binary=out/('texels-'+cc);run(cc+' compile',[cc,*flags,*defs,*san,n/'tests/d2r_bridge/texture_texels.c',*sources,'-lm','-o',binary]);r=json.loads(run(cc+' HOST execute',[binary]));assert r['passed']is True and r['public_geometry_cases']==156 and r['mock_nonexecuting_submissions']==313 and r['actual_GPU_submissions']==0;proof['results'].append(r)
   # Existing CP readback/resource behavior remains an independent regression.
   binary=out/('legacy-'+cc);olddefs=['-DNN_RESOURCE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_RESOURCE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))];run(cc+' legacy resource compile',[cc,*flags,*olddefs,*san,n/'tests/d2r_bridge/resource_copy.c',*sources,'-lm','-o',binary]);proof.setdefault('legacy_results',[]).append(json.loads(run(cc+' legacy resource execute',[binary])))
  cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc)]=sha(cc);proof['ndk']={}
  for mode in [0,1]:
   binary=out/f'libtexture-texels-block{mode}-api29.so';f=[x for x in flags if not x.startswith('-DNN_NVN_BLOCK_COPY')]+[f'-DNN_NVN_BLOCK_COPY={mode}'];run('NDK real backend strict link '+str(mode),[cc,*f,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*sources,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run('NDK ELF '+str(mode),[cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(s in elf.lower()for s in ['libvulkan','libegl','libgles']);proof['ndk'][str(mode)]=dict(binary=str(binary),sha256=sha(binary))
  for p,h in inputs.items():
   if sha(p)!=h:raise ValueError('input changed during check '+p)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 proof['artifacts']={str(p):sha(p)for p in out.iterdir()if p.is_file()and p.name not in ['progress.json','check.json']};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--kernel-check',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();raise SystemExit(check(a.root.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))
