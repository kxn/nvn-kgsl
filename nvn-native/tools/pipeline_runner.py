"""Fresh-namespace Dynamic pipeline qualification, sharing normal verified APK transport."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import time
import uuid
from pipeline_validation import NAMES,validate
from texture_conversion_validation import parse_result
from run_android_probe import copy_package_uid,collect_copy_readbacks,validate_copy_inventory

def stopped(run,package):
    value=run('shell','pidof',package,check=False)
    # shell-v2 exit1 plus empty stdout/stderr is pidof's documented no-match.
    # Transport/permission/other execution failure is never proof of stop.
    ok=value.returncode==1 and not value.stdout.strip() and not value.stderr.strip()
    return ok,{'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr}

def profile(record):
    import numpy  # Explicit existing offline tool dependency, before ANY ADB.
    check=record.get('native_pipeline_check')
    if (type(check)is not dict or check.get('kind')!='raw-native-dynamic-pipeline-qualification-v1' or
        any(type(check.get(k))is not int or check[k]!=v for k,v in dict(schema=1,layouts=3,draw_modes=4,repeats=2,jobs=36,submissions=72,native_vs_ubo_binding=1,original_maxwell_cb=3,ubo_bytes=64).items())):
        raise RuntimeError('invalid checked dynamic pipeline build')
    if any(record.get(k) for k in ('nvn_api','shader_check','maxwell_check','graphics_check','nvn_graphics_check','texture_conversion_check','fullsize_display_check',
        'nvn_external_image_check','nvn_window_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','texture_check','nvn_texture_check','nvn_texture_bridge_check',
        'compute_driverparams_check','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','d2r_host','graphics_draw_batch','window_source_check','texture_bridge_diagnostics')):
        raise RuntimeError('dynamic pipeline must be the only operation')
    if any(k not in record['source'] for k in ('src/pipeline.c','src/pipeline_shader.c','src/pipeline_program.c','tests/pipeline_probe.c','android/pipeline_probe_activity.c')):
        raise RuntimeError('dynamic pipeline lacks actual runtime/probe source inventory')

def exercise(args,build,record,adb,package,component,run,qualification=None):
    q=qualification or {}
    names=q.get('names',NAMES);validator=q.get('validate',validate)
    check_key=q.get('check_key','native_pipeline_check')
    prefix=q.get('prefix','pipeline-');nonce_file=q.get('nonce_file','pipeline-run-nonce')
    q.get('profile',profile)(record)
    for name in names:
        p=args.evidence.parent/(args.evidence.stem+'-'+name)
        if p.exists() or p.is_symlink():raise RuntimeError('Dynamic pipeline readback evidence exists; choose fresh output')
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    if args.evidence.is_symlink():raise RuntimeError('Dynamic pipeline evidence must not be a symlink')
    evidence={'scope':q.get('scope','ordinary Android raw native dynamic pipeline with original color VS/FS; no NVN API or original game claim'),
              'build':record,'native_pipeline_verified':False,'stopped':False}
    def save():
        args.evidence.write_text(json.dumps(evidence,indent=2)+'\n')
    with args.evidence.open('x') as stream:
        stream.write(json.dumps(evidence,indent=2)+'\n')
    nonce=None
    try:
        if args.install:
            installed=run('install','--no-incremental','-r',str(build/'probe.apk'),timeout=180)
            if 'Success' not in installed.stdout:raise RuntimeError('APK install not confirmed')
        installed=run('shell','pm','path',package).stdout.strip().splitlines()
        if len(installed)!=1 or not installed[0].startswith('package:'):raise RuntimeError('expected one installed standalone APK')
        apk_hash=run('shell','sha256sum',installed[0][8:]).stdout.split()[0]
        if apk_hash!=record['artifacts'][str(build/'probe.apk')]:raise RuntimeError('installed APK differs from verified build')
        evidence['installed_apk_sha256']=apk_hash
        run('shell','am','force-stop',package)
        ok,detail=stopped(run,package);evidence['initial_stop']=detail
        if not ok:raise RuntimeError('old probe stop not confirmed')
        uid_text=run('shell','cmd','package','list','packages','-U',package).stdout
        uid=copy_package_uid(uid_text,package);evidence['package_uid']=uid_text
        nonce=uuid.uuid4().hex;directory=prefix+nonce;evidence['run_nonce']=nonce
        pending=nonce_file+'.'+nonce+'.pending'
        script=(f'set -eu; umask 077; test ! -e files/{directory}; test ! -L files/{directory}; '
                f'rm -f files/result.json; set -C; printf %s {nonce} > files/{pending}; '
                f'mv -f files/{pending} files/{nonce_file}')
        run('shell',shlex.join(['run-as',package,'sh','-c',script]))
        evidence['launch']=run('shell','am','start','-W','-n',component).stdout
        # One bounded correctness batch, no frame/performance measurements.
        deadline=time.monotonic()+180
        while True:
            result=run('shell','run-as',package,'cat','files/result.json',check=False)
            if result.returncode==0:
                data=parse_result(result.stdout);break
            if time.monotonic()>=deadline:raise RuntimeError('Dynamic pipeline probe produced no result within bounded qualification deadline')
            time.sleep(.1)
        evidence['result']=data
        evidence['fingerprint']=run('shell','getprop','ro.build.fingerprint').stdout.strip()
        evidence['kernel']=run('shell','uname','-r').stdout.strip()
        if data.get('run_nonce')!=nonce or data.get('run_directory')!=directory:
            raise RuntimeError('Dynamic pipeline result nonce/namespace mismatch')
        if data.get('passed') is not True:
            # Failure may have zero or a prefix of raw outputs. Preserve what
            # was actually saved, but never let a secondary collector error
            # replace the device GPU/owner failure.
            count=data.get('files')
            if type(count)is int and 0<count<=len(names):
                try:collect_copy_readbacks(run,adb,package,directory,names[:count],args.evidence,evidence)
                except BaseException as error:evidence['secondary_collection_error']=str(error)
            save()
            raise RuntimeError('Native dynamic pipeline failed: stage='+str(data.get('stage'))+
                ' errno='+str(data.get('errno'))+' cleanup_errno='+str(data.get('cleanup_errno'))+
                ' resources_retained='+str(data.get('resources_retained')))
        captured=collect_copy_readbacks(run,adb,package,directory,names,args.evidence,evidence)
        save()  # Preserve actual raw output even when the independent oracle fails.
        validate_copy_inventory(evidence,directory,names)
        evidence['native_pipeline_verification']=validator(data,captured,record[check_key],nonce,uid)
        evidence['native_pipeline_verified']=True
    except BaseException as error:
        evidence['error']=str(error);save();raise
    finally:
        try:
            stop=run('shell','am','force-stop',package,check=False)
            evidence['force_stop']={'returncode':stop.returncode,'stdout':stop.stdout,'stderr':stop.stderr}
            ok,detail=stopped(run,package);evidence['final_pidof']=detail
            evidence['stopped']=stop.returncode==0 and ok
        except BaseException as error:
            evidence['stop_error']=str(error);evidence['stopped']=False
        save()
    if not evidence['stopped']:
        evidence['native_pipeline_verified']=False;evidence['error']='Dynamic pipeline owner process stop was not confirmed';save()
        raise RuntimeError(evidence['error'])
    print(json.dumps(evidence['result'],indent=2))
