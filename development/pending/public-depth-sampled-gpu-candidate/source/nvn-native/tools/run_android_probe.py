#!/usr/bin/env python3
"""Verify a build record, run the installed standalone APK, and save evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
import shlex
import uuid

MODE_NAMES = tuple(f'mode-o{o}-d{d}-z{z}.rgba'
                   for o in range(2) for d in range(2) for z in ('neg', 'pos', 'far'))


DRAW_BATCH_NAMES = tuple(f'draw-mode-{m}-replay-{r}.rgba' for m in range(4) for r in range(2))

def validate_draw_batch(record, data, images):
    check=record.get('nvn_graphics_check')
    if (record.get('graphics_draw_batch') is not True or not isinstance(check,dict) or
        check.get('modes') is not True or check.get('image',{}).get('schema')!=3 or
        check.get('program',{}).get('chip_id')!=0x06050002):
        raise RuntimeError('invalid draw-batch build profile')
    required={'completed_cases':8,'submissions':8,'mode_count':4,'handles_recorded':4,
              'handles_released':4,'handle_replays':4,'gpu_count_writes':4,'count_zero_checks':8,
              'count_readback_checks':4,'indirect_barriers_recorded':2,'indirect_argument_update_cases':1,'degenerate_index_cases':2,'nondegenerate_index_cases':2,'channel_checks':131072,'target_bytes':16384,
              'width':64,'height':64,'pitch':256,'cleanup_errno':0,'errno':0}
    if (data.get('operation')!='nvn_graphics_draw_batch' or data.get('submission')!='accepted' or
        data.get('reused_ib') is not True or data.get('resources_retained') is not False or
        data.get('passed') is not True or data.get('device_matches_artifact') is not True or
        data.get('chip_id')!=0x06050002 or data.get('gmem_bytes')!=check.get('image',{}).get('gmem_bytes') or
        any(type(data.get(k)) is not int or data[k]!=v for k,v in required.items()) or
        type(data.get('guard_byte_checks')) is not int or data['guard_byte_checks']<=0 or
        type(data.get('readonly_byte_checks')) is not int or data['readonly_byte_checks']<=0 or
        set(images)!=set(DRAW_BATCH_NAMES)):
        raise RuntimeError('result does not prove the complete draw batch')
    for mode in range(4):
        for repeat in range(2):
            payload=images[f'draw-mode-{mode}-replay-{repeat}.rgba']
            if len(payload)!=16384:raise RuntimeError('draw batch readback size mismatch')
            component=.125 if repeat else .25
            color=int(component*(1 if mode in (1,2) and not repeat else 2)*255+.5)
            for y in range(64):
                for x in range(64):
                    expected=[0,0,0,0]
                    if not (repeat and mode in (0,2)) and (not repeat or x>=32):
                        if mode<3:expected[mode]=color
                        else:expected[:3]=[color]*3
                        expected[3]=color
                    if any(abs(payload[y*256+x*4+c]-expected[c])>(1 if expected[c] else 0) for c in range(4)):
                        raise RuntimeError(f'draw mode{mode} replay{repeat} pixel({x},{y}) mismatch')


def read_draw_batch_image(run, adb, package, name):
    # exec-out may return0 plus remote error text on a missing file. Confirm
    # shell-v2 stat status/type/size, and independently require exact payload.
    if name not in DRAW_BATCH_NAMES:
        raise ValueError('unknown draw readback name')
    remote='files/'+name
    status=run('shell','run-as',package,'stat','-c','%F:%s',remote,check=False)
    if status.returncode or status.stdout.strip()!='regular file:16384':
        return None, 'missing-or-not-regular-16384'
    result=subprocess.run(adb+['exec-out','run-as',package,'cat',remote],capture_output=True,timeout=20)
    if result.returncode or len(result.stdout)!=16384:
        return None, 'read-status-or-payload-size'
    return result.stdout, None


def validate_compute_indirect(record,data):
    check=record.get('maxwell_check');manifest=record.get('compute_indirect_validation')
    if (record.get('compute_indirect') is not True or not isinstance(check,dict) or
        check.get('chip_id')!=0x06050002 or len(check.get('cases',[]))!=4 or
        not isinstance(manifest,dict) or manifest.get('chip_id')!=0x06050002 or
        len(manifest.get('entries',[]))!=4):
        raise RuntimeError('invalid original compute indirect build profile')
    required={'completed_cases':8,'submissions':8,'handles_recorded':4,'handles_released':4,
              'handle_replays':4,'gpu_group_readbacks':8,'cpu_zero_group_checks':16,
              'gpu_uniform_readbacks':8,'program_busy_rejections':8,'barriers_recorded':4,
              'data_byte_checks':131072,'command_byte_checks':131072,'output_byte_checks':3104,
              'readonly_byte_checks':126784,'errno':0,'cleanup_errno':0}
    if (data.get('operation')!='nvn_maxwell_compute_indirect' or data.get('passed') is not True or
        data.get('submission')!='accepted' or data.get('resources_retained') is not False or
        data.get('driverparams_consumed') is not False or
        any(type(data.get(k)) is not int or data[k]!=v for k,v in required.items()) or
        type(data.get('guard_byte_checks')) is not int or data['guard_byte_checks']<=0):
        raise RuntimeError('result does not prove original indirect scheduling and readbacks')
    expected=[{'program':p,'replay':r,'groups':[3 if r else 1,1,1],
               'limit':65,'output_elements':65 if r else 32,'passed':True}
              for p in range(4) for r in range(2)]
    cases=data.get('cases')
    typed=(type(cases) is list and len(cases)==8 and all(type(c) is dict and
           all(type(c.get(k)) is int for k in ('program','replay','limit','output_elements')) and
           type(c.get('groups')) is list and len(c['groups'])==3 and all(type(v) is int for v in c['groups']) and
           c.get('passed') is True for c in cases))
    if not typed or cases!=expected:
        raise RuntimeError('missing or wrong original indirect case matrix')


def validate_descriptor_audit(record, data):
    flags = record.get('texture_descriptor_flags', 0)
    if type(flags) is not int or flags not in (0, 0x12, 0x14):
        raise RuntimeError('unknown descriptor qualifier build profile')
    if not flags:
        if data.get('descriptor_software_tables', False) is not False:
            raise RuntimeError('unexpected descriptor qualifier in normal build')
        return
    if not record.get('nvn_texture_check') or record.get('texture_uncached_pools'):
        raise RuntimeError('descriptor qualifier requires cached GPU data')
    draws = 17 if record.get('texture_modes') else 5
    required = {'descriptor_metadata_flags': flags, 'descriptor_metadata_map_checks': 4,
                'descriptor_metadata_ranges': 4, 'descriptor_audit_attempts': draws,
                'descriptor_audit_delegated': draws, 'descriptor_audit_accepted': draws,
                'descriptor_audit_rejections': 0, 'descriptor_audit_errno': 0,
                'descriptor_excluded_object_checks': 8 * draws,
                'descriptor_private_jobs': draws, 'descriptor_private_base_checks': 4 * draws,
                'descriptor_metadata_invalidate_skips': 4 * draws,
                'public_cache_calls': 1 + 14 * draws + (6 if record.get('texture_noaccess_pool') else 0)}
    if (data.get('descriptor_software_tables') is not True or
            any(type(data.get(k)) is not int or data[k] != v for k, v in required.items())):
        raise RuntimeError('CPU-only descriptor tables/private native job submission were not qualified')


def validate_driverparams(record,data):
    c=record.get('compute_driverparams_check')
    if not isinstance(c,dict) or c.get('kind')!='own-numworkgroups-driverparams-v1' or c.get('chip_id')!=0x06050002 or c.get('gpu_execution') is not False or c.get('original_game_shader') is not False:
        raise RuntimeError('invalid own raw-native driverparams profile')
    required={'errno':0,'cleanup_errno':0,'chip_id':0x06050002,'completed_cases':2,'submissions':2,
              'retire_checks':2,'inflight_destroy_rejections':2,'cpu_zero_group_checks':2,
              'result_word_checks':8,'private_parameter_word_checks':8,'argument_word_checks':6,
              'all_byte_checks':49152,'readonly_byte_checks':49064}
    if (data.get('operation')!='kgsl_compute_driverparams' or data.get('submission')!='accepted' or
        any(type(data.get(k)) is not int or data[k]!=v for k,v in required.items()) or
        any(data.get(k) is not False for k in ('original_game_shader','nvn_api','runtime_compiler','resources_retained')) or
        any(data.get(k) is not True for k in ('driverparams_consumed','reused_native_ibs','passed'))):
        raise RuntimeError('own shader did not prove dynamic NumWorkgroups output/retirement')
    expected=[{'replay':r,'groups':[3 if r else 1,1,1],'observed':[3 if r else 1,1,1,324508639],'passed':True} for r in range(2)]
    cases=data.get('cases')
    if (type(cases) is not list or len(cases)!=2 or any(type(v) is not dict or
        type(v.get('replay')) is not int or v.get('passed') is not True or any(type(v.get(k)) is not list or any(type(x) is not int for x in v[k]) for k in ('groups','observed')) for v in cases) or cases!=expected):
        raise RuntimeError('own driverparams group/output case matrix is incomplete')


def copy_package_uid(text, package):
    import re
    match = re.fullmatch(r'package:' + re.escape(package) + r' uid:([0-9]+)\s*', text)
    if not match or int(match[1]) < 10000:
        raise RuntimeError('copy qualifier requires exactly one ordinary application package UID')
    return int(match[1])


def collect_copy_readbacks(run, adb, package, directory, names, evidence_path, evidence):
    base = 'files/' + directory
    shape = run('shell', 'run-as', package, 'stat', '-c', '%F', base, check=False)
    inventory = run('shell', 'run-as', package, 'find', base, '-mindepth', '1', '-print', check=False)
    regular = run('shell', 'run-as', package, 'find', base, '-mindepth', '1', '-type', 'f', '-print', check=False)
    evidence['copy_namespace_type'] = shape.stdout.strip()
    evidence['copy_file_inventory'] = inventory.stdout.splitlines()
    evidence['copy_regular_inventory'] = regular.stdout.splitlines()
    evidence['copy_inventory_returncodes'] = [shape.returncode, inventory.returncode, regular.returncode]
    images, captured = {}, {}
    # A failed probe can leave a strict subset. Preserve only known regular
    # files; never follow a symlink or descend into an unexpected directory.
    if shape.returncode == 0 and shape.stdout.strip() == 'directory':
        available = set(inventory.stdout.splitlines()) & set(regular.stdout.splitlines())
        for name in names:
            remote = base + '/' + name
            if remote not in available:
                continue
            result = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat', remote],
                                    capture_output=True, timeout=20)
            if result.returncode:
                continue
            path = evidence_path.parent / (evidence_path.stem + '-' + name)
            with path.open('xb') as stream:
                stream.write(result.stdout)
            captured[name] = result.stdout
            images[name] = {'path': str(path.resolve()), 'bytes': len(result.stdout),
                            'sha256': hashlib.sha256(result.stdout).hexdigest()}
    evidence['copy_images'] = images
    after = run('shell', 'run-as', package, 'find', base, '-mindepth', '1', '-print', check=False)
    evidence['copy_file_inventory_after'] = after.stdout.splitlines()
    evidence['copy_inventory_after_returncode'] = after.returncode
    return captured


def validate_copy_inventory(evidence, directory, names):
    expected = {'files/' + directory + '/' + name for name in names}
    fields = ('copy_file_inventory', 'copy_regular_inventory', 'copy_file_inventory_after')
    if (evidence.get('copy_namespace_type') != 'directory' or
            evidence.get('copy_inventory_returncodes') != [0, 0, 0] or
            evidence.get('copy_inventory_after_returncode') != 0 or
            any(len(evidence.get(field, [])) != len(expected) or set(evidence[field]) != expected for field in fields)):
        raise RuntimeError('copy namespace contains missing, extra, duplicate, symlink or non-regular entries')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--texture-bridge-diagnostic-mask',type=int,choices=range(256),
                        help='Only diagnostic Texture bridge qualifier: select CP stage stores; 0 is equal-size NOP padding')
    parser.add_argument('--window-source-present-interval',type=int,choices=range(6),
                        help='Only Window source qualifier: nonce-bound runtime interval 0..5; default profile unchanged')
    args = parser.parse_args()
    if args.evidence.exists():
        parser.error('evidence path already exists; choose a new path to preserve prior runs')
    build = args.build.resolve()
    record = json.loads((build / 'build-record.json').read_text())
    # Qualifier-only option is rejected before any ADB/installation/launch.
    from texture_bridge_runner import diagnostic_mask
    try:diagnostic_mask(record,args.texture_bridge_diagnostic_mask)
    except RuntimeError as error:parser.error(str(error))
    from window_interval_runtime import requested as window_interval_requested
    try:window_interval_requested(record,args.window_source_present_interval)
    except RuntimeError as error:parser.error(str(error))
    native = Path(__file__).resolve().parents[1]
    source_names = {str(p.relative_to(native)) for p in native.rglob('*')
                    if p.is_file() and p != build and build not in p.parents and '__pycache__' not in p.parts}
    if source_names != set(record['source']):
        parser.error('source inventory changed; rebuild before deployment')
    for relative, expected in record['source'].items():
        if hashlib.sha256((native / relative).read_bytes()).hexdigest() != expected:
            parser.error('source mismatch: ' + relative + '; rebuild before deployment')
    for path, expected in record['tools'].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != expected:
            parser.error('tool mismatch: ' + path + '; rebuild before deployment')
    for path, expected in record.get('compiled_inputs', {}).items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != expected:
            parser.error('compiled input mismatch: ' + path + '; rebuild before deployment')
    for path, expected in record['artifacts'].items():
        actual = hashlib.sha256(Path(path).read_bytes()).hexdigest()
        if actual != expected:
            parser.error(f'artifact mismatch: {path}; rebuild before deployment')
    copy_check = record.get('nvn_copy_check')
    if copy_check is not None:
        if (not isinstance(copy_check, dict) or copy_check.get('modes') is not True or
                copy_check.get('program', {}).get('chip_id') != 0x06050002 or
                copy_check.get('image', {}).get('schema') != 3 or type(record.get('texture_descriptor_flags', 0)) is not int or record.get('texture_descriptor_flags', 0) != 0 or
                any(record.get(key) for key in ('nvn_api', 'shader_check', 'maxwell_check', 'graphics_check',
                    'nvn_graphics_check', 'nvn_external_image_check', 'nvn_window_check', 'texture_check',
                    'nvn_texture_check', 'device_inspect', 'iocoherent_inspect', 'ahb_inspect',
                    'ahb_graphics_check', 'ahb_present_check', 'd2r_host'))):
            parser.error('invalid independent A650 copy build profile')
        from copy_validation import NAMES as COPY_NAMES
        for name in COPY_NAMES:
            destination = args.evidence.parent / (args.evidence.stem + '-' + name)
            if destination.exists() or destination.is_symlink():
                parser.error('copy evidence readback path already exists; choose a fresh evidence name')
    if record.get('window_source_check') or record.get('fullsize_display_check'):
        check=record.get('window_source_check') or record['fullsize_display_check']
        source_mode=bool(record.get('window_source_check'))
        if source_mode:
            if (check.get('kind')!='native-texture-consumer-bridge-v1' or type(record.get('window_source_slots')) is not int or record['window_source_slots'] not in (2,3) or record.get('activity')!='org.nvn.nativeprobe.PresentActivity'):
                parser.error('invalid Window source build mode')
            for path,digest in {**check['inputs'],**check['artifacts']}.items():
                if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=digest:parser.error('Window source bundle changed: '+path)
        elif (check.get('kind')!='native-fullsize-ahb-linear-copy-v1' or check.get('width')!=2880 or check.get('height')!=1620 or
                check.get('cpu_image_copy') is not False or check.get('game_modules_executed') is not False or
                type(record.get('fullsize_display_slots')) is not int or record['fullsize_display_slots'] not in (2,3) or
                record.get('activity')!='org.nvn.nativeprobe.PresentActivity'):
            parser.error('invalid fullsize display build mode')
        if args.evidence.exists() or args.evidence.is_symlink() or (args.evidence.parent.exists() and
                any(p.name.startswith(args.evidence.stem+'-') for p in args.evidence.parent.iterdir())):
            parser.error('fullsize evidence namespace already exists; choose a fresh evidence path')
        # Resolve screenshot dependencies before installing or starting the app.
        # The repository venv already provides Pillow; no device work is needed
        # to discover a missing host-side collector dependency.
        try:
            from PIL import Image
        except ImportError:
            parser.error('fullsize screenshot validation requires Pillow; use the repository .local/venv/bin/python')
    adb = ['adb', '-s', args.serial]
    package = 'org.nvn.nativeprobe'
    component = package + '/' + record.get('activity', 'android.app.NativeActivity')

    def run(*command, check=True, timeout=20):
        return subprocess.run(adb + list(command), check=check, text=True,
                              capture_output=True, timeout=timeout)

    if record.get('nvn_pipeline_runtime_views_check') is not None:
        from nvn_pipeline_runtime_views_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('public_depth_sampled_check') is not None:
        from public_depth_sampled_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('casc_gather_depth_check') is not None:
        from casc_gather_depth_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_color_target_check') is not None:
        from nvn_pipeline_color_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_pipeline_buffer_check') is not None:
        from nvn_pipeline_buffer_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_pipeline_sampled_check') is not None:
        from nvn_pipeline_sampled_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_pipeline_bc_mips_check') is not None:
        from nvn_pipeline_bc_mip_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_pipeline_bc_check') is not None:
        from nvn_pipeline_bc_runner import exercise
        return exercise(args, build, record, adb, package, component, run)
    if record.get('nvn_pipeline_gob_check') is not None:
        from nvn_pipeline_gob_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_pipeline_check') is not None:
        from nvn_pipeline_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_graphics_storage_check') is not None:
        from graphics_storage_probe_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_depth_sampled_check') is not None:
        from depth_sampled_probe_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_depth_shadow_check') is not None:
        from depth_shadow_probe_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_depth_check') is not None:
        from depth_probe_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_sampler_mip_check') is not None:
        from sampler_mip_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_gather_check') is not None:
        from gather_probe_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_color_target_check') is not None:
        from color_target_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('native_pipeline_check') is not None:
        from pipeline_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_texture_bridge_check') is not None:
        from texture_bridge_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('bc_sampled_check') is not None:
        from bc_sampled_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('texture_texels_check') is not None:
        from texture_texels_runner import exercise
        exercise(args,build,record,adb,package,component,run)
        return
    if record.get('buffer_texture_check') is not None:
        from buffer_texture_runner import exercise
        exercise(args,build,record,adb,package,component,run)
        return
    if record.get('volume_texture_check') is not None:
        from volume_texture_runner import exercise
        exercise(args,build,record,adb,package,component,run)
        return
    if record.get('volume_shadow_check') is not None:
        from volume_shadow_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('public_volume_copy_check') is not None:
        from public_volume_copy_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('public_packaged_copy_check') is not None:
        from public_packaged_copy_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('clear_color_check') is not None:
        from clear_color_runner import exercise
        exercise(args, build, record, adb, package, component, run)
        return
    if record.get('public_copy_check') is not None:
        from public_copy_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('block_copy_check') is not None:
        from block_copy_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('texture_conversion_check') is not None:
        from texture_conversion_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_sampler_check') is not None:
        from sampler_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if record.get('nvn_counter_check') is not None:
        from counter_runner import exercise
        return exercise(args,build,record,adb,package,component,run)
    if args.install:
        install = run('install', '--no-incremental', '-r', str(build / 'probe.apk'), timeout=180)
        if 'Success' not in install.stdout:
            parser.error('APK installation was not confirmed successful')
    installed = run('shell', 'pm', 'path', package).stdout.strip().splitlines()
    if len(installed) != 1 or not installed[0].startswith('package:'):
        parser.error('expected one installed standalone APK')
    installed_sha = run('shell', 'sha256sum', installed[0][8:]).stdout.split()[0]
    if installed_sha != record['artifacts'][str(build / 'probe.apk')]:
        parser.error('installed APK differs from verified build; install current artifact')
    # Ensure an old result cannot be mistaken for this execution.
    run('shell', 'am', 'force-stop', package)
    run('shell', 'run-as', package, 'rm', '-f', 'files/result.json')
    if record.get('graphics_draw_batch'):
        run('shell', 'run-as', package, 'rm', '-f', *['files/' + name for name in DRAW_BATCH_NAMES])
    elif record.get('graphics_check') or record.get('nvn_graphics_check'):
        run('shell', 'run-as', package, 'rm', '-f', *[f'files/graphics-case-{i}.rgba' for i in range(4)])
    if record.get('ahb_graphics_check') or record.get('ahb_present_check'):
        run('shell', 'run-as', package, 'rm', '-f', *[f'files/ahb-case-{i}.rgba' for i in range(4)])
    if record.get('texture_check') or record.get('nvn_texture_check'):
        run('shell', 'run-as', package, 'rm', '-f', *[f'files/texture-case-{i}.rgba' for i in range(4)])
    if record.get('nvn_texture_check'):
        run('shell', 'run-as', package, 'rm', '-f', 'files/texture-pool-b.rgba')
    if record.get('texture_modes'):
        run('shell', 'run-as', package, 'rm', '-f', *['files/' + name for name in MODE_NAMES])
    external_nonce, external_names, external_directory = None, (), None
    if record.get('nvn_external_image_check'):
        from external_image_validation import names
        external_names = names(record['external_image_slots'])
        external_nonce = uuid.uuid4().hex
        external_directory = 'external-' + external_nonce
        script = f'umask 077; printf %s {external_nonce} > files/external-run-nonce'
        run('shell', shlex.join(['run-as', package, 'sh', '-c', script]))
    copy_nonce, copy_directory, copy_names = None, None, ()
    if copy_check is not None:
        from copy_validation import NAMES
        copy_names = NAMES
        copy_nonce = uuid.uuid4().hex
        copy_directory = 'copy-' + copy_nonce
        pending_nonce = 'copy-run-nonce.' + copy_nonce + '.pending'
        script = (f'set -eu; umask 077; test ! -e files/{copy_directory}; test ! -L files/{copy_directory}; '
                  f'set -C; printf %s {copy_nonce} > files/{pending_nonce}; '
                  f'mv -f files/{pending_nonce} files/copy-run-nonce')
        run('shell', shlex.join(['run-as', package, 'sh', '-c', script]))
    fullsize_nonce=None
    if record.get('window_source_check') or record.get('fullsize_display_check'):
        fullsize_nonce=uuid.uuid4().hex
        prefix='window-source' if record.get('window_source_check') else 'fullsize'
        script=f'umask 077; printf %s {fullsize_nonce} > files/{prefix}-run-nonce'
        run('shell',shlex.join(['run-as',package,'sh','-c',script]))
        if record.get('window_source_check'):
            from window_interval_runtime import deploy_config
            interval_config=deploy_config(run,package,fullsize_nonce,args.window_source_present_interval)
    window_nonce = None
    if record.get('nvn_window_check'):
        window_nonce = uuid.uuid4().hex
        script = f'umask 077; printf %s {window_nonce} > files/window-run-nonce'
        run('shell', shlex.join(['run-as', package, 'sh', '-c', script]))
    try:
        if record.get('ahb_present_check') or window_nonce or fullsize_nonce:
            run('shell', 'input', 'keyevent', 'KEYCODE_WAKEUP')
            run('shell', 'wm', 'dismiss-keyguard')
        launched = run('shell', 'am', 'start', '-W', '-n', component)
        deadline = time.monotonic() + (20 if external_nonce or copy_nonce else 15 if record.get('texture_modes') else 5)
        while True:
            result = run('shell', 'run-as', package, 'cat', 'files/result.json', check=False)
            if result.returncode == 0:
                if copy_nonce:
                    from copy_validation import parse_result
                    data = parse_result(result.stdout)
                else:
                    data = json.loads(result.stdout)
                break
            if time.monotonic() >= deadline:
                raise RuntimeError('probe did not produce a result within the qualification deadline')
            time.sleep(0.1)
        evidence = {'scope': 'ordinary Android application; ' + data.get('operation', 'legacy_cp_mem_write'),
                    'build': record, 'result': data, 'launch': launched.stdout,
                    'installed_apk_sha256': installed_sha,
                    'fingerprint': run('shell', 'getprop', 'ro.build.fingerprint').stdout.strip(),
                    'kernel': run('shell', 'uname', '-r').stdout.strip(),
                    'package_uid': run('shell', 'cmd', 'package', 'list', 'packages', '-U', package).stdout.strip()}
        args.evidence.parent.mkdir(parents=True, exist_ok=True)
        if (data.get('operation') == ('nvn_window_source' if record.get('window_source_check') else 'kgsl_ahb_linear_copy')) != bool(fullsize_nonce):
            raise RuntimeError('fullsize operation does not match the verified build mode')
        if (data.get('operation') == 'nvn_gpu_copy') != bool(copy_nonce):
            raise RuntimeError('copy operation does not match its verified build profile')
        if copy_nonce:
            from copy_validation import validate
            evidence['run_nonce'] = copy_nonce
            captured = collect_copy_readbacks(run, adb, package, copy_directory, copy_names, args.evidence, evidence)
            args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
            validate_copy_inventory(evidence, copy_directory, copy_names)
            expected_uid = copy_package_uid(evidence['package_uid'], package)
            evidence['copy_verification'] = validate(data, captured, copy_nonce, copy_check, expected_uid)
            evidence['copy_verified'] = True
            args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
            print(json.dumps(data, indent=2))
            return
        if fullsize_nonce:
            if record.get('window_source_check'):
                from window_source_lifecycle import exercise
            else:
                from fullsize_display_lifecycle import exercise
            evidence['run_nonce']=fullsize_nonce
            if record.get('window_source_check'):
                evidence['window_source_present_interval']=args.window_source_present_interval
                evidence['window_interval_config']=interval_config
            exercise(run,adb,package,evidence,args.evidence)
            return
        if window_nonce:
            from window_lifecycle import exercise
            evidence['run_nonce'] = window_nonce
            exercise(run, adb, package, evidence, args.evidence)
            return
        if record.get('ahb_present_check'):
            from present_lifecycle import exercise
            data = exercise(run, adb, package, evidence, args.evidence)
        if any(record.get(key) for key in ('graphics_check', 'nvn_graphics_check', 'texture_check', 'nvn_texture_check', 'ahb_graphics_check', 'ahb_present_check')):
            images = {}
            # Preserve available failed-case readback too; no stale images can
            # survive the cleanup above. These files are GPU results, not inputs.
            draw_batch_bytes = {}
            for i in range(8 if record.get('graphics_draw_batch') else 4):
                if record.get('graphics_draw_batch'):
                    name = DRAW_BATCH_NAMES[i]
                elif record.get('ahb_graphics_check') or record.get('ahb_present_check'):
                    name = f'ahb-case-{i}.rgba'
                elif record.get('texture_check') or record.get('nvn_texture_check'):
                    name = f'texture-case-{i}.rgba'
                else:
                    name = f'graphics-case-{i}.rgba'
                if record.get('graphics_draw_batch'):
                    payload, rejection = read_draw_batch_image(run, adb, package, name)
                    if rejection:
                        evidence.setdefault('draw_readback_rejections', {})[name] = rejection
                        continue
                else:
                    result = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat', 'files/' + name], capture_output=True)
                    if result.returncode:
                        continue
                    payload = result.stdout
                path = args.evidence.parent / (args.evidence.stem + '-' + name)
                path.write_bytes(payload)
                if record.get('graphics_draw_batch'):
                    draw_batch_bytes[name] = payload
                images[name] = {'path': str(path.resolve()), 'sha256': hashlib.sha256(payload).hexdigest(), 'bytes': len(payload)}
            evidence['images'] = images
        if record.get('nvn_texture_check'):
            name = 'texture-pool-b.rgba'
            result = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat', 'files/' + name], capture_output=True)
            if not result.returncode:
                path = args.evidence.parent / (args.evidence.stem + '-' + name)
                path.write_bytes(result.stdout)
                evidence['diagnostic_images'] = {name: {'path': str(path.resolve()),
                    'sha256': hashlib.sha256(result.stdout).hexdigest(), 'bytes': len(result.stdout)}}
        mode_bytes = {}
        if record.get('texture_modes'):
            images = {}
            for name in MODE_NAMES:
                result = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat', 'files/' + name], capture_output=True)
                if result.returncode:
                    continue
                path = args.evidence.parent / (args.evidence.stem + '-' + name)
                path.write_bytes(result.stdout)
                mode_bytes[name] = result.stdout
                images[name] = {'path': str(path.resolve()), 'sha256': hashlib.sha256(result.stdout).hexdigest(),
                                'bytes': len(result.stdout)}
            evidence['mode_images'] = images
        external_bytes = {}
        if external_nonce:
            images = {}
            for name in external_names:
                result = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat',
                    'files/' + external_directory + '/' + name], capture_output=True, timeout=20)
                if result.returncode:
                    continue
                path = args.evidence.parent / (args.evidence.stem + '-' + name)
                path.write_bytes(result.stdout)
                external_bytes[name] = result.stdout
                images[name] = {'path': str(path.resolve()), 'sha256': hashlib.sha256(result.stdout).hexdigest(),
                                'bytes': len(result.stdout)}
            inventory = run('shell', 'run-as', package, 'find', 'files/' + external_directory,
                            '-type', 'f', check=False)
            evidence['external_images'] = images
            evidence['run_nonce'] = external_nonce
            evidence['external_file_inventory'] = inventory.stdout.splitlines()
        args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps(data, indent=2))
        if not data['passed'] or data['uid'] < 10000:
            raise SystemExit(1)
        if external_nonce:
            from external_image_validation import validate
            expected_inventory = {'files/' + external_directory + '/' + name for name in external_names}
            if set(evidence['external_file_inventory']) != expected_inventory:
                raise RuntimeError('external target namespace contains missing or unexpected files')
            evidence['external_image_verification'] = validate(data, external_bytes,
                record['external_image_slots'], external_nonce)
            args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
        if record.get('ahb_present_check'):
            evidence['readbacks_verified'] = (len(evidence.get('images', {})) == 4 and
                all(v['bytes'] == 16384 for v in evidence['images'].values()))
            args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
            if not evidence['readbacks_verified']:
                raise RuntimeError('presentation evidence is missing one of the four fresh GPU readbacks')
        if record.get('device_inspect') and (data.get('operation') != 'kgsl_device_info' or
                data.get('gpu_commands_submitted') is not False or data.get('errno')):
            raise RuntimeError('result does not prove a read-only KGSL property query')
        if record.get('iocoherent_inspect'):
            requested = 0x8c000000  # WB CPU mode plus KGSL_MEMFLAGS_IOCOHERENT.
            required = {'operation': 'kgsl_iocoherent_import_metadata', 'scope': 'stage1_metadata_only',
                        'chip_id': 0x06050002, 'bytes': 8192, 'seed_word_checks': 2048,
                        'gpu_submissions': 0, 'cache_maintenance_calls': 0, 'cleanup_errno': 0,
                        'requested_flags': requested}
            if (any(data.get(key) != value for key, value in required.items()) or
                    any(data.get(key) is not True for key in
                        ('metadata_gate_passed', 'import_attempted', 'input_pointer_preserved')) or
                    any(data.get(key) is not False for key in
                        ('physical_identity_measured', 'shader_coherency_qualified',
                         'nvn_uncached_qualified', 'resources_retained')) or
                    any(type(data.get(key)) is not int or data[key] & 0x8c000000 != requested
                        for key in ('import_returned_flags', 'info_returned_flags')) or
                    not data.get('object_id') or not data.get('gpu_address') or
                    data['gpu_address'] % 4096 or not data.get('submitted_useraddr')):
                raise RuntimeError('result does not prove the metadata-only IOCOHERENT import gate')
        if record.get('ahb_inspect'):
            if (data.get('operation') != 'ahb_properties' or data.get('gpu_commands_submitted') != 0 or
                    data.get('gpu_execution') is not False or data.get('display_presented') is not False or
                    data.get('cpu_sentinel_checks') != 16 or not data.get('buffer_released') or
                    data.get('fd_count_queried') != len(data.get('fds', [])) or not data.get('fds') or
                    data.get('mapping_consistent') is not True or
                    any(data.get(key, {}).get('matching_fd_count') != 1 or not data[key].get('shared')
                        for key in ('write_mapping', 'read_mapping'))):
                raise RuntimeError('result does not prove AHB allocation/mapping without GPU execution or display')
        if record.get('ahb_graphics_check'):
            required = {'completed_cases': 4, 'channel_checks': 65536,
                        'cp_submissions': 1, 'cp_payload_byte_checks': 16384,
                        'cp_corner_writes': 4, 'draw_submissions': 4,
                        'cpu_lock_count': 10, 'cpu_unlock_count': 10,
                        'cache_rejections': 2, 'inflight_destroy_rejections': 5,
                        'width': 64, 'height': 64, 'pitch': 256, 'target_bytes': 16384}
            if (data.get('operation') != 'kgsl_ahb_graphics' or
                    any(data.get(key) != expected for key, expected in required.items()) or
                    any(data.get(key) is not True for key in
                        ('cp_completed', 'reused_ib', 'imported_map_null', 'buffer_released', 'dmabuf_imported')) or
                    data.get('display_presented') is not False or
                    data.get('ahb_outside_payload_checked') is not False or
                    not data.get('guard_byte_checks') or not data.get('readonly_byte_checks') or
                    data.get('cleanup_errno') or data.get('submission') != 'accepted' or
                    data.get('pixel_offset', -1) < 0 or
                    data.get('fd_bytes', 0) < data.get('pixel_offset', 0) + 16384 or
                    len(evidence.get('images', {})) != 4 or
                    any(v['bytes'] != 16384 for v in evidence['images'].values())):
                raise RuntimeError('result does not prove direct KGSL CP/draw into the identified AHB backing')
        if record.get('nvn_api') and (data.get('operation') != 'nvn_clear_buffer' or
                not all(data.get(k) for k in ('imported_storage', 'overlapping_views_correct',
                                              'object_guards_intact', 'inflight_destroy_rejected'))):
            raise RuntimeError('NVN API result did not prove the required resource checks')
        if record.get('compute_driverparams_check'):
            validate_driverparams(record,data)
        if record.get('shader_check'):
            case = record['shader_check']['cases'][record.get('shader_case') or 0]
            expected = case['metadata']['expected']
            if (data.get('operation') != 'compute_shader' or data['expected'] != expected or
                    data['observed'] != expected or not data['completed'] or
                    data['submission'] != 'accepted' or data['cleanup_errno']):
                raise RuntimeError('result does not match the verified compute artifact')
        if record.get('compute_call'):
            from compute_call_validation import validate as validate_compute_call
            validate_compute_call(record,data)
        elif record.get('compute_indirect'):
            validate_compute_indirect(record,data)
        elif record.get('maxwell_check'):
            if (data.get('operation') != 'nvn_maxwell_compute' or data.get('completed_cases') != 12 or
                data.get('inflight_program_destroy_rejections') != 12 or data.get('input_miss_rejections') != 4 or
                data.get('repeated_handle_cases') != 4 or
                not data.get('nvn_buffer_slots') or data.get('binding_rejections') != 3 or
                len(data.get('cases', [])) != 12 or not all(c.get('passed') for c in data['cases'])):
                raise RuntimeError('result does not prove original Maxwell NVN compute qualification')
        if record.get('graphics_draw_batch'):
            validate_draw_batch(record, data, draw_batch_bytes)
            evidence['draw_batch_readbacks_verified'] = True
        elif record.get('graphics_check') or record.get('nvn_graphics_check'):
            operation = 'nvn_maxwell_graphics' if record.get('nvn_graphics_check') else 'maxwell_graphics'
            if (data.get('operation') != operation or data.get('completed_cases') != 4 or
                    data.get('channel_checks') != 64 * 64 * 4 * 4 or not data.get('guard_byte_checks') or
                    data.get('cleanup_errno') or data.get('submission') != 'accepted' or not data.get('reused_ib') or
                    len(evidence.get('images', {})) != 4 or
                    any(v['bytes'] != data['target_bytes'] for v in evidence['images'].values())):
                raise RuntimeError('result does not prove original VS/FS draw and pixel qualification')
        if record.get('texture_check') or record.get('nvn_texture_check'):
            operation = 'nvn_maxwell_texture' if record.get('nvn_texture_check') else 'maxwell_texture'
            if (data.get('operation') != operation or data.get('completed_cases') != 4 or
                    data.get('channel_checks') != 65536 or not data.get('guard_byte_checks') or
                    not data.get('readonly_byte_checks') or data.get('outside_pixels') != 1024 or
                    data.get('discard_pixels') != 3840 or data.get('shaded_pixels') != 11520 or
                    data.get('cleanup_errno') or data.get('submission') != 'accepted' or not data.get('reused_ib') or
                    len(evidence.get('images', {})) != 4 or
                    any(v['bytes'] != data['target_bytes'] for v in evidence['images'].values())):
                raise RuntimeError('result does not prove original sampled VS/FS pixels and resource updates')
        if record.get('nvn_texture_check'):
            validate_descriptor_audit(record, data)
            identity = record['nvn_contract']
            flags = record.get('texture_device_flags')
            if (data.get('abi_profile') != identity['profile'] or
                    data.get('api_major') != identity['reported_api']['major'] or
                    data.get('api_minor') != identity['reported_api']['minor'] or
                    data.get('device_flags_explicit') != (flags is not None) or
                    data.get('device_flags') != (flags if flags is not None else 0)):
                raise RuntimeError('NVN device flags/partial-ABI identity were not qualified')
            noaccess = record.get('texture_noaccess_pool', False)
            uncached = record.get('texture_uncached_pools', False)
            if data.get('uncached_pools_enabled') is not uncached:
                raise RuntimeError('CPU_UNCACHED pool qualifier does not match the build')
            if data.get('noaccess_pool_enabled') != noaccess:
                raise RuntimeError('NO_ACCESS pool qualification mode does not match the build')
            if noaccess:
                pool_checks = {'noaccess_pool_flags': 0xa1, 'noaccess_map_checks': 2,
                               'noaccess_gpuva_checks': 2, 'noaccess_pool_flush_calls': 0 if uncached else 5,
                               'noaccess_pool_invalidate_calls': 0 if uncached else 5,
                               'noaccess_buffer_cache_calls': 0 if uncached else 2,
                               'noaccess_range_rejections': 0 if uncached else 4, 'noaccess_texel_seed_bytes': 16,
                               'noaccess_host_payload_guard_checks': 0, 'noaccess_diagnostic_cases': 1,
                               'noaccess_diagnostic_channel_checks': 16384}
                if any(data.get(key) != value for key, value in pool_checks.items()):
                    raise RuntimeError('NO_ACCESS pool sampling, NULL maps and mapped-cache contract were not qualified')
            required = {'submissions': 5, 'recorded_handles': 2, 'binding_rejections': 3,
                        'binding_array_copies': 4, 'late_descriptor_registrations': 2,
                        'texture_registrations': 4, 'sampler_registrations': 2,
                        'texture_object_finalizations': 3, 'sampler_object_finalizations': 1,
                        'backing_pool_busy_rejections': 3, 'descriptor_pool_busy_rejections': 4,
                        'program_busy_rejections': 5, 'pool_identity_cases': 1,
                        'diagnostic_channel_checks': 16384, 'diagnostic_shaded_pixels': 4096}
            if (any(data.get(key) != expected for key, expected in required.items()) or
                    len(evidence.get('diagnostic_images', {})) != 1 or
                    any(v['bytes'] != data['target_bytes'] for v in evidence['diagnostic_images'].values())):
                raise RuntimeError('result does not prove NVN handle domains, descriptor consumption and pool ownership')
            if uncached:
                draws = 17 if record.get('texture_modes') else 5
                required = {'uncached_pool_flags': 0x22, 'uncached_shader_pool_flags': 0x62,
                            'uncached_pool_map_checks': 11 if noaccess else 12, 'uncached_buffer_map_checks': 6,
                            'public_cache_calls': 0, 'skipped_pool_flush_calls': 1 + 6 * draws,
                            'skipped_pool_invalidate_calls': 12 * draws,
                            'skipped_buffer_cache_calls': 2 if noaccess else 0,
                            'skipped_cache_range_checks': 4 if noaccess else 0,
                            'hot_target_preloads': draws, 'hot_target_word_checks': 4096 * draws}
                if any(type(data.get(key)) is not int or data[key] != value for key, value in required.items()):
                    raise RuntimeError('result does not prove same-pointer CPU_UNCACHED shader reads/writes without public cache calls')
        if record.get('texture_modes'):
            required = {'mode_submissions': 12, 'mode_recorded_handles': 12, 'mode_released_handles': 12,
                        'mode_completed_cases': 12, 'mode_channel_checks': 196608,
                        'mode_shaded_pixels': 18432, 'mode_discard_pixels': 6144,
                        'mode_depth_clipped_pixels': 24576, 'mode_snapshot_mutations': 12,
                        'mode_clipped_case_mask': 0xb2c, 'mode_getter_checks': 50,
                        'mode_program_busy_rejections': 12,
                        'mode_noaccess_pool_flush_calls': 12 if record.get('texture_noaccess_pool') and not record.get('texture_uncached_pools') else 0,
                        'mode_noaccess_pool_invalidate_calls': 12 if record.get('texture_noaccess_pool') and not record.get('texture_uncached_pools') else 0}
            if (data.get('texture_modes') is not True or
                    any(type(data.get(key)) is not int or data[key] != value for key, value in required.items()) or
                    any(type(data.get(key)) is not int or data[key] <= 0 for key in
                        ('mode_guard_byte_checks', 'mode_readonly_byte_checks')) or
                    set(mode_bytes) != set(MODE_NAMES) or any(len(v) != 16384 for v in mode_bytes.values()) or
                    not isinstance(data.get('mode_readback_names'), list) or
                    len(data['mode_readback_names']) != 12 or set(data['mode_readback_names']) != set(MODE_NAMES)):
                raise RuntimeError('result does not prove all12 origin/depth cases and recorded-state snapshots')
            clear = bytes((17, 29, 43, 59)) * 4096
            for o in range(2):
                positive = mode_bytes[f'mode-o{o}-d0-zpos.rgba']
                if (positive == clear or positive != mode_bytes[f'mode-o{o}-d1-zpos.rgba'] or
                        positive != mode_bytes[f'mode-o{o}-d0-zneg.rgba']):
                    raise RuntimeError('positive/negative Z visibility differs from the declared depth contract')
                for d, z in ((0, 'far'), (1, 'far'), (1, 'neg')):
                    if mode_bytes[f'mode-o{o}-d{d}-z{z}.rgba'] != clear:
                        raise RuntimeError('clipped mode case contains non-clear pixels')
            positive = mode_bytes['mode-o0-d0-zpos.rgba']
            mirrored = b''.join(positive[y * 256:(y + 1) * 256] for y in reversed(range(64)))
            if mode_bytes['mode-o1-d0-zpos.rgba'] != mirrored:
                raise RuntimeError('origin1 is not the exact vertical mirror of origin0')
            evidence['mode_readbacks_verified'] = True
            args.evidence.write_text(json.dumps(evidence, indent=2) + '\n')
    except Exception as error:
        if fullsize_nonce:
            saved=locals().get('evidence')
            if not isinstance(saved,dict):saved={'run_nonce':fullsize_nonce,'build':record}
            saved['screen_and_lifecycle_verified']=False;saved['fullsize_error']=str(error)
            if 'result' in locals():saved['raw_result_stdout']=result.stdout;saved['raw_result_stderr']=result.stderr;saved['raw_result_returncode']=result.returncode
            args.evidence.parent.mkdir(parents=True,exist_ok=True);args.evidence.write_text(json.dumps(saved,indent=2)+'\n')
        if copy_nonce:
            saved = locals().get('evidence')
            if not isinstance(saved, dict):
                saved = {'scope': 'failed copy qualification; no hardware success claimed', 'build': record,
                         'run_nonce': copy_nonce, 'installed_apk_sha256': installed_sha}
            saved['copy_validation_error'] = str(error)
            saved['copy_verified'] = False
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            if 'copy_images' not in saved:
                try:
                    collect_copy_readbacks(run, adb, package, copy_directory, copy_names, args.evidence, saved)
                except Exception as collection_error:
                    saved['copy_collection_error'] = str(collection_error)
            if 'result' not in saved and 'result' in locals():
                saved['raw_result_output'] = result.stdout
            args.evidence.write_text(json.dumps(saved, indent=2) + '\n')
        raise
    finally:
        if copy_nonce:
            final_evidence = locals().get('evidence')
            if args.evidence.is_file():
                final_evidence = json.loads(args.evidence.read_text())
            if not isinstance(final_evidence, dict):
                final_evidence = {'run_nonce': copy_nonce, 'build': record, 'copy_verified': False}
            try:
                run('shell', 'am', 'force-stop', package)
                alive = run('shell', 'pidof', package, check=False)
                if alive.stdout.strip():
                    raise RuntimeError('probe process is still running after force-stop')
                if alive.returncode != 1 or alive.stderr.strip():
                    raise RuntimeError('pidof did not prove process absence after force-stop')
                final_evidence['probe_stopped'] = True
            except Exception as stop_error:
                final_evidence['probe_stopped'] = False
                final_evidence['copy_verified'] = False
                final_evidence['stop_error'] = str(stop_error)
                args.evidence.parent.mkdir(parents=True, exist_ok=True)
                args.evidence.write_text(json.dumps(final_evidence, indent=2) + '\n')
                raise
            args.evidence.parent.mkdir(parents=True, exist_ok=True)
            args.evidence.write_text(json.dumps(final_evidence, indent=2) + '\n')
        elif fullsize_nonce:
            saved=json.loads(args.evidence.read_text()) if args.evidence.is_file() else {'run_nonce':fullsize_nonce,'build':record}
            if record.get('window_source_check'):
                saved['window_source_present_interval']=args.window_source_present_interval
                saved['window_interval_config']=interval_config
            try:
                run('shell','am','force-stop',package)
                alive=run('shell','pidof',package,check=False)
                if alive.returncode!=1 or alive.stdout.strip() or alive.stderr.strip():
                    raise RuntimeError('pidof did not prove fullsize probe process absence')
                saved['probe_stopped']=True
            except Exception as stop_error:
                saved['probe_stopped']=False;saved['screen_and_lifecycle_verified']=False;saved['stop_error']=str(stop_error)
                args.evidence.parent.mkdir(parents=True,exist_ok=True);args.evidence.write_text(json.dumps(saved,indent=2)+'\n');raise
            args.evidence.parent.mkdir(parents=True,exist_ok=True);args.evidence.write_text(json.dumps(saved,indent=2)+'\n')
        else:
            run('shell', 'am', 'force-stop', package)
            alive = run('shell', 'pidof', package, check=False)
            if alive.stdout.strip():
                raise RuntimeError('probe process is still running after force-stop')


if __name__ == '__main__':
    main()
