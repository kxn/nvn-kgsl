"""Standard NVN Window lifecycle qualifier; independent shader oracle, no CPU frame copy."""
import hashlib
import io
import json
import math
from pathlib import Path
import re
import subprocess
import time


def exercise(run, adb, package, evidence, destination):
    from PIL import Image
    destination = Path(destination)
    nonce, slots = evidence['run_nonce'], evidence['build']['window_slots']
    if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}', nonce) or type(slots) is not int or slots not in (2, 3):
        raise ValueError('invalid recorded Window run identity/slot count')
    uid_record=evidence.get('package_uid')
    uid_match=(re.fullmatch(r'package:'+re.escape(package)+r' uid:([0-9]+)',uid_record)
               if type(uid_record) is str else None)
    if not uid_match:
        raise ValueError('missing exact package-manager uid evidence')
    expected_uid=int(uid_match[1])
    history, captures, sessions = [], [], []
    evidence.update(lifecycle=history, screenshots=captures, window_sessions=sessions)

    def save():
        destination.write_text(json.dumps(evidence, indent=2) + '\n')

    def integer(value, minimum=0, maximum=(1 << 64)-1):
        return type(value) is int and minimum <= value <= maximum

    def validate_result(data):
        numbers = ('slots','errno','generation','window_created','window_destroyed',
                   'started_sessions','completed_sessions','frames','current_frames','current_case',
                   'current_slot','current_case_mask','pauses','resumes')
        flags = ('ready','worker_running','resources_retained','passed','held_final',
                 'hold_released_by_pause','activity_destroyed','cpu_image_copy','game_modules_executed')
        if (type(data) is not dict or type(data.get('schema')) is not int or data['schema'] != 1 or
                any(not integer(data.get(k)) for k in numbers) or
                any(type(data.get(k)) is not bool for k in flags) or
                any(not integer(data.get(k), -(1 << 31), (1 << 31)-1) for k in
                    ('window_width','window_height','window_screen_x','window_screen_y')) or
                not integer(data.get('uid'), 10000, (1 << 32)-1) or data['uid'] != expected_uid or
                type(data.get('directory')) is not str or type(data.get('phase')) is not str or
                data['cpu_image_copy'] is not False or data['game_modules_executed'] is not False):
            raise RuntimeError('Window result metadata types/required fields differ')

    def present_state(data, iteration):
        expected = dict(phase='presented',ready=True,worker_running=True,current_frames=slots*4,current_case=3,
                        current_case_mask=(1 << (slots*4))-1,held_final=True,hold_released_by_pause=False,
                        activity_destroyed=False,started_sessions=iteration,completed_sessions=iteration-1,
                        window_created=iteration,window_destroyed=iteration-1,frames=slots*4*iteration,passed=False)
        if any(data.get(k) != v for k,v in expected.items()) or not 0 <= data['current_slot'] < slots:
            raise RuntimeError('Window ready result is not held final case for this lifecycle')

    def wait(predicate, stage):
        deadline = time.monotonic() + 25
        while True:
            result = run('shell', 'run-as', package, 'cat', 'files/result.json', check=False)
            if result.returncode == 0:
                try:
                    data = json.loads(result.stdout)
                except json.JSONDecodeError:
                    data = None
                if data:
                    evidence['result'] = data
                    validate_result(data)
                    if (data.get('operation') != 'nvn_window' or data.get('run_nonce') != nonce or
                            data.get('slots') != slots or data.get('owner_thread') != 'worker' or
                            data.get('uid', 0) < 10000):
                        save()
                        raise RuntimeError('Window result belongs to another run/owner')
                    if data.get('errno') or data.get('resources_retained'):
                        history.append(dict(stage=stage, result=data)); save()
                        raise RuntimeError('NVN Window/GPU/compositor failed or retained owners')
                    if predicate(data):
                        history.append(dict(stage=stage, result=data)); save()
                        return data
            if time.monotonic() >= deadline:
                save()
                raise RuntimeError('NVN Window deadline: ' + stage)
            time.sleep(.1)

    def capture(data, stage):
        if data.get('current_frames') != slots * 4 or data.get('current_case') != 3:
            raise RuntimeError('display did not reach final original RGB shader case')
        width, height, left, top = (data[k] for k in ('window_width', 'window_height', 'window_screen_x', 'window_screen_y'))
        for attempt in range(3):
            raw = subprocess.run(adb + ['exec-out', 'screencap', '-p'], check=True, capture_output=True, timeout=20).stdout
            screen = Image.open(io.BytesIO(raw)).convert('RGB')
            path = destination.with_name(destination.stem + '-' + stage + f'-{attempt}.png'); path.write_bytes(raw)
            checks = []
            if width > 0 and height > 0 and left >= 0 and top >= 0 and left + width <= screen.width and top + height <= screen.height:
                for sy in (8, 24, 40, 56):
                    for sx in (8, 24, 40, 56):
                        green, blue = (sx + .5) / 128, (sy + .5) / 128
                        expected = [math.floor(v * 255 + .5) for v in (1-green-blue, green, blue)]
                        point = (left + int((sx+.5)*width/64), top + int((sy+.5)*height/64))
                        actual = list(screen.getpixel(point))
                        checks.append(dict(screen_xy=point, source_xy=[sx,sy], expected=expected, actual=actual,
                                           maximum_delta=max(abs(a-b) for a,b in zip(actual,expected))))
            valid = len(checks) == 16 and all(c['maximum_delta'] <= 8 for c in checks)
            captures.append(dict(stage=stage, generation=data['generation'], run_nonce=nonce,
                                 window_rect=[left,top,width,height], attempt=attempt, path=str(path.resolve()), sha256=hashlib.sha256(raw).hexdigest(),
                                 sample_checks=checks, channel_tolerance=8, passed=valid,
                                 scope='Actual screencap vs independent original-shader oracle; no CPU lock of CURRENT'))
            save()
            if valid:
                return
            time.sleep(.15)
        raise RuntimeError('actual screen does not match standard NVN rendered gradient')

    def collect(data, label):
        probe = data.get('probe', {})
        required = dict(operation='nvn_window',schema=1,slots=slots,errno=0,cleanup_errno=0,
                        uid=data['uid'],chip_id=0x06050002,image_schema=3,
                        completed_cases=slots*4,nvn_submissions=slots*4,window_presentations=slots*4,
                        channel_checks=slots*65536,owner_release_calls=slots,target_cpu_writes=0,raw_draw_calls=0,
                        window_acquire_calls=slots*2,queue_acquire_calls=slots*2,
                        presented_callbacks=slots*4,readback_locks=slots*4,readback_unlocks=slots*4,
                        external_pools_imported=slots,external_pools_released=slots)
        if (type(probe) is not dict or any(type(probe.get(k)) is not type(v) or probe[k] != v for k,v in required.items()) or
                any(probe.get(k) is not False for k in ('resources_retained','game_modules_executed','cpu_image_copy')) or
                any(probe.get(k) is not True for k in ('passed','window_finalized'))):
            raise RuntimeError('standard Window cases/readbacks/ownership did not complete')
        stats=probe.get('sc_before_retire')
        expected_stats=dict(generation=data['generation'],acquires=slots*4,enqueued=slots*4,shown=slots*4,
                            releases=slots*4-1,slots=slots,current_index=data['current_slot'],first_error=0,show_completed=slots*4)
        if (type(stats) is not dict or any(type(stats.get(k)) is not int or stats[k]!=v for k,v in expected_stats.items()) or
                not integer(stats.get('completion_callbacks'), slots*4)):
            raise RuntimeError('Window snapshot does not prove all presentations for this generation')
        if any(not integer(probe.get(k),1) for k in ('readonly_byte_checks','guard_byte_checks','guarded_object_checks')):
            raise RuntimeError('Window read-only or guard checks missing')
        directory = f'window-{nonce}-g{data["generation"]}'
        if Path(data['directory']).name != directory:
            raise RuntimeError('Window generation directory differs from current nonce')
        images = {}
        names = [f'window-slot-{i}-case-{j}.rgba' for i in range(slots) for j in range(4)]
        for name in names:
            p = subprocess.run(adb + ['exec-out','run-as',package,'cat','files/'+directory+'/'+name],capture_output=True,timeout=20)
            if p.returncode:
                raise RuntimeError('missing fresh Window readback: ' + name)
            target = destination.with_name(destination.stem+'-'+label+'-'+name); target.write_bytes(p.stdout)
            images[name] = dict(path=str(target.resolve()),sha256=hashlib.sha256(p.stdout).hexdigest(),bytes=len(p.stdout))
            if len(p.stdout) != 16384:
                raise RuntimeError('Window readback byte count differs')
            case = int(name.split('-case-')[1].split('.')[0])
            for y in range(64):
                for x in range(64):
                    if case == 0: expected = (255,0,0,255)
                    elif case == 1: expected = (0,255,0,255) if x>=32 else (0,0,0,255)
                    elif case == 2: expected = (0,0,0,255)
                    else:
                        g,b=(x+.5)/128,(y+.5)/128
                        expected=tuple(math.floor(v*255+.5) for v in (1-g-b,g,b,1))
                    for c,value in enumerate(expected):
                        if abs(p.stdout[(y*64+x)*4+c]-value)>(1 if case==3 else 0):
                            raise RuntimeError('Window independent GPU oracle mismatch: '+name)
        inventory = run('shell','run-as',package,'find','files/'+directory,'-type','f').stdout.splitlines()
        if set(inventory) != {'files/'+directory+'/'+n for n in names}:
            raise RuntimeError('Window directory has missing or unexpected readbacks')
        targets = probe.get('targets',[])
        identities=set()
        if type(targets) is not list or len(targets) != slots: raise RuntimeError('Window targets missing')
        for i,t in enumerate(targets):
            fields=('slot','pitch','inode','fd_bytes','pixel_offset','device_major','device_minor','fd_at_import','fd_index')
            if (type(t) is not dict or any(not integer(t.get(k)) for k in fields) or
                    not 0 < t['inode'] < (1 << 64) or not 0 <= t['fd_index'] < 8 or
                    not 0 <= t['fd_at_import'] < (1 << 31) or
                    any(not 0 <= t[k] < (1 << 32) for k in ('device_major','device_minor')) or
                    t.get('slot')!=i or t.get('pitch')!=256 or not t.get('inode') or
                    not 16384<=t.get('fd_bytes',0)<=1048576 or t['fd_bytes']%4096 or
                    t.get('pixel_offset',-1)<0 or t['pixel_offset']%64 or t['pixel_offset']>t['fd_bytes']-16384):
                raise RuntimeError('Window backing identity/span unqualified')
            identities.add((t['device_major'],t['device_minor'],t['inode']))
        if len(identities)!=slots:raise RuntimeError('Window targets alias backing')
        sessions.append(dict(generation=data['generation'], result=probe, images=images,
                             independent_channel_checks=slots*65536, unique_backings=len(identities)))
        save()

    try:
        previous = 0
        for generation in (1,2):
            first = wait(lambda d:d.get('ready') is True and d.get('generation',0)>previous, f'present-{generation}')
            present_state(first,generation)
            capture(first,f'present-{generation}')
            confirmed=wait(lambda d:True,f'confirm-{generation}')
            present_state(confirmed,generation)
            if any(confirmed[k] != first[k] for k in ('generation','directory','window_width','window_height','window_screen_x','window_screen_y')):
                raise RuntimeError('Window changed during screenshot capture')
            run('shell','input','keyevent','KEYCODE_HOME')
            final = wait(lambda d:d.get('completed_sessions')==generation and not d.get('worker_running') and
                         d.get('window_destroyed',0)>=generation,f'retired-{generation}')
            if (final['generation'] != first['generation'] or final['directory'] != first['directory'] or
                    final['phase'] != 'retired' or final['ready'] is not False or final['worker_running'] is not False or
                    final['held_final'] is not True or final['hold_released_by_pause'] is not True or
                    final['current_case_mask'] != (1 << (slots*4))-1 or final['current_frames'] != slots*4 or
                    final['frames'] != slots*4*generation or final['started_sessions'] != generation or
                    final['window_created'] != generation or final['window_destroyed'] != generation or
                    final['pauses'] < generation or final['passed'] is not (generation==2)):
                raise RuntimeError('Retired Window is not the captured held generation')
            collect(final,f'gen-{generation}')
            previous=first['generation']
            if generation==1:
                run('shell','am','start','-W','--activity-reorder-to-front','-n',package+'/'+evidence['build']['activity'])
        if (final.get('started_sessions')!=2 or final.get('completed_sessions')!=2 or
                final.get('window_created')!=2 or final.get('window_destroyed')!=2 or
                final.get('frames')!=slots*8 or final.get('passed') is not True):
            raise RuntimeError('NVN Window lifecycle did not balance exactly2 generations')
        evidence['screen_and_lifecycle_verified']=True
        evidence['window_verification']=dict(passed=True,generations=2,slots=slots,channel_checks=slots*131072,
            scope='Standard NVN Window/Acquire/Present and actual SurfaceView lifecycle; qualification shaders, no game frames')
    except Exception as error:
        evidence['lifecycle_error']=str(error)
        raise
    finally:
        save()
