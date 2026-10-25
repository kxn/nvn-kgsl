#!/usr/bin/env python3
"""Actual NVN metadata identity getters + weak-node ABA safety.
HOST RAM/session mocks and real ARM64 KGSL linkage, no GPU/Android execution.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,passed=False,scope=__doc__,GPU_execution=False,Android_execution=False,commands=[],inputs={})
 for folder in ['src','include','third_party/nvn','third_party/kgsl','android','tests/d2r_bridge']:
  for f in(n/folder).rglob('*'):
   if f.is_file():proof['inputs'][str(f)]=sha(f)
 proof['inputs'][str(Path(__file__).resolve())]=sha(__file__)
 modules=['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries'];sources=[n/'src'/f'{m}.c'for m in modules]
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
 defs=['-DNN_METADATA_LIFETIME_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_METADATA_LIFETIME_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))]
 def run(label,argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['commands'].append(dict(label=label,argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise ValueError(label+': '+r.stderr[-5000:])
  return r.stdout
 try:
  proof['results']=[];proof['previous_32_getters']=[];proof['window_owner_results']=[]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   binary=out/('metadata-lifetime-'+cc);tool=Path(subprocess.check_output(['which',cc],text=True).strip()).resolve();proof['inputs'][str(tool)]=sha(tool);run(cc+' actual core lifetime compile',[cc,*flags,*defs,*san,n/'tests/d2r_bridge/metadata_lifetime_getters.c',*sources,'-Wl,--wrap=calloc,--wrap=free','-lm','-o',binary]);result=json.loads(run(cc+' 13 getters and forced ABA HOST execute',[binary]));assert result['passed']is True and result['new_entries']==13 and result['forced_actual_node_reuses']==2 and result['GPU_submissions']==0;proof['results'].append(result)
   binary=out/('previous-32-'+cc);previous=['-DNN_METADATA_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_METADATA_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))];run(cc+' previous 32 compile',[cc,*flags,*previous,*san,n/'tests/d2r_bridge/metadata_getters.c',*sources,'-lm','-o',binary]);result=json.loads(run(cc+' previous 32 getters HOST execute',[binary]));assert result['passed']is True and result['new_entries']==32 and result['GPU_submissions']==0;proof['previous_32_getters'].append(result)
   binary=out/('window-owner-'+cc);run(cc+' actual Window owner/session fixture compile',[cc,*flags,*san,n/'tests/d2r_bridge/window_core.c',*sources,'-lm','-o',binary]);result=json.loads(run(cc+' live Window 2 and 3 slot getters and lifecycle HOST execute',[binary]));assert result['passed']is True and result['session_create_calls']>0;proof['window_owner_results'].append(result)
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc)]=sha(cc);proof['ndk']={}
  for mode in [0,1]:
   binary=out/f'libmetadata-lifetime-block{mode}-api29.so';f=[s for s in flags if not s.startswith('-DNN_NVN_BLOCK_COPY')]+[f'-DNN_NVN_BLOCK_COPY={mode}'];run('NDK actual KGSL backend strict link '+str(mode),[cc,*f,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*sources,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run('NDK ELF dependencies '+str(mode),[cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles']);proof['ndk'][str(mode)]=dict(path=str(binary),sha256=sha(binary))
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise ValueError('source changed '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 proof['artifacts']={str(f):sha(f)for f in out.iterdir()if f.is_file()and f.name not in ['progress.json','check.json']};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'),error=proof.get('error'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
