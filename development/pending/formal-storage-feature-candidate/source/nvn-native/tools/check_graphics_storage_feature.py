#!/usr/bin/env python3
"""Formal storage options/forwarding and actual ABI1/ABI2 ON/OFF NDK links.

This is source/ELF qualification, not public NVN GPU or original game execution.
"""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from graphics_storage_feature import prepare, audit, validate_options, sha


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check(a):
    root, ndk, out = a.root.resolve(), a.ndk.resolve(), a.out.resolve()
    native = Path(__file__).resolve().parents[1]
    if out.exists() or not out.is_relative_to(root / '.local'):
        raise ValueError('fresh ignored output required')
    out.mkdir(parents=True)
    inputs, commands, assertions, rejections, results = {}, [], 0, [], []
    def pin(p):
        p = Path(p).resolve(); h = sha(p)
        if str(p) in inputs and inputs[str(p)] != h:
            raise ValueError('storage feature input changed: ' + str(p))
        inputs[str(p)] = h
    def merge(mapping):
        for p, h in mapping.items():
            if p in inputs and inputs[p] != h:
                raise ValueError('storage feature dependency changed: ' + p)
            inputs[p] = h
    def require(v):
        nonlocal assertions
        if not v: raise ValueError('storage feature assertion failed')
        assertions += 1
    def reject(label, op):
        try: op()
        except (ValueError, OSError, KeyError, TypeError):
            rejections.append(label); return
        raise ValueError('invalid storage feature accepted: ' + label)
    def run(argv, expected=0):
        argv = list(map(str, argv)); p = subprocess.run(argv, text=True, capture_output=True)
        commands.append(dict(argv=argv, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr))
        (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if p.returncode != expected: raise ValueError((p.stdout+p.stderr)[-8000:])
        return p.stdout, p.stderr
    for folder in (native, root/'dm/host', root/'dm/third_party/runtime-codecs', root/'dm/third_party/libdeflate'):
        for p in folder.rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts: pin(p)
    for p in (a.names, root/'dm/gfx/hle_vi.c', root/'dm/gfx/hle_nv.c', ndk/'source.properties', ndk/'build/cmake/android.toolchain.cmake', Path(sys.executable).resolve()): pin(p)
    for tool in ('cmake', 'ninja'): pin(Path(shutil.which(tool)).resolve())
    for tool in ('clang', 'ld.lld', 'llvm-readelf'): pin(ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin'/tool)
    require(prepare(root,False,None,None,None)==(['-DNVN_D2R_GRAPHICS_STORAGE=OFF'],None,{}))
    for enabled in (0, 1, None, 'false', 'true', [], {}):
        reject('nonboolean storage '+repr(enabled),lambda enabled=enabled:validate_options(enabled,a.graphics_check,a.fdl_check))
    reject('missing all prerequisites',lambda:validate_options(True,None,None))
    reject('missing graphics',lambda:validate_options(True,None,a.fdl_check))
    reject('missing FDL',lambda:validate_options(True,a.graphics_check,None))
    # Use the existing records' exact normal producer paths; never rewrite an
    # input inventory to match an isolated candidate or bypass its validators.
    for name in ('package_graphics_stages','package_graphics_pipelines','graphics_pipeline_cache','build_runtime_fdl'):
        sys.modules[name]=load(root/'nvn-native/tools'/(name+'.py'),name)
    library, fdl = sys.modules['build_runtime_fdl'].verify(root,a.fdl_check)
    flags, manifest, mapping=prepare(root,True,a.graphics_check,a.fdl_check,library);merge(mapping)
    require(manifest['default_enabled'] is False and manifest['graphics_storage'] is True)
    require(manifest['gpu_qualification'] is False and manifest['original_module_execution'] is False)
    reject('different target FDL',lambda:prepare(root,True,a.graphics_check,a.fdl_check,out/'not-the-library.a'))
    reject('unpublished storage archive',lambda:prepare(root,True,a.no_storage_check,a.fdl_check,library))
    apk=load(native/'tools/build_d2r_activity_apk.py','storage_feature_apk')
    cli=load(native/'tools/run_d2r_host.py','storage_feature_cli')
    fake=out/'forwarding-fixture';fake.write_text('forwarding only; never deployment\n'); inv={str(fake):sha(fake)}
    options=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
    host=dict(options=options,modules=str(out/'modules'),artifacts=inv,sources=inv,tools=inv)
    for enabled in (False,True):
        h=copy.deepcopy(host);h['options'].update(graphics_storage=enabled,graphics_pipeline_check=str(a.graphics_check.resolve()),pipeline_target_fdl_check=str(a.fdl_check.resolve()))
        av=apk.host_verify(h,root);h['options'].update(host_abi=1,rewrite_abi='elf')
        for argv in (av,cli.host_verify_argv(h,root)):require(('--graphics-storage'in argv)==enabled)
    for value in (0,1,None,'false','true',[],{}):
        h=copy.deepcopy(host);h['options']['graphics_storage']=value
        reject('APK nonboolean '+repr(value),lambda h=h:apk.host_verify(h,root))
        h['options'].update(host_abi=1,rewrite_abi='elf')
        reject('CLI nonboolean '+repr(value),lambda h=h:cli.host_verify_argv(h,root))
    for name in ('build_d2r_host.py','build_d2r_activity.py'):
        _,err=run([sys.executable,native/'tools'/name,'--exefs','/must-not-read','--ndk',ndk,'--out',out/'must-not-build','--graphics-storage'],2)
        require('--graphics-storage requires'in err and not(out/'must-not-build').exists())
    toolchain=['-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29']
    for label,source,extra,diagnostic in (
        ('without-bridge',native,['-DNVN_D2R_HOST_BRIDGE=OFF'],'Original graphics storage requires'),
        ('without-graphics',native/'adapters/d2r/host',['-DNVN_D2R_NAMES_HEADER='+str(a.names.resolve()),'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY='+str(library)],'Original graphics storage requires'),
        ('without-fdl',native/'adapters/d2r/host',['-DNVN_D2R_NAMES_HEADER='+str(a.names.resolve()),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+manifest['graphics_header']],'Original graphics storage requires')):
        _,err=run(['cmake','-S',source,'-B',out/('reject-'+label),'-G','Ninja',*toolchain,'-DNVN_D2R_GRAPHICS_STORAGE=ON',*extra],1)
        require(diagnostic in err)
        rejections.append('CMake '+label)
    if not a.policy_only:
        for mode in ('host','activity'):
            for enabled in (False,True):
                dest=out/(mode+('-on'if enabled else'-off'));target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
                localflags,localmanifest,_=prepare(root,enabled,a.graphics_check,a.fdl_check,library)
                run(['cmake','-S',native/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_D2R_NAMES_HEADER='+str(a.names.resolve()),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+manifest['graphics_header'],'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY='+str(library),*localflags])
                run(['cmake','--build',dest,'--target',target,'--parallel','4'])
                actual,_=run(['ninja','-C',dest,'-t','commands',target]);audit(native,localmanifest,actual)
                compiled=[]
                for line in actual.splitlines():
                    argv=shlex.split(line)
                    if '-c'in argv:compiled.append(Path(argv[argv.index('-c')+1]).resolve())
                hs=[Path(p).resolve()for p in(dest/'host-sources.txt').read_text().splitlines()]
                require(len(hs)==len(set(hs))==36 and all(p in compiled for p in hs))
                require(len(compiled)==len(set(compiled)))
                core=('kgsl_probe','kgsl','nvn','compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','block_copy','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries','pipeline_views')
                expected=set(hs+[native/'src'/(n+'.c')for n in core]+[native/'android'/(n+'.c')for n in ('window_session','window_android','present','present_pacer','window_display','ahb')]+[native/'adapters/d2r/host_bridge.c'])
                if mode=='activity':expected.add(native/'adapters/d2r/activity/activity_host.c')
                if enabled:expected.add(native/'src/pipeline_storage.c')
                require(set(compiled)==expected)

                require(not any('/tests/'in str(p)or'/gfx/metal/'in str(p)or'/gfx/vulkan/'in str(p)for p in compiled))
                if enabled:
                    reject('missing pipeline macro '+mode,lambda:audit(native,localmanifest,actual.replace('-DNN_PIPELINE_GRAPHICS_STORAGE=1','')))
                    reject('missing bridge macro '+mode,lambda:audit(native,localmanifest,actual.replace('-DNN_NVN_GRAPHICS_STORAGE=1','')))
                    reject('duplicate storage source '+mode,lambda:audit(native,localmanifest,actual+'\n'+shlex.join(['clang','-c',str(native/'src/pipeline_storage.c'),'-o','duplicate.o'])))
                else:
                    reject('unrequested source '+mode,lambda:audit(native,None,actual+'\n'+shlex.join(['clang','-c',str(native/'src/pipeline_storage.c'),'-o','unrequested.o'])))
                elf=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl')
                info,_=run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf','-h','-d','-s','--wide',elf])
                require('AArch64'in info and not any(x in info.lower()for x in('libvulkan','libegl','libgles','libstdc++')))
                require(('nn_pipeline_storage_archive_validate'in info)==enabled)
                require(('NN_GRAPHICS_PIPELINE_STORAGE_ARCHIVE'in info)==enabled)
                results.append(dict(mode=mode,graphics_storage=enabled,original_sources=36,compiled_sources=len(compiled),elf=str(elf),sha256=sha(elf),bytes=elf.stat().st_size,android_execution=False))
    for p,h in inputs.items():
        if sha(p)!=h:raise ValueError('storage feature input changed during check: '+p)
    proof=dict(schema=1,kind='native-graphics-storage-feature-policy-v1'if a.policy_only else'native-graphics-storage-feature-links-v1',passed=True,assertions=assertions,rejections=rejections,results=results,storage_manifest=manifest,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in('.json','.txt','.so')},gpu_execution=False,android_execution=False,original_module_execution=False)
    (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('root','graphics-check','no-storage-check','fdl-check','names','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--policy-only',action='store_true')
    print(check(p.parse_args()))
