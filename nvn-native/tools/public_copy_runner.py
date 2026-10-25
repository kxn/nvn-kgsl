"""Fresh-namespace Public NVN copy qualification, sharing normal verified APK transport."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import time
import uuid
import public_copy_validation
from public_copy_validation import parse_result
def _oracle(volume,shadow=False,packaged=False,volume_texture=False, buffer_texture=False):
    if buffer_texture:
        import buffer_texture_validation
        return buffer_texture_validation
    if volume_texture:
        import volume_texture_validation
        return volume_texture_validation
    if packaged:
        import public_packaged_copy_validation
        return public_packaged_copy_validation
    if shadow:
        import volume_shadow_validation
        return volume_shadow_validation
    if volume:
        import public_volume_copy_validation
        return public_volume_copy_validation
    return public_copy_validation
from run_android_probe import copy_package_uid,collect_copy_readbacks,validate_copy_inventory

def stopped(run,package):
    value=run('shell','pidof',package,check=False)
    # shell-v2 exit1 plus empty stdout/stderr is pidof's documented no-match.
    # Transport/permission/other execution failure is never proof of stop.
    ok=value.returncode==1 and not value.stdout.strip() and not value.stderr.strip()
    return ok,{'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr}

def profile(record, *, volume=False, shadow=False, packaged=False, volume_texture=False, buffer_texture=False):
    if buffer_texture:
        from buffer_texture_runner import profile as buffer_profile
        return buffer_profile(record)
    if volume_texture:
        from volume_texture_runner import profile as texture_profile
        return texture_profile(record)
    if shadow:
        from volume_shadow_runner import profile as shadow_profile
        return shadow_profile(record)
    call=record.get('public_copy_call',False)
    if type(call)is not bool or (call and packaged):raise RuntimeError('invalid public Call copy build flag')
    if call and any(k not in record.get('source',{})for k in ('src/nvn_command_copy_types.inc','src/nvn_command_copy_cp_record.inc','src/nvn_command_copy_canonical_record.inc','src/nvn_command_copy_bound_clear.inc','src/nvn_command_copy_projection.inc','src/nvn_command_sequence.inc','src/nvn_command_projection.inc')):raise RuntimeError('Call copy build lacks its semantic source inventory')
    oracle=_oracle(volume,packaged=packaged);key="public_packaged_copy_check"if packaged else"public_volume_copy_check" if volume else "public_copy_check"
    check=record.get(key)
    if (not isinstance(check,dict) or check.get('schema')!=1 or check.get('kind')!=('public-nvn-packaged-bntx-copy-experiment-v1'if packaged else'public-nvn-volume-copy-qualification-v1'if volume else'public-nvn-canonical-copy-qualification-v1') or
        check.get('gpu_execution') is not False or len(check.get('cases',[]))!=(72 if packaged else 228 if volume else 234) or
        any(record.get(k)for k in ('texture_texels_check','bc_sampled_check',('public_copy_check'if volume else'public_volume_copy_check'))) or
        any(record.get(k) for k in ('nvn_api','shader_check','maxwell_check','graphics_check','nvn_graphics_check',
            'nvn_external_image_check','nvn_window_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','texture_check','nvn_texture_check',
            'block_copy_check','texture_conversion_check','nvn_pipeline_check','nvn_pipeline_gob_check','native_pipeline_check','window_source_check','fullsize_display_check','compute_driverparams_check','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','d2r_host','graphics_draw_batch')) or
        type(record.get('texture_descriptor_flags',0)) is not int or record.get('texture_descriptor_flags',0)!=0):
        raise RuntimeError('invalid independent checked A650 public-copy build')
    if packaged and (record.get('public_copy_check')or record.get('public_volume_copy_check')):raise RuntimeError('packaged qualifier combined with ordinary copy')
    oracle.validate_cases(check['cases'])
    if packaged and any(k not in record['source']for k in ('src/packaged_bntx.c','include/nvn_native/packaged_bntx.h','tests/public_packaged_copy_layout.c')):raise RuntimeError('Packaged experiment source inventory missing')
    if volume and any(k not in record['source']for k in ('src/volume_copy.inc','src/texture_volume.inc','tests/public_volume_copy_layout.c','android/public_volume_copy_activity.c')):
        raise RuntimeError('Public volume build lacks its actual 3D source inventory')
    if any(k not in record['source'] for k in ('src/compute.c','src/texture_source.c','src/block_copy.c','src/nvn.c','src/canonical_copy_record.inc','src/canonical_texture_copy.inc','tests/public_copy_cases.h','tests/public_copy_probe.c','android/public_copy_activity.c')):
        raise RuntimeError('Public NVN copy build lacks its actual runtime/probe source inventory')

def exercise(args,build,record,adb,package,component,run, *, volume=False, shadow=False, packaged=False, volume_texture=False, buffer_texture=False):
    profile(record,volume=volume,shadow=shadow,packaged=packaged,volume_texture=volume_texture,buffer_texture=buffer_texture);oracle=_oracle(volume,shadow,packaged,volume_texture,buffer_texture);names=oracle.NAMES
    key="buffer_texture"if buffer_texture else"volume_texture"if volume_texture else"public_packaged_copy"if packaged else"volume_shadow"if shadow else"public_volume_copy"if volume else"public_copy"
    prefix="buffer-texture"if buffer_texture else"volume-texture"if volume_texture else"volume-shadow"if shadow else"public-volume-copy"if volume else"public-copy"
    for name in names:
        p=args.evidence.parent/(args.evidence.stem+'-'+name)
        if p.exists() or p.is_symlink():raise RuntimeError('Public NVN copy readback evidence exists; choose fresh output')
    args.evidence.parent.mkdir(parents=True,exist_ok=True)
    if args.evidence.is_symlink():raise RuntimeError('Public NVN copy evidence must not be a symlink')
    evidence={'scope':('ordinary Android authored raw-native BUFFER FLOAT TXF/40format TXS; no public NVN/game claim'if buffer_texture else'ordinary Android authored true3D native TXF/TXS + owner materialization; no public NVN/game/compositor claim'if volume_texture else'ordinary Android experimental BNTX-writer 8B public Copy interop; no normative NVN encoding/game/frame claim'if packaged else'ordinary Android authored true3D owner materialization; no sampling/public NVN/game/compositor claim'if shadow else'ordinary Android authored public NVN canonical Copy/CopyCommands; no game/compositor claim'),
              'build':record,key+'_verified':False,'stopped':False,'call_commands':record.get('public_copy_call',False)}
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
        nonce=uuid.uuid4().hex;directory=prefix+'-'+nonce;evidence['run_nonce']=nonce
        pending=prefix+'-run-nonce.'+nonce+'.pending'
        script=(f'set -eu; umask 077; test ! -e files/{directory}; test ! -L files/{directory}; '
                f'rm -f files/result.json; set -C; printf %s {nonce} > files/{pending}; '
                f'mv -f files/{pending} files/{prefix}-run-nonce')
        run('shell',shlex.join(['run-as',package,'sh','-c',script]))
        evidence['launch']=run('shell','am','start','-W','-n',component).stdout
        # One bounded correctness batch, no frame/performance measurements.
        deadline=time.monotonic()+180
        while True:
            result=run('shell','run-as',package,'cat','files/result.json',check=False)
            if result.returncode==0:
                data=parse_result(result.stdout);break
            if time.monotonic()>=deadline:raise RuntimeError('Public NVN copy probe produced no result within bounded qualification deadline')
            time.sleep(.1)
        evidence['result']=data
        evidence['fingerprint']=run('shell','getprop','ro.build.fingerprint').stdout.strip()
        evidence['kernel']=run('shell','uname','-r').stdout.strip()
        captured=collect_copy_readbacks(run,adb,package,directory,names,args.evidence,evidence)
        save()  # Preserve actual raw output even when counts/overlap fail.
        if data.get('passed')is not True:
            raise RuntimeError('Public NVN copy device failed: stage='+str(data.get('stage'))+' errno='+str(data.get('errno'))+' cleanup_errno='+str(data.get('cleanup_errno'))+' resources_retained='+str(data.get('resources_retained')))
        validate_copy_inventory(evidence,directory,names)
        call_kwargs={'call_commands':record.get('public_copy_call',False)}if not(buffer_texture or volume_texture or shadow or packaged)else{}
        evidence[key+'_verification']=oracle.validate(data,captured,nonce,uid,record[key+'_check'],**call_kwargs)
        evidence[key+'_verified']=True
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
        evidence[key+'_verified']=False;evidence['error']='Public NVN copy owner process stop was not confirmed';save()
        raise RuntimeError(evidence['error'])
    print(json.dumps(evidence['result'],indent=2))
