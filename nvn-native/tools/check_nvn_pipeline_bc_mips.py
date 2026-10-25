#!/usr/bin/env python3
"""HOST oracle/strict collector tests and actual NDK public BC DSO link, no GPU."""
import argparse,copy,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
from nvn_pipeline_bc_mip_qualification import build,sha
from nvn_pipeline_bc_mip_validation import NAMES,pixels,validate

def run_check(root,check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];header,c,inputs=build(check,out,root=root)
 out.mkdir(parents=True);commands=[]
 def run(argv):
  r=subprocess.run(list(map(str,argv)),capture_output=True,text=True);commands.append(dict(argv=list(map(str,argv)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr or r.stdout)
  return r.stdout
 builder_rejections=[]
 for extra in (['--cs-case','1'],['--nvn-pipeline-gob-check',str(check)],['--nvn-pipeline-bc-check',str(check)],['--bc-sampled-check',str(check)],['--public-volume-copy-check',str(check)]):
  argv=[sys.executable,str(own/'tools/build_android_probe.py'),'--sdk',str(ndk.parent.parent),'--ndk',str(ndk),'--out',str(out/'forbidden-output'),'--nvn-pipeline-bc-mips-check',str(check),*extra]
  r=subprocess.run(argv,capture_output=True,text=True)
  if r.returncode!=2 or 'independent public NVN BC mip qualification'not in r.stderr or (out/'forbidden-output').exists():raise ValueError('builder combined incompatible mode')
  builder_rejections.append(extra)
 nonce='a'*32;uid=10001
 data=dict(schema=1,operation='nvn_bc_mip_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,submissions=96,finishes=96,completed_cases=96,recorded_handles=48,released_handles=48,replays=48,files=96,channel_checks=1572864,guard_byte_checks=786432,readonly_byte_checks=c['readonly_byte_checks'],texture_registrations=192,registrations_after_record=192,registered_cpu_object_finalizes=240,binding_array_copies=96,target_bytes=24576,target_offset=4096,target_width=64,target_height=64,target_pitch=256,graphics_control_profile_bytes=2176,source_levels=4,inflight_descriptor_updates=96,backing_inflight_busy_checks=96,backing_postfinish_recycles=96,authored_mip_uploads=384,selected_mip_changes=48,all_lods_pixel_qualified=False,mixed_unorm_srgb=True,typed_archive=True,passed=True,resources_retained=False,same_handle_replay=True,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False,maximum_channel_error=0,cases=c['cases'],run_nonce=nonce,run_directory='nvn-pipeline-'+nonce)
 captured={NAMES[2*x['case']+r]:b'\xa5'*4096+pixels(x,r)+b'\xa5'*4096 for x in c['cases']for r in range(2)}
 positive=validate(data,captured,c,nonce,uid);negatives=[]
 def reject(label,d,b,k=c,n=nonce,u=uid):
  try:validate(d,b,k,n,u)
  except (RuntimeError,ValueError,KeyError,TypeError):negatives.append(label);return
  raise ValueError('collector accepted bad input '+label)
 for key,bad in [('typed_archive',False),('mixed_unorm_srgb',False),('submissions',95),('finishes',95),('completed_cases',95),('recorded_handles',47),('released_handles',47),('replays',47),('files',95),('channel_checks',1),('guard_byte_checks',1),('readonly_byte_checks',1),('texture_registrations',191),('registrations_after_record',191),('registered_cpu_object_finalizes',239),('binding_array_copies',95),('graphics_control_profile_bytes',2200),('resources_retained',True),('cleanup_errno',5),('public_nvn',False),('cpu_image_conversion',True),('game_modules_executed',True),('uid',2000),('run_nonce','b'*32),('maximum_channel_error',2),('source_levels',1),('inflight_descriptor_updates',95),('backing_inflight_busy_checks',95),('backing_postfinish_recycles',95),('authored_mip_uploads',383),('selected_mip_changes',47),('all_lods_pixel_qualified',True)]:
  d=copy.deepcopy(data);d[key]=bad;reject(key,d,captured)
 d=copy.deepcopy(data);d['cases'][0]['format']=0x42;reject('source case format',d,captured)
 for key,bad in [('base0',3),('count0',1),('base1',0),('count1',4),('levels',1),('view',1)]:
  d=copy.deepcopy(data);d['cases'][0][key]=bad;reject('mip view '+key,d,captured)
 for label,change in [('missing',lambda b:b.pop(NAMES[-1])),('unexpected',lambda b:b.update({'other':b''})),('short',lambda b:b.update({NAMES[0]:b'error text'})),('guard',lambda b:b.update({NAMES[0]:b'\0'+b[NAMES[0]][1:]})),('RGB pixel',lambda b:b.update({NAMES[0]:b[NAMES[0]][:4096]+bytes([b[NAMES[0]][4096]^255])+b[NAMES[0]][4097:]})),('alpha pixel',lambda b:b.update({NAMES[0]:b[NAMES[0]][:4099]+bytes([b[NAMES[0]][4099]^1])+b[NAMES[0]][4100:]}))]:
  b=dict(captured);change(b);reject(label,data,b)
 # These are collector-only synthetic byte buffers, never a GPU/probe pass.
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 tools=[cc.resolve(),cc.parent/'llvm-readelf',Path(shutil.which('cmake')).resolve(),Path(shutil.which('ninja')).resolve()]
 sources={str(p):sha(p)for p in own.rglob('*')if p.is_file()and '__pycache__'not in p.parts}
 inputs.update(sources);inputs.update({str(p):sha(p)for p in tools})
 argv=['cmake','-S',own,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_NVN_PIPELINE_BC=ON','-DNVN_NVN_PIPELINE_BC_SRGB=OFF','-DNVN_NVN_PIPELINE_BC_MIPS=ON','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(header),'-DNVN_NVN_PIPELINE_BC_FDL_LIBRARY='+c['runtime_fdl_library']]
 run(argv);run(['cmake','--build',out/'cmake','--target','nvn_kgsl_probe','-j','6'])
 elf=out/'cmake/libnvn_kgsl_probe.so';info=run([tools[1],'-h','-d',elf]);(out/'elf.txt').write_text(info)
 if 'AArch64'not in info or any(x in info.lower()for x in ('libvulkan','libgles','libegl')):raise ValueError('wrong Android metadata/GPU primitive DSO dependencies')
 # Verify default BC=0 keeps all original color-probe code/data. Compile both
 # from the exact same qualification input with no debug/compiler identity.
 baseline=root/'nvn-native/tests/nvn_pipeline_probe.c'
 default_check=dict(executed=False,reason='baseline already contains this candidate; normal default source retained')
 if baseline.read_bytes()!=(own/'tests/nvn_pipeline_probe.c').read_bytes():
  default_header=out/'default.h';default_header.write_text('#include '+json.dumps(str(header))+'\n#define NN_PIPELINE_QUAL_PAIR_INDEX NN_BC_PUBLIC_PAIR\nstatic const struct nn_tex_native_view *const NN_PIPELINE_QUAL_VIEWS[]={&NN_TEXTURE_NATIVE_VIEWS[0],&NN_TEXTURE_NATIVE_VIEWS[0],&NN_TEXTURE_NATIVE_VIEWS[0]};\n')
  objcopy=cc.parent/'llvm-objcopy';texts=[]
  for label,source in [('baseline',baseline),('candidate',own/'tests/nvn_pipeline_probe.c')]:
   obj=out/(label+'.o');raw=out/(label+'.text')
   run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-g0','-fno-ident','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-I'+str(own/'tests'),'-DNVN_NVN_PIPELINE_BC=0','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(default_header)),'-c',source,'-o',obj]);run([objcopy,'--dump-section=.text='+str(raw),obj]);texts.append(raw.read_bytes())
  if texts[0]!=texts[1]:raise ValueError('default color probe machine text changed')
  default_check=dict(executed=True,identical_default_text_sha256=hashlib.sha256(texts[0]).hexdigest())
  legacy=[]
  for label,source in [('baseline',baseline),('candidate',own/'tests/nvn_pipeline_probe.c')]:
   obj=out/(label+'-legacy-bc.o');raw=out/(label+'-legacy-bc.text')
   run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-g0','-fno-ident','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-I'+str(own/'tests'),'-DNVN_NVN_PIPELINE_BC=1','-DNVN_NVN_PIPELINE_BC_MIPS=0','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-c',source,'-o',obj]);run([objcopy,'--dump-section=.text='+str(raw),obj]);legacy.append(raw.read_bytes())
  if legacy[0]!=legacy[1]:raise ValueError('default legacy singlemip BC text changed')
  default_check['identical_legacy_bc_text_sha256']=hashlib.sha256(legacy[0]).hexdigest()

 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('changed HOST/link input '+p)
 record=dict(schema=1,kind='public-nvn-bc-mip-sampled-host-check-v1',passed=True,android_execution=False,gpu_execution=False,game_frame=False,qualification_check=str(check),scope='C/Python authored oracle, collector negatives, actual normal CMake NDK whole-library strict link. Synthetic collector positives are NOT GPU execution.',synthetic_validator_positive=positive,validator_negatives=negatives,builder_rejections=builder_rejections,default_off_regression=default_check,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in ('.so','.o','.a','.text','.json','.txt')})
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(run_check(a.root.resolve(),a.check.resolve(),a.ndk.resolve(),a.out.resolve()))
if __name__=='__main__':main()
