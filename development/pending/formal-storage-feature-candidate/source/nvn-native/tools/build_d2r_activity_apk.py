#!/usr/bin/env python3
"""Ignored APK candidate: one thin controller, one verified shared original host.

Build/verify only; no ADB, deployment, original module or GPU execution.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import zipfile


def sha(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f,'sha256').hexdigest()


def verify(mapping):
    if not isinstance(mapping,dict) or not mapping:
        raise ValueError('missing hash inventory')
    for name,expected in mapping.items():
        if not Path(name).is_file() or sha(name)!=expected:
            raise ValueError('integrity mismatch: '+name)


def host_verify(host,root):
    o=host['options']
    if (o['host_abi']!=2 or o['rewrite_abi']!='elfdesc' or o['rules']!='native' or
            str(o['private_gates'])!='1' or o.get('library_kind')!='shared' or
            o.get('execute') is not False or o.get('deploy') is not False):
        raise ValueError('verified native ELFdesc/HOST_ABI2 shared host required')
    for field in ('artifacts','sources','tools'):verify(host[field])
    argv=[o['python'],str(root/'nvn-native/tools/build_d2r_activity.py'),'--python',o['python'],
            '--exefs',o['exefs'],'--ndk',o['ndk'],'--out',str(Path(host['modules']).parent),
            '--jobs',str(o['jobs']),'--verify']
    if o.get('compute_cache_check'):argv+=['--compute-cache-check',o['compute_cache_check']]
    if o.get('texture_bridge_check'):argv+=['--texture-bridge-check',o['texture_bridge_check']]
    if o.get('block_copy_check'):argv+=['--block-copy-check',o['block_copy_check']]
    if o.get('volume_copy_check'):argv+=['--volume-copy-check',o['volume_copy_check']]
    if o.get('bc_sampled_fdl_check'):argv+=['--bc-sampled-fdl-check',o['bc_sampled_fdl_check']]
    if 'depth_targets' in o and type(o['depth_targets'])is not bool:raise ValueError('depth feature option must be boolean')
    if o.get('depth_targets'):argv+=['--depth-targets']
    if 'graphics_storage' in o and type(o['graphics_storage'])is not bool:raise ValueError('graphics storage option must be boolean')
    if o.get('graphics_storage'):argv+=['--graphics-storage']
    if 'depth_sampled' in o and type(o['depth_sampled'])is not bool:raise ValueError('depth sampling option must be boolean')
    if o.get('depth_sampled'):argv+=['--depth-sampled']
    for key in ('depth_fdl_check','depth_a2d_check','depth_runtime_check'):
        if o.get(key):argv+=['--'+key.replace('_','-'),o[key]]
    for option in ('sampled_images','sampled_mip_filter','gather'):
        if option in o and type(o[option])is not bool:raise ValueError('sampled feature option must be boolean: '+option)
        if o.get(option):argv+=['--'+option.replace('_','-')]
    if o.get('pipeline_target_fdl_check'):argv+=['--pipeline-target-fdl-check',o['pipeline_target_fdl_check']]
    for key in ('clear_color_kernel_check','clear_color_constant_check'):
        if o.get(key):argv+=['--'+key.replace('_','-'),o[key]]
    if o.get('color_targets'):argv+=['--color-targets']
    if o.get('bntx_packaged_layout_experiment'):argv+=['--bntx-packaged-layout-experiment']
    if o.get('graphics_pipeline_check'):argv+=['--graphics-pipeline-check',o['graphics_pipeline_check']]
    if 'shader_source_check' in o and o['shader_source_check'] is not None:
        if not isinstance(o['shader_source_check'],str) or not o['shader_source_check']:raise ValueError('source check option must be a path string')
        argv+=['--shader-source-check',o['shader_source_check']]
    return argv


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--host-record',type=Path,required=True)
    ap.add_argument('--sdk',type=Path,required=True)
    ap.add_argument('--ndk',type=Path,required=True)
    ap.add_argument('--keystore',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--verify',action='store_true')
    a=ap.parse_args()
    root=Path(__file__).resolve().parents[2]
    candidate=root/'nvn-native/android/d2r_activity'
    out=a.out.resolve();sdk=a.sdk.resolve();ndk=a.ndk.resolve();key=a.keystore.resolve();bt=sdk/'build-tools/36.0.0'
    if not out.is_relative_to(root/'.local') or out==candidate or out in candidate.parents:
        ap.error('separate ignored output directory required')
    host_path=a.host_record.resolve();host=json.loads(host_path.read_text());canonical=host_verify(host,root)
    if ndk!=Path(host['options']['ndk']).resolve():ap.error('controller and host must use recorded NDK')
    if not key.is_file():ap.error('reuse an existing probe keystore')
    inputs=[candidate/'controller.c',candidate/'CMakeLists.txt',Path(__file__),
            root/'nvn-native/adapters/d2r/activity/activity_host.h',
            root/'nvn-native/android/AndroidManifest.xml',
            root/'nvn-native/android/java/org/nvn/nativeprobe/PresentActivity.java',host_path]
    sources={str(p):sha(p) for p in inputs}
    sysroot=ndk/'toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/29'
    compiled_inputs={str(p):sha(p) for p in [Path(host['shared_library']),*[sysroot/n for n in ('crtbegin_so.o','crtend_so.o','libc.so','libandroid.so','libdl.so','libm.so')]]}
    tools=[bt/n for n in ('aapt2','zipalign','apksigner','d8')]+[bt/'lib/apksigner.jar',bt/'lib/d8.jar',
            sdk/'platforms/android-36/android.jar',key,ndk/'source.properties',ndk/'build/cmake/android.toolchain.cmake',
            ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang',ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld',
            ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf']
    tools += [Path(shutil.which(n)).resolve() for n in ('cmake','ninja','javac')]
    toolhash={str(p):sha(p) for p in tools};commands=[]
    def run(argv):
        argv=list(map(str,argv));r=subprocess.run(argv,cwd=root,text=True,capture_output=True)
        commands.append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise ValueError(r.stderr or r.stdout)
        return r.stdout
    record_path=out/'build-record.json';prior=json.loads(record_path.read_text()) if record_path.exists() else None
    if prior:verify(prior['artifacts'])
    options={'sdk':str(sdk),'ndk':str(ndk),'keystore':str(key),'host_record':str(host_path),
             'package':'org.nvn.nativeprobe','activity':'org.nvn.nativeprobe.PresentActivity',
             'controller_lib':'nvn_d2r_activity_controller','host_abi':2}
    if a.verify:
        if not prior or prior['sources']!=sources or prior['tools']!=toolhash or prior['options']!=options:
            ap.error('APK/source/tool/options changed')
        verify(prior['compiled_inputs']);run(canonical);print('verified '+str(record_path));return
    out.mkdir(parents=True,exist_ok=True);run(canonical)
    cmake=out/'cmake'
    run(['cmake','-S',candidate,'-B',cmake,'-G','Ninja',f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
         '-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo',
         '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',f'-DD2R_ACTIVITY_HEADER_DIR={root}/nvn-native/adapters/d2r/activity'])
    run(['cmake','--build',cmake,'--target','nvn_d2r_activity_controller'])
    controller=cmake/'libnvn_d2r_activity_controller.so';readelf=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
    elf=run([readelf,'-h','-d','-l','-s','--wide',controller])
    if 'AArch64' not in elf or not re.search(r'\bANativeActivity_onCreate$',elf,re.M):raise ValueError('controller ELF/entry missing')
    if re.search(r'\b(?:nvnBootstrapLoader|nn_device_open|dm_android_main)\b',elf):raise ValueError('controller must not link another host/core')
    needed=re.findall(r'\(NEEDED\).*?\[(.*?)\]',elf)
    if any(x not in ('libandroid.so','libdl.so','libm.so','libc.so') for x in needed):raise ValueError('unexpected controller dependency')
    manifest=out/'AndroidManifest.xml'
    manifest.write_text((root/'nvn-native/android/AndroidManifest.xml').read_text().replace('android:hasCode="false"','android:hasCode="true"')
        .replace('android:name="android.app.NativeActivity"','android:name="org.nvn.nativeprobe.PresentActivity"')
        .replace('android:value="nvn_kgsl_probe"','android:value="nvn_d2r_activity_controller"'))
    classes=out/'classes';dex=out/'dex';classes.mkdir(exist_ok=True);dex.mkdir(exist_ok=True)
    run(['javac','-source','8','-target','8','-classpath',sdk/'platforms/android-36/android.jar','-d',classes,inputs[5]])
    run([bt/'d8','--min-api','29','--lib',sdk/'platforms/android-36/android.jar','--output',dex,classes/'org/nvn/nativeprobe/PresentActivity.class'])
    raw=out/'unsigned.apk';aligned=out/'aligned.apk';apk=out/'d2r-activity.apk'
    run([bt/'aapt2','link','-I',sdk/'platforms/android-36/android.jar','--manifest',manifest,'-o',raw])
    hostlib=Path(host['shared_library'])
    with zipfile.ZipFile(raw,'a',compression=zipfile.ZIP_DEFLATED) as z:
        z.write(controller,'lib/arm64-v8a/'+controller.name);z.write(hostlib,'lib/arm64-v8a/libdmhost_activity_kgsl.so');z.write(dex/'classes.dex','classes.dex')
    run([bt/'zipalign','-f','4',raw,aligned]);run([bt/'apksigner','sign','--ks',key,'--ks-pass','pass:android','--out',apk,aligned]);run([bt/'apksigner','verify',apk])
    verify(sources);verify(toolhash);verify(compiled_inputs)
    for field in ('artifacts','sources','tools'):verify(host[field])
    with zipfile.ZipFile(apk) as z:
        if hashlib.sha256(z.read('lib/arm64-v8a/libdmhost_activity_kgsl.so')).hexdigest()!=sha(hostlib):raise ValueError('packaged host changed')
    audit=out/'controller-audit.txt';audit.write_text(elf)
    artifacts=[controller,apk,manifest,dex/'classes.dex',classes/'org/nvn/nativeprobe/PresentActivity.class',audit,cmake/'CMakeCache.txt',cmake/'compile_commands.json']+list(cmake.rglob('*.o'))
    rec={'schema':1,'operation':'d2r_activity_apk','options':options,'sources':sources,'tools':toolhash,
         'compiled_inputs':compiled_inputs,'artifacts':{str(p):sha(p) for p in artifacts},'commands':commands,
         'd2r_activity_host':host,'host_verify_argv':canonical,'controller_needed':needed,'apk':str(apk),
         'scope':'Build only. No deployment/Android lifecycle/GPU/original module execution. Controller dlopens exactly one verified original shared host.'}
    record_path.write_text(json.dumps(rec,indent=2)+'\n');print(record_path)


if __name__=='__main__':main()
