"""Fresh-namespace Sampler qualification, sharing normal verified APK transport."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import time
import uuid
from sampler_validation import NAMES,parse_result,validate
from run_android_probe import copy_package_uid,collect_copy_readbacks,validate_copy_inventory

def stopped(run,package):
    value=run('shell','pidof',package,check=False)
    # shell-v2 exit1 plus empty stdout/stderr is pidof's documented no-match.
    # Transport/permission/other execution failure is never proof of stop.
    ok=value.returncode==1 and not value.stdout.strip() and not value.stderr.strip()
    return ok,{'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr}

def profile(record):
    check=record.get('nvn_sampler_check')
    if (not isinstance(check,dict) or check.get('modes') is not True or check.get('image',{}).get('schema')!=3 or
        check.get('program',{}).get('chip_id')!=0x06050002 or
        any(record.get(k) for k in ('nvn_api','shader_check','maxwell_check','graphics_check','nvn_graphics_check',
            'nvn_external_image_check','nvn_window_check','nvn_copy_check','nvn_counter_check','texture_check','nvn_texture_check',
            'compute_driverparams_check','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','d2r_host','graphics_draw_batch')) or
        type(record.get('texture_descriptor_flags',0)) is not int or record.get('texture_descriptor_flags',0)!=0):
        raise RuntimeError('invalid independent checked A650 Sampler build')
    if any(k not in record['source'] for k in ('src/sampler_state.c','src/texture_source.c','tests/sampler_cases.h','tests/nvn_sampler_probe.c','android/sampler_probe_activity.c')):
        raise RuntimeError('Sampler build lacks its actual runtime/probe source inventory')

def exercise(args,build,record,adb,package,component,run):
    profile(record)
    for name in NAMES:
        p=args.evidence.parent/(args.evidence.stem+'-'+name)
        if p.exists() or p.is_symlink():raise RuntimeError('Sampler readback evidence exists; choose fresh output')
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    if args.evidence.is_symlink():raise RuntimeError('Sampler evidence must not be a symlink')
    evidence={'scope':'ordinary Android actual NVN Sampler qualification; no game/compositor',
              'build':record,'sampler_verified':False,'stopped':False}
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
        nonce=uuid.uuid4().hex;directory='sampler-'+nonce;evidence['run_nonce']=nonce
        pending='sampler-run-nonce.'+nonce+'.pending'
        script=(f'set -eu; umask 077; test ! -e files/{directory}; test ! -L files/{directory}; '
                f'rm -f files/result.json; set -C; printf %s {nonce} > files/{pending}; '
                f'mv -f files/{pending} files/sampler-run-nonce')
        run('shell',shlex.join(['run-as',package,'sh','-c',script]))
        evidence['launch']=run('shell','am','start','-W','-n',component).stdout
        # One bounded correctness batch, no frame/performance measurements.
        deadline=time.monotonic()+60
        while True:
            result=run('shell','run-as',package,'cat','files/result.json',check=False)
            if result.returncode==0:
                data=parse_result(result.stdout);break
            if time.monotonic()>=deadline:raise RuntimeError('Sampler probe produced no result within bounded qualification deadline')
            time.sleep(.1)
        evidence['result']=data
        evidence['fingerprint']=run('shell','getprop','ro.build.fingerprint').stdout.strip()
        evidence['kernel']=run('shell','uname','-r').stdout.strip()
        captured=collect_copy_readbacks(run,adb,package,directory,NAMES,args.evidence,evidence)
        save()  # Preserve actual raw output even when counts/overlap fail.
        validate_copy_inventory(evidence,directory,NAMES)
        evidence['sampler_verification']=validate(data,captured,nonce,uid)
        evidence['sampler_verified']=True
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
        evidence['sampler_verified']=False;evidence['error']='Sampler owner process stop was not confirmed';save()
        raise RuntimeError(evidence['error'])
    print(json.dumps(evidence['result'],indent=2))
