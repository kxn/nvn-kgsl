#!/usr/bin/env python3
"""Complete source binding normal HOST policy and actual ABI1/ABI2 links, no GPU."""
import argparse
import copy
import importlib.util
import json
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from workspace_paths import d2r_root
from shader_source_feature import prepare, audit, validate_options, sha


def load(path, name):
    s = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(s)
    s.loader.exec_module(m)
    return m


def check(args):
    root, ndk, out = args.root.resolve(), args.ndk.resolve(), args.out.resolve()
    native = Path(__file__).resolve().parents[1]
    if out.exists() or not out.is_relative_to(root / '.local'):
        raise ValueError('fresh ignored output required')
    out.mkdir(parents=True)
    inputs, commands, assertions, rejections = {}, [], 0, []
    def pin(path):
        path = Path(path).resolve()
        value = sha(path)
        if str(path) in inputs and inputs[str(path)] != value:
            raise ValueError('source feature input changed while pinning: ' + str(path))
        inputs[str(path)] = value
    def merge(mapping):
        for path,value in mapping.items():
            if path in inputs and inputs[path]!=value:
                raise ValueError('source feature dependency changed: '+path)
            inputs[path]=value
    def require(v):
        nonlocal assertions
        if not v:
            raise ValueError('source feature HOST assertion failed')
        assertions += 1
    def reject(label, operation):
        try:
            operation()
        except (ValueError, OSError, TypeError, KeyError):
            rejections.append(label)
            return
        raise ValueError('invalid source feature accepted: ' + label)
    def run(argv, expected=0):
        argv = list(map(str, argv))
        p = subprocess.run(argv, text=True, capture_output=True)
        commands.append(dict(argv=argv, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr))
        (out/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if p.returncode != expected:
            raise ValueError((p.stdout+p.stderr)[-8000:])
        return p.stdout, p.stderr
    # Pin source/tool/compiler closures BEFORE any compile/execute operation.
    for folder in (native, d2r_root()/'dm/host', d2r_root()/'dm/third_party/runtime-codecs', d2r_root()/'dm/third_party/libdeflate'):
        for p in folder.rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:
                pin(p)
    for p in (root/'nvn-native/tools').glob('*.py'):pin(p)
    pin(Path(sys.executable).resolve())
    for tool in ('cmake', 'ninja'):
        pin(Path(shutil.which(tool)).resolve())
    for p in (args.names, d2r_root()/'dm/gfx/hle_vi.c', d2r_root()/'dm/gfx/hle_nv.c', ndk/'source.properties', ndk/'build/cmake/android.toolchain.cmake'):
        pin(p)
    for tool in ('clang', 'ld.lld', 'llvm-readelf'):
        pin(ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin'/tool)
    require(prepare(root, None, None) == (['-DNVN_D2R_SHADER_SOURCE_HEADER=', '-DNVN_D2R_SHADER_SOURCE_C=', '-DNVN_D2R_SHADER_SOURCE_ASM=', '-DNVN_D2R_SHADER_SOURCE_BLOB='], None, {}))
    reject('source without native Draw directory', lambda: validate_options(args.source_check, None))
    for value in (False, True, 0, 1, [], {}, ''):
        reject('nonpath source option ' + repr(value), lambda value=value: validate_options(value, args.graphics_check))
    flags, manifest, mapping = prepare(root, args.source_check, args.graphics_check)
    merge(mapping)
    require(manifest['shader_compilation'] is False and manifest['complete_source_match'])
    apk = load(native/'tools/build_d2r_activity_apk.py', 'source_feature_apk')
    cli = load(native/'tools/run_d2r_host.py', 'source_feature_cli')
    fake = out/'verification-fixture';fake.write_text('HOST forwarding only\n')
    inv = {str(fake):sha(fake)}
    options=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
    host=dict(options=options,modules=str(out/'modules'),artifacts=inv,sources=inv,tools=inv)
    for enabled in (False, True):
        h = copy.deepcopy(host);h['options']['shader_source_check']=str(args.source_check.resolve()) if enabled else None
        h['options']['graphics_pipeline_check']=str(args.graphics_check.resolve())
        activity_argv=apk.host_verify(h,root)
        h['options'].update(host_abi=1,rewrite_abi='elf')
        for argv in (activity_argv,cli.host_verify_argv(h,root)):
            require(('--shader-source-check' in argv)==enabled)
            if enabled:require(argv[argv.index('--shader-source-check')+1]==h['options']['shader_source_check'])
    for value in (False,True,0,1,[],{},''):
        h=copy.deepcopy(host);h['options']['shader_source_check']=value
        reject('APK nonpath '+repr(value),lambda h=h:apk.host_verify(h,root))
        h['options'].update(host_abi=1,rewrite_abi='elf')
        reject('CLI nonpath '+repr(value),lambda h=h:cli.host_verify_argv(h,root))
    for name in ('build_d2r_activity.py','build_d2r_host.py'):
        _,err=run([sys.executable,native/'tools'/name,'--exefs','/must-not-read','--ndk',ndk,'--out',out/'must-not-build','--shader-source-check',args.source_check],2)
        require('--shader-source-check requires' in err and not (out/'must-not-build').exists())
    if args.policy_only:
        results=[]
    else:
        # Normal validators retain actual root source/provenance contracts. No
        # source/header/record is rewritten to fit this isolated integration.
        # Existing normal records bind their real producer path as well as
        # bytes. Validate through those original normal tools, never relabel a
        # candidate copy or rewrite an old record to fit an isolated source.
        for name in ('package_graphics_stages','package_graphics_pipelines','graphics_pipeline_cache','build_runtime_fdl','compile_block_copy'):
            module=load(root/'nvn-native/tools'/(name+'.py'),name)
            sys.modules[name]=module
        header,graphics=sys.modules['graphics_pipeline_cache'].build(root,args.graphics_check)
        fdl_library,fdl=sys.modules['build_runtime_fdl'].verify(root,args.fdl_check)
        kernel=sys.modules['compile_block_copy'].verify(root,args.block_check)
        merge(graphics['inputs']);merge(fdl['inputs']);merge(fdl['artifacts'])
        from native_depth_features import prepare as depth_prepare,audit as depth_audit
        from native_sampled_features import prepare as sampled_prepare,audit as sampled_audit
        results=[]
        for mode,enabled,depth in (('host',True,False),('activity',True,False),('activity',False,False),('activity',True,True)):
            dest=out/(mode+('-on'if enabled else'-off')+('-depth-sampled'if depth else''));target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
            localflags,localmanifest,_=prepare(root,args.source_check if enabled else None,args.graphics_check)
            extra=[];depthmanifest=sampledmanifest=None
            if depth:
                depthflags,depthmanifest,m=depth_prepare(root,True,args.depth_fdl_check,args.depth_a2d_check,args.depth_runtime_check,args.graphics_check,args.fdl_check,args.block_check,fdl_library,True,True);merge(m)
                sampledflags,sampledmanifest,m=sampled_prepare(root,True,True,True,args.graphics_check,args.fdl_check,args.block_check,None,fdl_library,None,True);merge(m)
                extra=['-DNVN_D2R_COLOR_TARGETS=ON',*depthflags,*sampledflags]
            run(['cmake','-S',native/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_D2R_NAMES_HEADER='+str(args.names.resolve()),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+str(header),'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY='+str(fdl_library),'-DNVN_D2R_BLOCK_COPY_HEADER='+kernel['header'],*localflags,*extra])
            run(['cmake','--build',dest,'--target',target,'--parallel','4'])
            actual,_=run(['ninja','-C',dest,'-t','commands',target]);audit(native,localmanifest,actual)
            if depth:depth_audit(native,depthmanifest,actual);sampled_audit(native,sampledmanifest,actual)
            compiled=[]
            for line in actual.splitlines():
                argv=shlex.split(line)
                if '-c'in argv:compiled.append(Path(argv[argv.index('-c')+1]).resolve())
            hs=[Path(p).resolve()for p in(dest/'host-sources.txt').read_text().splitlines()]
            require(len(hs)==len(set(hs))==36 and all(p in compiled for p in hs))
            require(not any('/tests/'in str(p)or'/gfx/metal/'in str(p)or'/gfx/vulkan/'in str(p)for p in compiled))
            if enabled:
                reject('missing actual source macro '+mode+str(depth),lambda:audit(native,localmanifest,actual.replace('-DNN_PIPELINE_SOURCE_CATALOGUE=1','')))
                bad=actual+'\n'+shlex.join(['clang','-c',str(native/'src/shader_source.c'),'-o','duplicate.o'])
                reject('duplicate source '+mode+str(depth),lambda:audit(native,localmanifest,bad))
            elf=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl')
            info,_=run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf','-h','-d','-s','--wide',elf])
            require('AArch64'in info and not any(x in info.lower()for x in('libvulkan','libegl','libgles','libstdc++')))
            require(('nn_complete_source_blob'in info)==enabled)
            require(('nn_depth_sampled_create'in info)==depth)
            results.append(dict(mode=mode,source_catalogue=enabled,depth_sampled=depth,original_sources=36,compiled_sources=len(compiled),elf=str(elf),sha256=sha(elf),bytes=elf.stat().st_size,android_execution=False))
    for p,h in inputs.items():
        if sha(p)!=h:raise ValueError('input changed during source feature check: '+p)
    proof=dict(schema=1,kind='complete-source-host-feature-policy-v1'if args.policy_only else'complete-source-host-feature-links-v1',passed=True,assertions=assertions,rejections=rejections,results=results,source_manifest=manifest,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in('.json','.txt','.so')},gpu_execution=False,android_execution=False,original_module_execution=False)
    (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in('root','source-check','graphics-check','fdl-check','block-check','depth-fdl-check','depth-a2d-check','depth-runtime-check','names','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--policy-only',action='store_true')
    print(check(p.parse_args()))
