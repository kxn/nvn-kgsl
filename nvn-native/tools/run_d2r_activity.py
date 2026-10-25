#!/usr/bin/env python3
"""Ignored runner candidate for the genuine Activity/ABI2 shared original host.

No guessed waitpid status: Activity process exit_code remains unknown. Runtime
execution is explicit; --validate-only verifies local provenance without ADB.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
import uuid
import zipfile

PACKAGE='org.nvn.nativeprobe'
ACTIVITY=PACKAGE+'/org.nvn.nativeprobe.PresentActivity'
NAMES={'1':'sdk-init','2':'constructors','3':'run','sdk-init':'sdk-init','constructors':'constructors','run':'run'}
STAGES={'sdk-init':1,'constructors':2,'run':3}


def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def checked(mapping):
    if not isinstance(mapping,dict) or not mapping:raise ValueError('missing provenance inventory')
    for p,h in mapping.items():
        if not Path(p).is_file() or sha(p)!=h:raise ValueError('integrity mismatch: '+p)


def classify_activity(stage,status,alive,timed_out,stdout,stderr,base_classify):
    # Do not substitute a synthetic process return code, even after a stage
    # returned. We only observed the shared original entry's returned value.
    answer=base_classify(stage,None,timed_out,stdout,stderr)
    marker={'sdk-init':'sdk-initialized','constructors':'constructors-completed','run':'nnMain-returned'}[stage]
    completed=(not timed_out and isinstance(status,dict) and status.get('stage_returned') is True and
               type(status.get('return_code')) is int and status['return_code']==0 and status.get('errno')==0 and
               re.search(r'"stage"\s*:\s*"'+marker+'"',stdout+'\n'+stderr) is not None)
    passed=completed and not answer['first_named_error'] and not answer['guest_error_lines'] and not answer['guest_initialization_error'] and not answer['resource_failure']
    terminal=bool(re.search(r'^.*\[native NVN D2R\].*terminating execution \(exit 78\).*$',stderr,re.M))
    boundary=not timed_out and alive is False and terminal
    answer.update(stage_completed=bool(completed),passed=bool(passed),unsupported_boundary=boundary,
                  process_exit_code=None,process_exit_code_observation='No Activity waitpid status; unknown',
                  original_stage_return_code=status.get('return_code') if isinstance(status,dict) and status.get('stage_returned') is True else None,
                  first_frame=False,original_host_entry_observed=bool(re.search(r'\[native D2R Activity\] original ARM64 owner entry',stderr)),
                  original_module_entry_observed=bool(re.search(r'native >>> (?:_init_libc0|nnMain)(?:\s|$)',stderr)))
    if boundary:answer['outcome']='unsupported-nvn-boundary'
    elif timed_out:answer['outcome']='timeout'
    elif answer['resource_failure']:answer['outcome']='guest-resource-failure'
    elif answer['guest_initialization_error']:answer['outcome']='guest-initialization-failure'
    elif answer['guest_error_lines']:answer['outcome']='guest-error'
    else:answer['outcome']='stage-completed' if passed else 'controller-or-process-failure'
    return answer


def read_app_file(command,adb,relative):
    """shell-v2 carries remote exit status and keeps stderr out of file bytes.

    Only cat's precise ENOENT is a not-ready file. Permission/run-as/transport
    errors must abort; neither those messages nor partial reads are artifacts.
    """
    argv=adb+['shell','-T',shlex.join(['run-as',PACKAGE,'cat',relative])]
    result=command(argv,allow_failure=True)
    missing=('cat: '+relative+': No such file or directory').encode()
    if result.returncode==1 and not result.stdout and result.stderr.strip()==missing:return None
    if result.returncode or result.stderr or result.stdout.startswith(('cat: '+relative+': ').encode()):
        raise ValueError('Activity file read failed: '+relative+' rc='+str(result.returncode)+' '+
                         (result.stderr or result.stdout).decode(errors='replace'))
    return result.stdout


def parse_pidof_result(result):
    """Only pidof's explicit no-match result proves absence of this package."""
    if result.returncode==1 and not result.stdout and not result.stderr:return []
    if result.returncode==0 and not result.stderr:
        words=result.stdout.split()
        if words and all(re.fullmatch(rb'[1-9][0-9]*',word) for word in words):
            values=[int(word) for word in words]
            if len(values)==len(set(values)):return values
    raise ValueError('cannot prove package process absence: pidof rc='+str(result.returncode)+' '+
                     (result.stderr or result.stdout).decode(errors='replace'))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--serial',required=True);ap.add_argument('--build',type=Path,required=True)
    ap.add_argument('--evidence',type=Path,required=True);ap.add_argument('--stage',choices=NAMES,required=True)
    ap.add_argument('--install',action='store_true');ap.add_argument('--timeout',type=float,default=20)
    ap.add_argument('--romfs',type=Path);ap.add_argument('--reuse-romfs',action='store_true')
    ap.add_argument('--validate-only',action='store_true')
    args=ap.parse_args();stage=NAMES[args.stage];candidate=Path(__file__).resolve().parent
    root=Path(__file__).resolve().parents[2]
    if args.timeout<=0 or args.timeout>600:ap.error('timeout must be (0,600] seconds')
    if (stage=='run' and not (args.romfs and args.reuse_romfs)) or (args.reuse_romfs and not args.romfs):
        ap.error('run requires --romfs ORIGINAL_LOCAL_DIR --reuse-romfs; no resources are fabricated')
    evidence_path=args.evidence.resolve();evidence_path.parent.mkdir(parents=True,exist_ok=True)
    fd=os.open(evidence_path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600);os.close(fd)
    nonce=uuid.uuid4().hex
    ev={'schema':1,'operation':'d2r_activity_execution','run_nonce':nonce,'stage':stage,'stage_value':STAGES[stage],
        'process_exit_code':None,'first_frame':False,'adb_invoked':False,'activity_launched':False,'commands':[],
        'artifacts':{},'passed':False,'validate_only':args.validate_only}
    controlled=False;device_tmp=None;adb=[]
    def save():
        tmp=evidence_path.with_name(evidence_path.name+'.pending');tmp.write_text(json.dumps(ev,indent=2)+'\n');tmp.replace(evidence_path)
    def artifact(suffix,raw):
        path=evidence_path.with_name(evidence_path.stem+'-'+suffix)
        with path.open('xb') as f:f.write(raw)
        ev['artifacts'][str(path)]={'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)};return path
    def command(argv,timeout=30,allow_failure=False):
        r=subprocess.run(list(map(str,argv)),capture_output=True,timeout=timeout)
        ev['commands'].append({'argv':list(map(str,argv)),'returncode':r.returncode,'stdout':r.stdout.decode(errors='replace'),'stderr':r.stderr.decode(errors='replace')})
        if r.returncode and not allow_failure:raise ValueError('command failed: '+shlex.join(list(map(str,argv)))+' '+r.stderr.decode(errors='replace'))
        return r
    def remote(argv,**kw):
        ev['adb_invoked']=True
        return command(adb+['shell',shlex.join(list(map(str,argv)))],**kw)
    def app(argv,**kw):return remote(['run-as',PACKAGE,*argv],**kw)
    def device_sha(path,ordinary=False,timeout=180):
        r=(app if ordinary else remote)(['sha256sum',path],timeout=timeout)
        value=r.stdout.decode().split()[0]
        if not re.fullmatch('[0-9a-f]{64}',value):raise ValueError('device SHA output invalid')
        return value
    try:
        record_path=args.build.resolve()/'build-record.json';record=json.loads(record_path.read_text());o=record['options']
        if record.get('operation')!='d2r_activity_apk' or o.get('host_abi')!=2 or o.get('package')!=PACKAGE or o.get('controller_lib')!='nvn_d2r_activity_controller':
            raise ValueError('requires the genuine ABI2 Activity APK build')
        for key in ('sources','tools','compiled_inputs','artifacts'):checked(record[key])
        host=record['d2r_activity_host']
        for key in ('sources','tools','artifacts'):checked(host[key])
        verify_argv=[host['options']['python'],str(candidate/'build_d2r_activity_apk.py'),'--host-record',o['host_record'],
                     '--sdk',o['sdk'],'--ndk',o['ndk'],'--keystore',o['keystore'],'--out',str(record_path.parent),'--verify']
        command(verify_argv,timeout=300)
        ev.update(build_record=str(record_path),build_record_sha256=sha(record_path),apk_build=record,
                  host_verify_argv=record['host_verify_argv'],apk_verify_argv=verify_argv,
                  runner_sources={str(p):sha(p) for p in [Path(__file__),root/'nvn-native/tools/run_d2r_host.py',candidate/'build_d2r_activity_apk.py']},
                  original_nso_inputs=host['inputs'],modules_manifest=host['rewrite_manifest'],host_abi=2,rewrite_abi='elfdesc')
        apk=Path(record['apk']);hostlib=Path(host['shared_library']);controller=next(Path(p) for p in record['artifacts'] if Path(p).name=='libnvn_d2r_activity_controller.so')
        with zipfile.ZipFile(apk) as z:
            libs={n for n in z.namelist() if n.startswith('lib/')}
            if libs!={'lib/arm64-v8a/libdmhost_activity_kgsl.so','lib/arm64-v8a/libnvn_d2r_activity_controller.so'}:raise ValueError('APK must contain exactly controller and original host, no second core')
            for path in (hostlib,controller):
                if hashlib.sha256(z.read('lib/arm64-v8a/'+path.name)).hexdigest()!=sha(path):raise ValueError('APK member hash differs')
        modules=Path(host['modules']);module_files=sorted(modules.iterdir())
        if not module_files or any(not p.is_file() or p.is_symlink() or str(p) not in host['artifacts'] for p in module_files):raise ValueError('unrecorded module file')
        romfs={}
        if args.romfs:
            original=args.romfs.resolve()
            if not original.is_dir() or args.romfs.is_symlink():raise ValueError('original RomFS directory required')
            for p in sorted(original.rglob('*')):
                relative=str(p.relative_to(original))
                if p.is_symlink() or any(ord(c)<32 or ord(c)==127 for c in relative):raise ValueError('RomFS symlink/control path refused')
                if p.is_file():romfs[relative]={'sha256':sha(p),'bytes':p.stat().st_size}
                elif not p.is_dir():raise ValueError('nonregular RomFS entry refused')
            if not romfs:raise ValueError('full original RomFS is empty')
        ev['romfs']={'local':str(args.romfs.resolve()) if args.romfs else None,'reuse':args.reuse_romfs,'files':romfs,'verified_files':0,'verified_bytes':0}
        if args.validate_only:ev['outcome']='local-provenance-verified-no-adb';ev['local_verification_passed']=True;save();return 0
        adb_path=Path(o['sdk'])/'platform-tools/adb';adb=[str(adb_path),'-s',args.serial];ev['adb_tool']={'path':str(adb_path),'sha256':sha(adb_path)}
        ev['serial']=args.serial
        ev['adb_invoked']=True
        features=command(adb+['features']).stdout.decode().replace(',',' ').split()
        if 'shell_v2' not in features:raise ValueError('ADB shell_v2 is required for reliable remote exit status')
        ev['adb_shell_protocol']='shell_v2 (shell -T; separate stdout/stderr and remote exit status)'
        if args.install:
            remote(['am','force-stop',PACKAGE]);controlled=True;command(adb+['install','-r','-t',str(apk)],timeout=180)
        paths=remote(['cmd','package','path',PACKAGE]).stdout.decode().splitlines()
        if len(paths)!=1 or not paths[0].startswith('package:'):raise ValueError('single installed base APK required')
        installed=paths[0][8:]
        if device_sha(installed)!=sha(apk):raise ValueError('installed APK hash differs')
        parent=str(Path(installed).parent);expected_lib=parent+'/lib/arm64'
        dump=remote(['dumpsys','package',PACKAGE]).stdout.decode()
        if not (re.search(r'nativeLibraryDir='+re.escape(expected_lib)+r'(?:\s|$)',dump) or
                (re.search(r'legacyNativeLibraryDir='+re.escape(parent+'/lib')+r'(?:\s|$)',dump) and 'primaryCpuAbi=arm64-v8a' in dump)):
            raise ValueError('installed native library path/ABI contract differs')
        remote(['am','force-stop',PACKAGE]);controlled=True
        uid=int(app(['id','-u']).stdout.decode().strip())
        package_uid=remote(['cmd','package','list','packages','-U',PACKAGE]).stdout.decode()
        if uid<10000 or not re.search(r'package:'+re.escape(PACKAGE)+r'\s+uid:'+str(uid)+r'(?:\s|$)',package_uid):raise ValueError('package UID differs')
        installed_libraries={}
        for lib in (hostlib,controller):
            resolved=app(['readlink','-f',expected_lib+'/'+lib.name]).stdout.decode().strip()
            if not resolved.startswith('/data/app/') or Path(resolved).name!=lib.name or device_sha(resolved,True)!=sha(lib):raise ValueError('installed library hash/path differs')
            installed_libraries[lib.name]={'path':resolved,'sha256':sha(lib)}
        home=app(['pwd']).stdout.decode().strip()
        if not home.startswith('/data/') or not home.endswith('/'+PACKAGE):raise ValueError('unexpected package home')
        files=home+'/files';module_dest=files+'/d2r-activity/modules';romfs_dest=files+'/d2r-host/game/romfs'
        ev.update(uid=uid,installed_apk={'path':installed,'sha256':sha(apk)},installed_libraries=installed_libraries,
                  package_uid=package_uid,execution_context='NativeActivity app process; run-as is deployment/observation only',
                  host_environment={k:files+'/d2r-host/game/'+v for k,v in [('DM_ROOT','root'),('DM_ROMFS','romfs'),('DM_SAVE','save'),('DM_TEMP','temp')]} )
        ev['host_environment'].update(DM_IMPORTS='1',DM_BOOTPROF='1',DM_FS='1')
        app(['mkdir','-p',files+'/d2r-activity/runs',module_dest,*[files+'/d2r-host/game/'+x for x in ('root','romfs','save','temp')]])
        for directory in (files,files+'/d2r-activity',files+'/d2r-activity/runs',module_dest,files+'/d2r-host',files+'/d2r-host/game',*[files+'/d2r-host/game/'+x for x in ('root','romfs','save','temp')]):app(['test','!','-L',directory])
        if args.reuse_romfs:
            def inventory():
                unusual=app(['find',romfs_dest,'!','-type','f','!','-type','d','-print0']).stdout
                if unusual:raise ValueError('reused RomFS has symlink/special entry')
                raw=app(['find',romfs_dest,'-type','f','-print0']).stdout
                if raw and not raw.endswith(b'\0'):raise ValueError('invalid remote inventory')
                names=raw[:-1].split(b'\0') if raw else []
                if len(names)!=len(set(names)) or set(names)!={(romfs_dest+'/'+n).encode() for n in romfs}:raise ValueError('complete RomFS inventory differs')
            inventory()
            for name,meta in romfs.items():
                if sha(args.romfs.resolve()/name)!=meta['sha256'] or device_sha(romfs_dest+'/'+name,True)!=meta['sha256']:raise ValueError('original/device RomFS hash differs: '+name)
                ev['romfs']['verified_files']+=1;ev['romfs']['verified_bytes']+=meta['bytes'];save()
            inventory()
        device_tmp='/data/local/tmp/d2r-activity-'+nonce;remote(['mkdir',device_tmp])
        for i,p in enumerate(module_files):
            remote_file=device_tmp+'/'+str(i);command(adb+['push',str(p),remote_file],timeout=180)
            app(['test','!','-L',module_dest+'/'+p.name]);app(['cp',remote_file,module_dest+'/'+p.name])
            if device_sha(module_dest+'/'+p.name,True)!=host['artifacts'][str(p)]:raise ValueError('deployed ABI2 module hash differs')
        if app(['find',module_dest,'!','-type','f','!','-type','d','-print0']).stdout:raise ValueError('module tree contains symlink/special entry')
        remote_modules=app(['find',module_dest,'-type','f','-print0']).stdout
        if set(remote_modules[:-1].split(b'\0'))!={(module_dest+'/'+p.name).encode() for p in module_files}:
            raise ValueError('device module inventory differs from full ABI2 manifest')
        ev['deployed_modules']={p.name:host['artifacts'][str(p)] for p in module_files}
        config=nonce+' '+str(STAGES[stage])+'\n';config_tmp=device_tmp+'/run.cfg'
        local_config=evidence_path.with_name(evidence_path.stem+'-run.cfg');local_config.write_text(config);ev['artifacts'][str(local_config)]={'sha256':sha(local_config),'bytes':35}
        command(adb+['push',str(local_config),config_tmp]);app(['test','!','-L',files+'/d2r-activity/run.cfg']);app(['cp',config_tmp,files+'/d2r-activity/run.cfg'])
        if device_sha(files+'/d2r-activity/run.cfg',True)!=sha(local_config):raise ValueError('configuration hash differs')
        for key in ('sources','tools','compiled_inputs','artifacts'):checked(record[key])
        for key in ('sources','tools','artifacts'):checked(host[key])
        if device_sha(installed)!=sha(apk) or remote(['cmd','package','path',PACKAGE]).stdout.decode().splitlines()!=paths:
            raise ValueError('installed APK/path changed before launch')
        for lib in installed_libraries.values():
            if device_sha(lib['path'],True)!=lib['sha256']:raise ValueError('installed shared library changed before launch')
        remote(['input','keyevent','KEYCODE_WAKEUP'])
        remote(['wm','dismiss-keyguard'])
        relative='files/d2r-activity/runs/'+nonce+'/';statuses={};identity=None;alive=None;timed_out=False
        def read(name):return read_app_file(command,adb,relative+name)
        def valid_status(raw):
            data=json.loads(raw)
            if (type(data) is not dict or type(data.get('schema')) is not int or data['schema']!=1 or
                    data.get('operation')!='d2r_activity_host' or type(data.get('stage')) is not int or
                    type(data.get('host_abi')) is not int or type(data.get('errno')) is not int or
                    type(data.get('return_code')) is not int or type(data.get('stage_returned')) is not bool or
                    data.get('first_frame') is not False or data.get('controller_links_core') is not False):
                raise ValueError('invalid Activity status schema/types')
            if data.get('run_nonce')!=nonce or data.get('stage')!=STAGES[stage] or data.get('host_abi')!=2 or data.get('uid')!=uid or 'untrusted_app' not in data.get('selinux_context',''):raise ValueError('fresh Activity identity/stage mismatch')
            if any(type(data.get(k)) is not int or data[k]<=0 for k in ('pid','uid','process_start_ticks')):raise ValueError('invalid process identity metadata')
            return data
        def is_alive(start):
            r=app(['cat','/proc/'+str(start['pid'])+'/stat'],allow_failure=True)
            if r.returncode:
                if b'No such file or directory' in r.stderr:return False
                raise ValueError('cannot prove Activity process identity/death: '+r.stderr.decode(errors='replace'))
            raw=r.stdout.decode();tail=raw[raw.rfind(')')+2:].split()
            if len(tail)<20:raise ValueError('invalid process stat')
            same=int(tail[19])==start['process_start_ticks']
            if not same:ev['original_pid_reused']=True
            return same
        ev['activity_launch_requested']=True
        command(adb+['shell','am','start','-W','-n',ACTIVITY],timeout=30);ev['activity_launched']=True
        deadline=time.monotonic()+args.timeout
        while True:
            for name in ('start.json','attached.json','result.json'):
                if name not in statuses:
                    raw=read(name)
                    if raw is not None:statuses[name]=valid_status(raw);artifact(name,raw)
            if 'start.json' in statuses:
                identity=statuses['start.json']
                for observed in statuses.values():
                    if any(observed[k]!=identity[k] for k in ('pid','uid','process_start_ticks','selinux_context')):
                        raise ValueError('status records refer to different process identities')
                alive=is_alive(identity)
                if alive and 'process_observation' not in ev:
                    proc_uid=app(['cat','/proc/'+str(identity['pid'])+'/status']).stdout.decode()
                    if not re.search(r'^Uid:\s+'+str(uid)+r'\s',proc_uid,re.M):raise ValueError('Activity process UID differs')
                    cmdline=app(['cat','/proc/'+str(identity['pid'])+'/cmdline']).stdout
                    if not cmdline.startswith(PACKAGE.encode()+b'\0'):raise ValueError('Activity command line differs')
                    context=app(['cat','/proc/'+str(identity['pid'])+'/attr/current'],allow_failure=True)
                    if not context.returncode and context.stdout.decode().strip()!=identity['selinux_context']:raise ValueError('Activity SELinux context differs')
                    ev['process_observation']={'pid':identity['pid'],'start_ticks':identity['process_start_ticks'],'uid':uid,
                        'selinux_context_from_verified_controller':identity['selinux_context'],'proc_status':proc_uid,
                        'identity_checked_while_alive':True,'cmdline':cmdline.decode(errors='replace'),
                        'kernel_context_observation':context.stdout.decode(errors='replace') if not context.returncode else None}
                if alive is False or 'result.json' in statuses:break
            if time.monotonic()>=deadline:timed_out=True;break
            time.sleep(.1)
        logs={}
        for name in ('stdout.log','stderr.log'):
            raw=read(name)
            if raw is not None:artifact('observed-'+name,raw)
            logs[name]=(raw or b'').decode(errors='replace')
        if identity is not None:alive=is_alive(identity)
        sys.path.insert(0,str(root/'nvn-native/tools'));from run_d2r_host import classify
        outcome=classify_activity(stage,statuses.get('result.json'),alive,timed_out,logs['stdout.log'],logs['stderr.log'],classify)
        ev.update(statuses=statuses,original_process_alive=alive,timed_out=timed_out,result=outcome,passed=outcome['passed'])
        save();return 0 if outcome['passed'] else 1
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError) as error:
        ev['error']=str(error);ev['outcome']='verification-or-execution-failure';save();return 1
    finally:
        if controlled and adb:
            try:
                remote(['am','force-stop',PACKAGE])
                remaining=b''
                for _ in range(10):
                    remaining=parse_pidof_result(remote(['pidof',PACKAGE],allow_failure=True))
                    if not remaining:break
                    time.sleep(.1)
                if remaining:raise ValueError('Activity package process remained after force-stop')
                ev['finally_force_stopped']=True
            except Exception as error:ev['finally_stop_error']=str(error);ev['passed']=False
        # Capture final immutable logs after stop too, including an Activity
        # that exited78 before am-start/polling could return. Never infer an
        # exit code or natural boundary from our own force-stop.
        if adb and ev.get('activity_launch_requested'):
            try:
                final_logs={}
                for name in ('start.json','attached.json','result.json','stdout.log','stderr.log'):
                    raw=read_app_file(command,adb,'files/d2r-activity/runs/'+nonce+'/'+name)
                    if raw is not None:
                        if name.endswith('.json'):
                            data=valid_status(raw)
                            ev.setdefault('post_stop_statuses',{})[name]=data
                        artifact('post-stop-'+name,raw)
                        if name.endswith('.log'):final_logs[name]=raw.decode(errors='replace')
                if ev.get('result') and len(final_logs)==2:
                    ev['result']=classify_activity(stage,ev.get('statuses',{}).get('result.json'),
                        ev.get('original_process_alive'),ev.get('timed_out',False),
                        final_logs['stdout.log'],final_logs['stderr.log'],classify)
                    ev['passed']=ev['result']['passed'] and not ev.get('finally_stop_error')
            except Exception as error:ev['final_log_collection_error']=str(error);ev['passed']=False
        if device_tmp and adb:
            try:remote(['rm','-rf',device_tmp])
            except Exception as error:ev['temporary_cleanup_error']=str(error);ev['passed']=False
        save()
        if ev.get('finally_stop_error') or ev.get('temporary_cleanup_error') or (ev.get('activity_launch_requested') and not ev['passed']):return 1


if __name__=='__main__':sys.exit(main())
