"""Fresh-namespace Texture bridge qualification, sharing normal verified APK transport."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import time
import uuid
from texture_bridge_validation import NAMES,validate
from texture_conversion_validation import parse_result
from run_android_probe import copy_package_uid,collect_copy_readbacks,validate_copy_inventory

def stopped(run,package):
    value=run('shell','pidof',package,check=False)
    # shell-v2 exit1 plus empty stdout/stderr is pidof's documented no-match.
    # Transport/permission/other execution failure is never proof of stop.
    ok=value.returncode==1 and not value.stdout.strip() and not value.stderr.strip()
    return ok,{'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr}

def profile(record):
    check=record.get('nvn_texture_bridge_check')
    if (not isinstance(check,dict) or any(type(check.get(k)) is not int for k in ('schema','cases','source_flags','target_flags','pool_flags')) or check.get('schema')!=1 or check.get('kind')!='nvn-texture-consumer-qualification-v1' or
        check.get('cases')!=4 or check.get('source_flags')!=8 or check.get('target_flags')!=9 or check.get('pool_flags')!=0xa4 or
        any(record.get(k) for k in ('nvn_api','shader_check','maxwell_check','graphics_check','nvn_graphics_check',
            'texture_conversion_check','fullsize_display_check','nvn_external_image_check','nvn_window_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','texture_check','nvn_texture_check',
            'compute_driverparams_check','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','d2r_host','graphics_draw_batch')) or
        type(record.get('texture_descriptor_flags',0)) is not int or record.get('texture_descriptor_flags',0)!=0):
        raise RuntimeError('invalid independent checked A650 conversion build')
    if any(k not in record['source'] for k in ('src/compute.c','src/texture_source.c','src/texture_bridge.c','src/texture_bridge_core.inc','tests/nvn_texture_bridge_probe.c','android/texture_bridge_activity.c')):
        raise RuntimeError('Texture bridge build lacks its actual runtime/probe source inventory')

def diagnostic_mask(record,requested):
    if requested is not None and (type(requested) is not int or not 0<=requested<=255):
        raise RuntimeError('Texture bridge diagnostic mask must be an integer 0..255')
    enabled=record.get('texture_bridge_diagnostics',False)
    if type(enabled) is not bool:
        raise RuntimeError('invalid Texture bridge diagnostic build marker')
    if enabled:
        if (any(record.get(key) for key in ('native_pipeline_check','window_source_check','window_source_slots',
                'fullsize_display_slots','external_image_slots','window_slots','texture_modes',
                'texture_noaccess_pool','texture_uncached_pools','compute_indirect_validation')) or
                record.get('texture_device_flags') is not None):
            raise RuntimeError('diagnostic mask is incompatible with other qualifier modes')
        profile(record)
        return 255 if requested is None else requested
    if requested is not None:
        raise RuntimeError('--texture-bridge-diagnostic-mask requires the independent diagnostic Texture bridge qualifier')
    return None

def diagnostic_payload(nonce,mask):
    if len(nonce)!=32 or any(c not in '0123456789abcdef' for c in nonce) or type(mask) is not int or not 0<=mask<=255:
        raise RuntimeError('invalid nonce/mask configuration')
    value=nonce+' '+format(mask,'03d')
    assert len(value)==36
    return value

def validate_diagnostic_result(data,mask):
    if (data.get('diagnostic_instrumented') is not (mask is not None) or
            (type(data.get('diagnostic_mask')) is not int or data['diagnostic_mask']!=mask
             if mask is not None else data.get('diagnostic_mask') is not None)):
        raise RuntimeError('diagnostic mask result differs from verified runtime request')

def exercise(args,build,record,adb,package,component,run):
    profile(record)
    mask=diagnostic_mask(record,getattr(args,"texture_bridge_diagnostic_mask",None))
    for name in NAMES:
        p=args.evidence.parent/(args.evidence.stem+'-'+name)
        if p.exists() or p.is_symlink():raise RuntimeError('Texture bridge readback evidence exists; choose fresh output')
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    if args.evidence.is_symlink():raise RuntimeError('Texture bridge evidence must not be a symlink')
    evidence={'scope':'ordinary Android actual NVN canonical GOB sample/RT writeback; no original game/fullsize/CPU_NO_ACCESS claim',
              'build':record,'texture_bridge_verified':False,'stopped':False,'diagnostic_mask_requested':mask}
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
        nonce=uuid.uuid4().hex;directory='texture-bridge-'+nonce;evidence['run_nonce']=nonce
        pending='texture-bridge-run-nonce.'+nonce+'.pending'
        script=(f'set -eu; umask 077; test ! -e files/{directory}; test ! -L files/{directory}; '
                f'rm -f files/result.json; set -C; printf %s {nonce} > files/{pending}; '
                f'mv -f files/{pending} files/texture-bridge-run-nonce')
        if mask is not None:
            payload=diagnostic_payload(nonce,mask)
            config_pending='texture-bridge-diagnostic-mask.'+nonce+'.pending'
            script+=(f'; printf %s {shlex.quote(payload)} > files/{config_pending}; '
                     f'mv -f files/{config_pending} files/texture-bridge-diagnostic-mask')
            evidence['diagnostic_configuration']={'nonce':nonce,'bytes':36,'mask':mask,
                'sha256':hashlib.sha256(payload.encode('ascii')).hexdigest()}
        run('shell',shlex.join(['run-as',package,'sh','-c',script]))
        if mask is not None:
            digest=run('shell','run-as',package,'sha256sum','files/texture-bridge-diagnostic-mask').stdout.split()
            if not digest or digest[0]!=evidence['diagnostic_configuration']['sha256']:
                raise RuntimeError('private diagnostic configuration differs from nonce/mask')
        evidence['launch']=run('shell','am','start','-W','-n',component).stdout
        # One bounded correctness batch, no frame/performance measurements.
        deadline=time.monotonic()+120
        while True:
            result=run('shell','run-as',package,'cat','files/result.json',check=False)
            if result.returncode==0:
                data=parse_result(result.stdout);break
            if time.monotonic()>=deadline:raise RuntimeError('Texture bridge probe produced no result within bounded qualification deadline')
            time.sleep(.1)
        evidence['result']=data
        evidence['fingerprint']=run('shell','getprop','ro.build.fingerprint').stdout.strip()
        evidence['kernel']=run('shell','uname','-r').stdout.strip()
        captured=collect_copy_readbacks(run,adb,package,directory,NAMES,args.evidence,evidence)
        save()  # Preserve actual raw output even when counts/overlap fail.
        validate_diagnostic_result(data,mask)
        if data.get('passed') is not True:
            raise RuntimeError('NVN texture consumer failed: stage='+str(data.get('stage'))+
                ' errno='+str(data.get('errno'))+' cleanup_errno='+str(data.get('cleanup_errno'))+
                ' resources_retained='+str(data.get('resources_retained')))
        validate_copy_inventory(evidence,directory,NAMES)
        evidence['texture_bridge_verification']=validate(data,captured,nonce,uid)
        evidence['texture_bridge_verified']=True
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
        evidence['texture_bridge_verified']=False;evidence['error']='Texture bridge owner process stop was not confirmed';save()
        raise RuntimeError(evidence['error'])
    print(json.dumps(evidence['result'],indent=2))
