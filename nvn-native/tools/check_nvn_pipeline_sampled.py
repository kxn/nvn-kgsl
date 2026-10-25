#!/usr/bin/env python3
"""HOST collector/oracle negatives + actual NDK full public sampled22 DSO. No GPU."""
import argparse,copy,hashlib,json,shlex,subprocess,sys
from pathlib import Path
from nvn_pipeline_sampled_qualification import build,sha,load
from nvn_pipeline_sampled_validation import NAMES,pixels,validate,compact

def run_check(root,check,legacy_bridge,ndk,out,baseline_root=None,baseline_probe=None):
 baseline_root=(baseline_root or root).resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];header,c,inputs=build(check,out,root=root);out.mkdir(parents=True);commands=[]
 def run(argv):
  a=list(map(str,argv));r=subprocess.run(a,capture_output=True,text=True);commands.append(dict(argv=a,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stderr+r.stdout)[-6000:])
  return r.stdout
 builder_rejections=[]
 for extra in (['--cs-case','1'],['--native-sampler-mip-check',str(check)],['--nvn-pipeline-buffer-check',str(check)],['--nvn-pipeline-bc-mips-check',str(check)],['--clear-color-check',str(check)]):
  argv=[sys.executable,own/'tools/build_android_probe.py','--sdk',ndk.parent.parent,'--ndk',ndk,'--out',out/'must-not-exist','--nvn-pipeline-sampled-check',check,*extra];r=subprocess.run(list(map(str,argv)),capture_output=True,text=True)
  if r.returncode!=2 or 'independent public sampled22 qualification'not in r.stderr or(out/'must-not-exist').exists():raise ValueError('incompatible builder modes accepted')
  builder_rejections.append(extra)
 nonce='a'*32;uid=10001
 data=dict(schema=1,operation='nvn_float2d_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,submissions=264,finishes=264,completed_cases=264,recorded_handles=132,released_handles=132,replays=132,files=264,channel_checks=4325376,guard_byte_checks=2162688,readonly_byte_checks=c['readonly_byte_checks'],texture_registrations=528,registrations_after_record=528,registered_cpu_object_finalizes=660,binding_array_copies=264,target_bytes=24576,target_offset=4096,target_width=64,target_height=64,target_pitch=256,graphics_control_profile_bytes=2176,source_levels=4,inflight_descriptor_updates=264,backing_inflight_busy_checks=264,backing_postfinish_recycles=264,authored_mip_uploads=1056,selected_mip_changes=132,floating_result_formats=22,passed=True,resources_retained=False,same_handle_replay=True,public_nvn=True,typed_archive=True,all_lods_pixel_qualified=False,cpu_image_conversion=False,game_modules_executed=False,offline_one_texel_patterns=True,native_mip_filter=False,gather=False,maximum_channel_error=0,cases=compact(c['cases']),run_nonce=nonce,run_directory='nvn-pipeline-'+nonce)
 if len(json.dumps(data,separators=(',',':')))>32768-32:raise ValueError('real Activity result buffer would overflow')
 captured={NAMES[2*x['case']+r]:b'\xa5'*4096+pixels(x,r,c['patterns'])+b'\xa5'*4096 for x in c['cases']for r in range(2)}
 positive=validate(data,captured,c,nonce,uid);negatives=[]
 def reject(label,d,b,k=c,n=nonce,u=uid):
  try:validate(d,b,k,n,u)
  except(RuntimeError,ValueError,KeyError,TypeError):negatives.append(label);return
  raise ValueError('invalid sampled22 collection accepted '+label)
 fields=[('resources_retained',True),('cleanup_errno',5),('uid',2000),('run_nonce','b'*32),('maximum_channel_error',2),('source_levels',1),('typed_archive',False),('gather',True),('native_mip_filter',True),('all_lods_pixel_qualified',True),('cpu_image_conversion',True),('game_modules_executed',True),('offline_one_texel_patterns',False),('public_nvn',False)]
 for key in('submissions','finishes','completed_cases','recorded_handles','released_handles','replays','files','channel_checks','guard_byte_checks','readonly_byte_checks','texture_registrations','registrations_after_record','registered_cpu_object_finalizes','binding_array_copies','graphics_control_profile_bytes','inflight_descriptor_updates','backing_inflight_busy_checks','backing_postfinish_recycles','authored_mip_uploads','selected_mip_changes','floating_result_formats'):fields.append((key,data[key]-1))
 for key,bad in fields:d=copy.deepcopy(data);d[key]=bad;reject(key,d,captured)
 for key in('case','format','width','height','levels','view','base0','count0','base1','count1'):
  d=copy.deepcopy(data);d['cases'][0][key]+=1;reject('case '+key,d,captured)
 for name,change in [('missing',lambda b:b.pop(NAMES[-1])),('unexpected',lambda b:b.update({'other':b''})),('short',lambda b:b.update({NAMES[0]:b'error text'})),('guard',lambda b:b.update({NAMES[0]:b'\0'+b[NAMES[0]][1:]})),('RGB',lambda b:b.update({NAMES[0]:b[NAMES[0]][:4096]+bytes([b[NAMES[0]][4096]^255])+b[NAMES[0]][4097:]})),('alpha',lambda b:b.update({NAMES[0]:b[NAMES[0]][:4099]+bytes([b[NAMES[0]][4099]^255])+b[NAMES[0]][4100:]}))]:
  b=dict(captured);change(b);reject('raw '+name,data,b)
 # Collector-only synthetic buffers above are NOT probe/GPU execution.
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf';nm=cc.parent/'llvm-nm'
 inputs.update({str(p):sha(p)for p in own.rglob('*')if p.is_file()and'__pycache__'not in p.parts})
 for p in(cc.resolve(),readelf,nm):inputs[str(p)]=sha(p)
 args=['cmake','-S',own,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_NVN_PIPELINE_PROBE=ON','-DNVN_NVN_PIPELINE_SAMPLED=ON','-DNVN_NVN_PIPELINE_RUNTIME_VIEWS=ON','-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+str(header),'-DNVN_NVN_PIPELINE_RUNTIME_FDL_LIBRARY='+c['runtime_fdl_library']]
 run(args);run(['cmake','--build',out/'cmake','--target','nvn_kgsl_probe','--parallel','4']);actual=run(['ninja','-C',out/'cmake','-t','commands','nvn_kgsl_probe'])
 for name in('src/sampled_image.c','src/texture_bc_shadow.c','src/pipeline_views.c','tests/nvn_pipeline_probe.c'):
  count=sum('-c'in(parts:=shlex.split(line))and Path(parts[parts.index('-c')+1]).resolve()==(own/name).resolve()for line in actual.splitlines())
  if count!=1:raise ValueError('sampled22 actual source missing/duplicate '+name)
 for macro in('NN_PIPELINE_SAMPLED_IMAGES=1','NN_NVN_SAMPLED_IMAGES=1','NN_NVN_BLOCK_COPY=1','NN_PIPELINE_RUNTIME_VIEWS=1'):
  if '-D'+macro not in actual:raise ValueError('sampled22 actual mode absent '+macro)
 if any(x in actual for x in('NN_PIPELINE_GATHER=1','NN_PIPELINE_SAMPLED_MIP_FILTER=1','NN_PIPELINE_BC_SAMPLED=1','sampled_fixture_pack.c')):raise ValueError('unexpected consumer/CPU fixture in actual sampled22 build')
 elf=out/'cmake/libnvn_kgsl_probe.so';info=run([readelf,'-h','-d',elf]);symbols=run([nm,'--defined-only',elf]);(out/'link-audit.txt').write_text(actual+'\n'+info+'\n'+symbols)
 if 'AArch64'not in info or any(x in info.lower()for x in('libvulkan','libgles','libegl','libstdc++')):raise ValueError('unexpected Android library/backend')
 if any(x in symbols for x in('util_format_unpack','util_format_read','util_format_write','nn_clear_color_pattern','pack_rgba_float')):raise ValueError('HOST CPU format conversion leaked into Android DSO')
 # Recompile identical old/default source profiles under both files and compare
 # actual target .text, without relabeling any legacy artifact as new GPU proof.
 bh,bm=load(baseline_root/'nvn-native/tools/texture_bridge_bundle.py','sampled_legacy_bridge').build(baseline_root,legacy_bridge);inputs.update(bm['inputs']);inputs.update(bm['artifacts']);inputs[str(legacy_bridge)]=sha(legacy_bridge)
 default_header=out/'legacy-header.h';p=json.loads(Path(c['pipeline_check']).read_text());default_header.write_text('#include '+json.dumps(str(Path(c['pipeline_check']).parent/'offline_graphics_pipelines.h'))+'\n#include '+json.dumps(str(Path(bh).with_name('texture_native_views.h')))+'\n#include '+json.dumps(str(bh))+'\n#include '+json.dumps(json.loads(Path(c['kernel_check']).read_text())['header'])+'\n#define NN_BC_PUBLIC_PAIR '+str(c['pair_index'])+'u\n#define NN_BC_BLOCK_IMAGE NVN_COMPILED_IMAGE\n#define NN_PIPELINE_QUAL_PAIR_INDEX NN_BC_PUBLIC_PAIR\nstatic const struct nn_tex_native_view *const NN_PIPELINE_QUAL_VIEWS[]={&NN_TEXTURE_NATIVE_VIEWS[0],&NN_TEXTURE_NATIVE_VIEWS[0],&NN_TEXTURE_NATIVE_VIEWS[0]};\n')
 regression=[]
 for bc in(0,1):
  texts=[]
  for label,source in(('baseline',baseline_probe or baseline_root/'nvn-native/tests/nvn_pipeline_probe.c'),('candidate',own/'tests/nvn_pipeline_probe.c')):
   inputs[str(source)]=sha(source);o=out/(label+str(bc)+'.o');text=out/(label+str(bc)+'.text')
   run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-g0','-fno-ident','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-I'+str(own/'tests'),'-DNVN_NVN_PIPELINE_BC='+str(bc),'-DNVN_NVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(default_header)),'-c',source,'-o',o]);run([cc.parent/'llvm-objcopy','--dump-section=.text='+str(text),o]);texts.append(text.read_bytes())
  if texts[0]!=texts[1]:raise ValueError('default/legacyBC machine text changed')
  regression.append(dict(bc=bc,text_sha256=hashlib.sha256(texts[0]).hexdigest()))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('sampled22 normal input changed '+p)
 result=dict(schema=1,kind='public-nvn-float2d-sampled-host-check-v1',passed=True,qualification_check=str(check),validator_synthetic_only=True,validator_positive=positive,validator_negatives=negatives,builder_rejections=builder_rejections,default_off_regression=regression,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in('.so','.o','.a','.text','.json','.txt')},android_execution=False,gpu_execution=False,game_modules_executed=False)
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','check','legacy-bridge-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--baseline-root',type=Path,help='Optional immutable old/default profile tree for byte-equivalence; qualification still uses --root')
 p.add_argument('--baseline-probe',type=Path,help='Explicit frozen pre-change public probe for genuine default/BC ARM64 byte-equivalence')
 x=p.parse_args();print(run_check(x.root.resolve(),x.check.resolve(),x.legacy_bridge_check.resolve(),x.ndk.resolve(),x.out.resolve(),x.baseline_root,x.baseline_probe))
