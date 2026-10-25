#!/usr/bin/env python3
"""Verify and run the original ARM64 CLI host as the probe package's ordinary UID.

This is an explicit deployment/execution tool, not a CPU runtime or Activity
launcher. Exit78 records an unsupported NVN boundary, never a successful stage
or a first frame. With no --romfs it supplies an explicitly empty resource
directory; missing game resources remain a real, reported limitation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time
import uuid
import zipfile

PACKAGE = 'org.nvn.nativeprobe'
HOST_MEMBER = 'lib/arm64-v8a/libnvn_d2r_host.so'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify_hashes(mapping, base=None):
    if not isinstance(mapping, dict) or not mapping:
        raise ValueError('missing nonempty hash inventory')
    for name, expected in mapping.items():
        path = Path(name)
        if not path.is_absolute():
            if base is None:
                raise ValueError(f'absolute recorded path required: {name}')
            path = base / path
        if not path.is_file() or sha(path) != expected:
            raise ValueError(f'integrity mismatch: {path}')


def host_verify_argv(host, root):
    options = host['options']
    if (options.get('host_abi') != 1 or options.get('rewrite_abi') != 'elf' or
            options.get('rules') != 'native' or str(options.get('private_gates')) != '1' or
            options.get('abi') != 'arm64-v8a' or options.get('execute') is not False or
            options.get('deploy') is not False):
        raise ValueError('requires the original ELF/HOST_ABI1 native-rule build record')
    modules = Path(host['modules']).resolve()
    argv=[options['python'], str(root / 'nvn-native/tools/build_d2r_host.py'),
            '--python', options['python'], '--exefs', options['exefs'], '--ndk', options['ndk'], '--out', str(modules.parent),
            '--jobs', str(options['jobs']), '--verify']
    if options.get('compute_cache_check'):argv+=['--compute-cache-check',options['compute_cache_check']]
    if options.get('texture_bridge_check'):argv+=['--texture-bridge-check',options['texture_bridge_check']]
    if options.get('block_copy_check'):argv+=['--block-copy-check',options['block_copy_check']]
    if options.get('bc_sampled_fdl_check'):argv+=['--bc-sampled-fdl-check',options['bc_sampled_fdl_check']]
    if 'depth_targets' in options and type(options['depth_targets'])is not bool:raise ValueError('depth feature option must be boolean')
    if options.get('depth_targets'):argv+=['--depth-targets']
    if 'graphics_storage' in options and type(options['graphics_storage'])is not bool:raise ValueError('graphics storage option must be boolean')
    if options.get('graphics_storage'):argv+=['--graphics-storage']
    if 'depth_sampled' in options and type(options['depth_sampled'])is not bool:raise ValueError('depth sampling option must be boolean')
    if options.get('depth_sampled'):argv+=['--depth-sampled']
    for key in ('depth_fdl_check','depth_a2d_check','depth_runtime_check'):
        if options.get(key):argv+=['--'+key.replace('_','-'),options[key]]
    for option in ('sampled_images','sampled_mip_filter','gather'):
        if option in options and type(options[option])is not bool:raise ValueError('sampled feature option must be boolean: '+option)
        if options.get(option):argv+=['--'+option.replace('_','-')]
    if options.get('pipeline_target_fdl_check'):argv+=['--pipeline-target-fdl-check',options['pipeline_target_fdl_check']]
    for key in ('clear_color_kernel_check','clear_color_constant_check'):
        if options.get(key):argv+=['--'+key.replace('_','-'),options[key]]
    if options.get('color_targets'):argv+=['--color-targets']
    if options.get('bntx_packaged_layout_experiment'):argv+=['--bntx-packaged-layout-experiment']
    if options.get('graphics_pipeline_check'):argv+=['--graphics-pipeline-check',options['graphics_pipeline_check']]
    if 'shader_source_check' in options and options['shader_source_check'] is not None:
        if not isinstance(options['shader_source_check'],str) or not options['shader_source_check']:raise ValueError('source check option must be a path string')
        argv+=['--shader-source-check',options['shader_source_check']]
    return argv


def device_romfs_source(value):
    package, separator, relative = value.partition(':')
    if (not separator or package == PACKAGE or
            not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+', package) or
            not relative or relative.startswith('/') or '\\' in relative or
            any(ord(c) < 32 or ord(c) == 127 for c in relative) or
            any(part in ('', '.', '..') for part in relative.split('/'))):
        raise argparse.ArgumentTypeError('expected a different source PACKAGE:RELATIVE RomFS directory without absolute/dot/control paths')
    return {'package': package, 'relative_directory': relative, 'method': 'same-device read-only run-as pipe; local original SHA256 authoritative'}


def classify(stage, exit_code, timed_out, stdout, stderr):
    text = stdout + '\n' + stderr
    # Bootstrap observations share the driver prefix, but are not failures.
    # Only the adapter's explicit terminal diagnostic proves an NVN boundary.
    errors = re.findall(r'^.*\[native NVN D2R\].*terminating execution \(exit 78\).*$', text, re.M)
    named_errors = re.findall(r'^.*(?:\[native NVN D2R\].*terminating execution \(exit 78\)|\[dm\] FATAL|native fault signal=).*$', text, re.M)
    observed_flags = [int(value, 16) for value in re.findall(
        r'\[native NVN D2R\] nvnDeviceBuilderSetFlags: builder=\S+ flags=0x([0-9a-fA-F]{8})', text)]
    pool_access = [dict(pool=pool, flags=int(flags,16), policy=int(policy),
                        offset=int(offset), size=int(size), consumer=consumer,
                        guest_pc=None)
                   for pool,flags,policy,offset,size,consumer in re.findall(
        r'\[native NVN D2R\] pool_access: pool=(\S+) flags=0x([0-9a-fA-F]{8}) '
        r'policy=(\d+) offset=(\d+) size=(\d+) consumer=(.*?) guest_pc=unavailable(?:\n|$)', text)]
    guest_errors = [line for line in text.splitlines() if '[guest]' in line and
                    re.search(r'\b(?:failed|failure|fatal|error)\b', line, re.I)]
    resource_failure = bool(re.search(
        r'\[guest\].*(?:Failed to open the config file for reading|Failed to load build config)', text, re.I))
    initialization_error = bool(re.search(
        r'\[guest\].*(?:TACT Failed to create an instance|\[Init\]: Failed to initialize data)', text, re.I))
    reached_main = bool(re.search(r'native >>> nnMain(?:\s|$)', text))
    # Original --run calls nnMain through run0(), whose entry log is the proof.
    expected = {'sdk-init': 'sdk-initialized', 'constructors': 'constructors-completed',
                'run': 'nnMain-returned'}[stage]
    stage_marker = bool(re.search(r'"stage"\s*:\s*"' + expected + '"', text))
    completed = not timed_out and exit_code == 0 and stage_marker
    passed = completed and not guest_errors and not resource_failure and not initialization_error
    boundary = not timed_out and exit_code == 78 and bool(errors)
    return {'stage_completed': completed, 'passed': passed, 'unsupported_boundary': boundary,
            'guest_initialization_error': initialization_error, 'resource_failure': resource_failure,
            'guest_error_lines': guest_errors,
            'reached_nnMain': reached_main, 'first_named_nvn_error': errors[0] if errors else None,
            'observed_device_flags': observed_flags,
            'pool_access_observations': pool_access,
            'nnMain_observation': 'original host entry log before indirect call; not a first-frame claim',
            'first_named_error': named_errors[0] if named_errors else guest_errors[0] if guest_errors else None,
            'first_frame': False, 'outcome': 'timeout' if timed_out else
            'guest-resource-failure' if resource_failure else
            'guest-initialization-failure' if initialization_error else
            'guest-error' if guest_errors else 'unsupported-nvn-boundary' if boundary else
            'stage-completed' if passed else 'failed'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--build', type=Path, required=True, help='APK build directory with build-record.json')
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--stage', choices=('sdk-init', 'constructors', 'run'), required=True)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--timeout', type=float, default=20, help='Host execution timeout in seconds (default20)')
    parser.add_argument('--romfs', type=Path, help='Explicit original local RomFS directory; no resources are fabricated')
    parser.add_argument('--reuse-romfs', action='store_true',
                        help='Reuse the probe private RomFS only after exact inventory and per-file local-original SHA256 verification; requires --romfs')
    parser.add_argument('--device-romfs-source', type=device_romfs_source,
                        help='PACKAGE:RELATIVE original RomFS on the same device; requires --romfs for complete local hash verification')
    args = parser.parse_args()
    if not 0 < args.timeout <= 300:
        parser.error('--timeout must be greater than0 and at most300 seconds')
    if args.device_romfs_source and not args.romfs:
        parser.error('--device-romfs-source requires the local original --romfs directory')
    if args.reuse_romfs and not args.romfs:
        parser.error('--reuse-romfs requires the local original --romfs directory')
    if args.reuse_romfs and args.device_romfs_source:
        parser.error('--reuse-romfs cannot be combined with --device-romfs-source')
    root = Path(__file__).resolve().parents[2]
    native = root / 'nvn-native'
    build, evidence_path = args.build.resolve(), args.evidence.resolve()
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    if evidence_path.exists():
        parser.error('evidence path already exists; use a new path for each real execution')
    token = uuid.uuid4().hex
    evidence = {'schema': 1, 'scope': 'Original ARM64 CLI modules through existing loader/HLE; ordinary UID run-as shell, not Activity execution',
                'serial': args.serial, 'stage': args.stage, 'run_token': token,
                'runner': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))},
                'commands': [], 'deployed': False, 'host_execution_attempted': False,
                'first_frame': False, 'passed': False, 'timeout_seconds': args.timeout}
    adb = None
    device_tmp = None
    execution_root = None
    host_binary = None
    host_pid = None
    phase = 'local-preflight'

    def command(argv, check=True, timeout=20, binary=False):
        argv = list(map(str, argv))
        try:
            result = subprocess.run(argv, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            evidence['commands'].append({'argv': argv, 'timed_out': True,
                'stdout': (error.stdout or b'').decode(errors='replace'),
                'stderr': (error.stderr or b'').decode(errors='replace')})
            raise
        item = {'argv': argv, 'returncode': result.returncode}
        if binary:
            item.update(stdout_sha256=hashlib.sha256(result.stdout).hexdigest(), stdout_bytes=len(result.stdout),
                        stderr=result.stderr.decode(errors='replace'))
        else:
            item.update(stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'))
        evidence['commands'].append(item)
        if check and result.returncode:
            raise ValueError(f'command failed ({result.returncode}): {shlex.join(argv)}; {item.get("stderr", "")}')
        return result

    def remote(argv, **kwargs):
        return command(adb + ['shell', shlex.join(list(map(str, argv)))], **kwargs)

    def app(argv, **kwargs):
        return remote(['run-as', PACKAGE] + list(map(str, argv)), **kwargs)

    def app_script(script, **kwargs):
        return app(['sh', '-c', script], **kwargs)

    def device_sha(path, ordinary=False):
        result = (app if ordinary else remote)(['sha256sum', path], timeout=180)
        fields = result.stdout.decode().split()
        value = fields[0] if fields else ''
        if not re.fullmatch('[0-9a-f]{64}', value):
            raise ValueError(f'invalid device SHA256 response for {path}')
        return value

    def capture_file(remote_path, suffix):
        result = app(['cat', remote_path], check=False, binary=True)
        path = evidence_path.with_name(evidence_path.stem + suffix)
        path.write_bytes(result.stdout)
        return {'path': str(path), 'sha256': sha(path), 'bytes': len(result.stdout),
                'read_returncode': result.returncode}, result.stdout.decode(errors='replace')

    def stop_own_host():
        nonlocal host_pid
        if not execution_root or not host_binary:
            return
        pid_result = app(['cat', execution_root + '/pid'], check=False)
        value = pid_result.stdout.decode().strip()
        if not re.fullmatch('[0-9]+', value) or int(value) < 2:
            evidence['cleanup'] = {'pid_available': False, 'host_not_started_or_pid_missing': True}
            if evidence['host_execution_attempted']:
                raise ValueError('cannot prove host exit: missing valid per-execution PID')
            return
        host_pid = int(value)
        exe = app(['readlink', f'/proc/{host_pid}/exe'], check=False)
        if exe.returncode:
            alive = app(['kill', '-0', host_pid], check=False)
            if alive.returncode == 0:
                raise ValueError('host PID exists but its executable cannot be verified; refusing broad kill')
            evidence['cleanup'] = {'pid': host_pid, 'confirmed_exited': True, 'signals': []}
            return
        if exe.stdout.decode().strip() != host_binary:
            raise ValueError('PID executable differs from this host; refusing to kill an unrelated process')
        recorded_stat = app(['cat', execution_root + '/start-stat'], check=False).stdout.decode()
        def starttime(stat):
            fields = stat.rsplit(')', 1)[-1].split()
            if len(fields) < 20 or not fields[19].isdigit():
                raise ValueError('cannot verify the per-execution PID start identity')
            return fields[19]
        expected_start = starttime(recorded_stat)
        signals = []
        for signal in ('TERM', 'KILL'):
            current_stat = app(['cat', f'/proc/{host_pid}/stat'], check=False)
            if current_stat.returncode:
                if app(['kill', '-0', host_pid], check=False).returncode:
                    evidence['cleanup'] = {'pid': host_pid, 'confirmed_exited': True, 'signals': signals}
                    return
                raise ValueError('live host PID has no readable start identity')
            if starttime(current_stat.stdout.decode()) != expected_start:
                raise ValueError('host PID was recycled; refusing to signal it')
            app(['kill', '-' + signal, host_pid], check=False)
            signals.append(signal)
            for _ in range(10):
                if app(['kill', '-0', host_pid], check=False).returncode:
                    evidence['cleanup'] = {'pid': host_pid, 'confirmed_exited': True, 'signals': signals}
                    return
                time.sleep(.1)
            # Verify identity again before escalation; a recycled PID is never killed.
            exe = app(['readlink', f'/proc/{host_pid}/exe'], check=False)
            if exe.returncode or exe.stdout.decode().strip() != host_binary:
                raise ValueError('cannot revalidate host identity before signal escalation')
        raise ValueError('the per-execution host PID is still alive after KILL')

    try:
        record_path = build / 'build-record.json'
        record = json.loads(record_path.read_text())
        host = record.get('d2r_host')
        if not isinstance(host, dict):
            raise ValueError('APK record must embed the complete d2r_host build record')
        evidence.update(build_record_path=str(record_path), build_record_sha256=sha(record_path), build=record)
        verify_hashes(record['source'], native)
        verify_hashes(record['tools'])
        verify_hashes(record['artifacts'])
        if record.get('compiled_inputs'):
            verify_hashes(record['compiled_inputs'])
        for field in ('sources', 'tools', 'artifacts'):
            verify_hashes(host[field])
        for item in host['inputs'].values():
            if sha(item['path']) != item['sha256'] or Path(item['path']).stat().st_size != item['bytes']:
                raise ValueError('original NSO identity changed')
        modules = Path(host['modules']).resolve()
        host_record_path = modules.parent / 'build-record.json'
        if json.loads(host_record_path.read_text()) != host:
            raise ValueError('embedded host record differs from canonical host build record')
        command(host_verify_argv(host, root), timeout=300)
        apk = build / 'probe.apk'
        if str(apk) not in record['artifacts']:
            raise ValueError('APK is not a recorded artifact')
        expected_host = host['artifacts'][host['executable']]
        with zipfile.ZipFile(apk) as archive:
            if archive.namelist().count(HOST_MEMBER) != 1:
                raise ValueError('APK must contain exactly one original CLI host member')
            with archive.open(HOST_MEMBER) as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != expected_host:
                    raise ValueError('APK host member differs from verified executable')
        module_files = sorted(p for p in modules.iterdir() if p.is_file())
        if not module_files or any(p.is_symlink() or str(p) not in host['artifacts'] for p in module_files):
            raise ValueError('module directory contains unrecorded or symlinked files')
        manifest = json.loads((modules / 'rewrite-manifest.json').read_text())
        for name in ('main', 'sdk', 'subsdk0'):
            for suffix in ('.patched.bin', '.fixups.tsv', '.gates.tsv'):
                if modules / (name + suffix) not in module_files:
                    raise ValueError('incomplete original module deployment set')
            if manifest['modules'][name]['recipe']['input']['sha256'] != host['inputs'][name]['sha256']:
                raise ValueError('rewrite manifest is not tied to the original input')
        romfs_files = []
        if args.romfs:
            romfs = args.romfs.resolve()
            if not romfs.is_dir():
                raise ValueError('--romfs requires a real directory')
            for path in sorted(romfs.rglob('*')):
                relative = path.relative_to(romfs)
                if path.is_symlink() or any(ord(c) < 32 or c == '\\' for c in str(relative)):
                    raise ValueError(f'unsupported symlink/control-character RomFS path: {relative}')
                if path.is_file():
                    romfs_files.append((path, str(relative), sha(path)))
                elif not path.is_dir():
                    raise ValueError(f'unsupported RomFS entry: {relative}')
        evidence['romfs'] = {'source': str(args.romfs.resolve()) if args.romfs else None,
            'files': {rel: value for _, rel, value in romfs_files}, 'empty': not romfs_files,
            'device_source': args.device_romfs_source,
            'reuse_requested': args.reuse_romfs, 'reused': False,
            'verified_files': 0, 'verified_bytes': 0,
            'limitation': 'No game resources fabricated. An empty RomFS can block real game startup.' if not romfs_files else None}
        found = shutil.which('adb')
        if not found:
            raise ValueError('adb not found')
        adb = [found, '-s', args.serial]
        evidence['adb'] = {'path': found, 'sha256': sha(found)}
        phase = 'installed-artifact-verification'
        if args.install:
            installed = command(adb + ['install', '--no-incremental', '-r', str(apk)], timeout=180)
            if b'Success' not in installed.stdout:
                raise ValueError('APK installation did not report Success')
        paths = remote(['pm', 'path', PACKAGE]).stdout.decode().strip().splitlines()
        if len(paths) != 1 or not paths[0].startswith('package:'):
            raise ValueError('expected one installed APK without splits')
        installed_apk = paths[0][8:]
        installed_sha = device_sha(installed_apk)
        if installed_sha != record['artifacts'][str(apk)]:
            raise ValueError('installed APK differs from verified build')
        expected_lib = str(Path(installed_apk).parent / 'lib/arm64')
        package_dump = remote(['dumpsys', 'package', PACKAGE]).stdout.decode()
        native_dirs = re.findall(r'\bnativeLibraryDir=(\S+)', package_dump)
        legacy_dirs = re.findall(r'\blegacyNativeLibraryDir=(\S+)', package_dump)
        primary_abis = re.findall(r'\bprimaryCpuAbi=(\S+)', package_dump)
        if expected_lib in native_dirs:
            library_contract = 'nativeLibraryDir'
        elif str(Path(installed_apk).parent / 'lib') in legacy_dirs and 'arm64-v8a' in primary_abis:
            library_contract = 'legacyNativeLibraryDir + primaryCpuAbi=arm64-v8a'
        else:
            raise ValueError('package native-library directory/ABI does not match APK parent/lib/arm64')
        host_binary = expected_lib + '/libnvn_d2r_host.so'
        if device_sha(host_binary) != expected_host:
            raise ValueError('installed host executable differs from APK/original verified host')
        remote(['test', '-x', host_binary])
        uid = int(app(['id', '-u']).stdout.decode().strip())
        context = app(['sh', '-c', 'id; cat /proc/self/attr/current']).stdout.decode()
        if uid < 10000:
            raise ValueError('host must execute under the ordinary package UID')
        package_uid = remote(['cmd', 'package', 'list', 'packages', '-U', PACKAGE]).stdout.decode()
        if not re.search(r'\buid:' + str(uid) + r'\b', package_uid):
            raise ValueError('run-as UID differs from package UID')
        resolved_host = app(['readlink', '-f', host_binary]).stdout.decode().strip()
        if (not resolved_host.startswith('/data/app/') or
                Path(resolved_host).name != 'libnvn_d2r_host.so' or
                device_sha(resolved_host, ordinary=True) != expected_host):
            raise ValueError('run-as resolved host path/hash differs from installed verified host')
        app(['test', '-x', resolved_host])
        host_binary = resolved_host
        app_home = app(['pwd']).stdout.decode().strip()
        if not app_home.startswith('/data/') or not app_home.endswith('/' + PACKAGE):
            raise ValueError('unexpected run-as data directory')
        evidence.update(installed_apk={'path': installed_apk, 'sha256': installed_sha},
            installed_host={'path': host_binary, 'sha256': expected_host}, uid=uid,
            run_as_context=context, package_uid=package_uid, nativeLibraryDir=expected_lib,
            native_library_contract=library_contract,
            execution_context='run-as shell (normally SELinux runas_app); not Activity/untrusted_app context',
            fingerprint=remote(['getprop', 'ro.build.fingerprint']).stdout.decode().strip(),
            kernel=remote(['uname', '-r']).stdout.decode().strip())
        phase = 'isolated-deployment'
        device_tmp = '/data/local/tmp/nvn-d2r-' + token
        isolated = app_home + '/files/d2r-host'
        execution_root = isolated + '/runs/' + token
        module_dest = isolated + '/modules'
        # Refuse parallel invocations that could replace modules during execution.
        app(['mkdir', '-p', isolated])
        app(['mkdir', isolated + '/runner.lock'])
        evidence['device_lock'] = isolated + '/runner.lock'
        romfs_dest = isolated + '/game/romfs'
        if args.reuse_romfs:
            phase = 'romfs-reuse-verification'

            def reuse_inventory():
                # Inspect every private ancestor too: find's root argument can
                # traverse a symlink before its normal no-follow walk begins.
                for directory in (app_home + '/files', isolated, isolated + '/game', romfs_dest):
                    app(['test', '!', '-L', directory])
                app(['test', '-d', romfs_dest])
                unusual = app(['find', romfs_dest, '!', '-type', 'f', '!', '-type', 'd', '-print0'], binary=True)
                if unusual.stdout:
                    raise ValueError('reused RomFS contains a symlink or non-regular entry')
                raw = app(['find', romfs_dest, '-type', 'f', '-print0'], binary=True).stdout
                if raw and not raw.endswith(b'\0'):
                    raise ValueError('invalid NUL-delimited reused RomFS inventory')
                prefix = (romfs_dest + '/').encode()
                paths = raw[:-1].split(b'\0') if raw else []
                if len(paths) != len(set(paths)) or any(not path.startswith(prefix) for path in paths):
                    raise ValueError('invalid reused RomFS paths')
                expected = {(romfs_dest + '/' + relative).encode() for _, relative, _ in romfs_files}
                if set(paths) != expected:
                    raise ValueError('reused RomFS file inventory differs from local original')

            reuse_inventory()
            for path, relative, expected in romfs_files:
                if sha(path) != expected:
                    raise ValueError('RomFS source changed during reuse verification')
                if device_sha(romfs_dest + '/' + relative, ordinary=True) != expected:
                    raise ValueError('reused RomFS hash mismatch: ' + relative)
                evidence['romfs']['verified_files'] += 1
                evidence['romfs']['verified_bytes'] += path.stat().st_size
            # Detect additions/removals/symlinks while the large files hashed.
            # The runner lock excludes cooperating deployments for the entire run.
            reuse_inventory()
            evidence['romfs']['reused'] = True
            phase = 'isolated-deployment'
        remote(['mkdir', '-m', '0755', device_tmp])
        directories = [execution_root, isolated + '/game/save', isolated + '/game/temp', isolated + '/game/root']
        if not args.reuse_romfs:
            directories.append(romfs_dest)
        app(['mkdir', '-p'] + directories)
        staged_modules = isolated + '/modules-' + token
        app(['mkdir', staged_modules])
        deployed = {}
        for path in module_files:
            if sha(path) != host['artifacts'][str(path)]:
                raise ValueError('module changed during deployment')
            temporary = device_tmp + '/' + path.name
            command(adb + ['push', str(path), temporary], timeout=180)
            destination = staged_modules + '/' + path.name
            app(['cp', temporary, destination])
            actual = device_sha(destination, ordinary=True)
            if actual != host['artifacts'][str(path)]:
                raise ValueError('deployed module hash mismatch')
            deployed[path.name] = {'bytes': path.stat().st_size, 'sha256': actual}
        app(['rm', '-rf', module_dest])
        app(['mv', staged_modules, module_dest])
        if not args.reuse_romfs:
            # Recreate only the isolated resource tree, never a save or unrelated app files.
            app(['rm', '-rf', romfs_dest])
            app(['mkdir', romfs_dest])
            for index, (path, relative, expected) in enumerate(romfs_files):
                if sha(path) != expected:
                    raise ValueError('RomFS source changed during deployment')
                destination = romfs_dest + '/' + relative
                app(['mkdir', '-p', str(Path(destination).parent)])
                if args.device_romfs_source:
                    source = args.device_romfs_source
                    source_path = source['relative_directory'] + '/' + relative
                    source_command = ['run-as', source['package'], 'cat', source_path]
                    target_command = ['run-as', PACKAGE, 'sh', '-c',
                                      'umask 077; cat > ' + shlex.quote(destination)]
                    # Android mksh pipefail makes source read errors fatal too,
                    # including when a legitimate local resource is zero bytes.
                    pipeline = 'set -o pipefail; ' + shlex.join(source_command) + ' | ' + shlex.join(target_command)
                    remote(['sh', '-c', pipeline], timeout=180)
                else:
                    temporary = device_tmp + '/romfs-' + str(index)
                    command(adb + ['push', str(path), temporary], timeout=180)
                    app(['cp', temporary, destination])
                if device_sha(destination, ordinary=True) != expected:
                    raise ValueError('deployed RomFS hash mismatch')
                evidence['romfs']['verified_files'] += 1
                evidence['romfs']['verified_bytes'] += path.stat().st_size
        evidence.update(deployed=True, deployed_modules=deployed, module_directory=module_dest,
                        isolated_directory=isolated, execution_directory=execution_root)
        # Recheck identities immediately before real execution, after any transfer.
        verify_hashes(record['source'], native)
        verify_hashes(host['sources'])
        verify_hashes(host['tools'])
        if device_sha(host_binary) != expected_host or device_sha(installed_apk) != installed_sha:
            raise ValueError('installed executable or APK changed before execution')
        phase = 'real-original-arm64-execution'
        environment = {'DM_ROOT': isolated + '/game/root', 'DM_ROMFS': romfs_dest,
            'DM_SAVE': isolated + '/game/save', 'DM_TEMP': isolated + '/game/temp',
            'DM_IMPORTS': '1', 'DM_BOOTPROF': '1', 'DM_FS': '1'}
        evidence['host_environment'] = environment
        argv = [host_binary, module_dest, '--' + args.stage]
        evidence['host_argv'] = argv
        q = shlex.quote
        script = ('umask 077; cd ' + q(isolated + '/game/root') + ' || exit 125; '
            + ' '.join(key + '=' + q(value) for key, value in environment.items())
            + ' ' + shlex.join(argv)
            + ' >' + q(execution_root + '/stdout') + ' 2>' + q(execution_root + '/stderr') + ' & '
            + 'child=$!; echo "$child" >' + q(execution_root + '/pid') + '; '
            + 'cat /proc/"$child"/stat >' + q(execution_root + '/start-stat') + ' 2>/dev/null; '
            + 'cat /proc/"$child"/attr/current >' + q(execution_root + '/child-context') + ' 2>/dev/null; '
            + 'wait "$child"; result=$?; echo "$result" >' + q(execution_root + '/exit') + '; exit "$result"')
        timed_out = False
        evidence['host_execution_attempted'] = True
        try:
            app_script(script, check=False, timeout=args.timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
        stop_own_host()
        evidence['timed_out'] = timed_out
        stdout_file, stdout = capture_file(execution_root + '/stdout', '-stdout.log')
        stderr_file, stderr = capture_file(execution_root + '/stderr', '-stderr.log')
        evidence['logs'] = {'stdout': stdout_file, 'stderr': stderr_file}
        evidence['child_selinux_context'] = app(['cat', execution_root + '/child-context'], check=False).stdout.decode().strip() or None
        status = app(['cat', execution_root + '/exit'], check=False).stdout.decode().strip()
        exit_code = int(status) if re.fullmatch('[0-9]+', status) else None
        evidence['host_exit_code'] = exit_code
        evidence.update(classify(args.stage, exit_code, timed_out, stdout, stderr))
        if stdout_file['read_returncode'] or stderr_file['read_returncode']:
            raise ValueError('host logs could not be fully retrieved')
        phase = 'complete'
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
            subprocess.SubprocessError, zipfile.BadZipFile) as error:
        evidence['error'] = str(error)
        evidence['failed_phase'] = phase
        if evidence['host_execution_attempted']:
            try:
                stop_own_host()
            except (OSError, ValueError, subprocess.SubprocessError) as cleanup_error:
                evidence['cleanup_error'] = str(cleanup_error)
            if execution_root:
                try:
                    out_file, _ = capture_file(execution_root + '/stdout', '-stdout.log')
                    err_file, _ = capture_file(execution_root + '/stderr', '-stderr.log')
                    evidence['logs'] = {'stdout': out_file, 'stderr': err_file}
                except (OSError, ValueError, subprocess.SubprocessError) as log_error:
                    evidence['log_retrieval_error'] = str(log_error)
    finally:
        if adb and device_tmp:
            try:
                remote(['rm', '-rf', device_tmp])
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                evidence['temporary_cleanup_error'] = str(error)
        if adb and evidence.get('device_lock'):
            try:
                if evidence['host_execution_attempted'] and not evidence.get('cleanup', {}).get('confirmed_exited'):
                    evidence['lock_retained'] = True
                else:
                    app(['rmdir', evidence['device_lock']])
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                evidence['lock_cleanup_error'] = str(error)
        evidence['final_phase'] = phase
        if evidence.get('error') or any(value for key, value in evidence.items()
                                      if key.endswith('_error') and key not in ('first_named_error', 'first_named_nvn_error')):
            evidence['passed'] = False
        evidence_path.write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps({key: evidence.get(key) for key in ('stage', 'outcome', 'host_exit_code', 'reached_nnMain',
        'first_named_nvn_error', 'passed', 'error', 'cleanup_error')}, indent=2))
    print(evidence_path)
    return 0 if evidence.get('passed') else 1


if __name__ == '__main__':
    raise SystemExit(main())
