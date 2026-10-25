"""ADB screenshot/lifecycle qualification; no custom timing or profiling."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import time


def exercise(run, adb, package, evidence, destination):
    from PIL import Image

    destination = Path(destination)
    history = evidence.setdefault('lifecycle', [])
    screenshots = evidence.setdefault('screenshots', [])

    def preserve():
        destination.write_text(json.dumps(evidence, indent=2) + '\n')

    def wait(predicate, description):
        deadline = time.monotonic() + 12
        while True:
            result = run('shell', 'run-as', package, 'cat', 'files/result.json', check=False)
            if result.returncode == 0:
                try:
                    data = json.loads(result.stdout)
                except json.JSONDecodeError:
                    data = None
                if data:
                    evidence['result'] = data
                    if data.get('errno') or data.get('resources_retained'):
                        history.append({'stage': description, 'result': data})
                        preserve()
                        raise RuntimeError('window/GPU/compositor qualification failed')
                    if predicate(data):
                        history.append({'stage': description, 'result': data})
                        preserve()
                        return data
            if time.monotonic() >= deadline:
                preserve()
                raise RuntimeError('lifecycle timeout: ' + description)
            time.sleep(0.1)

    def capture(data, label):
        raw = subprocess.run(adb + ['exec-out', 'run-as', package, 'cat', 'files/ahb-case-3.rgba'],
                             check=True, capture_output=True, timeout=20).stdout
        if len(raw) != 16384:
            raise RuntimeError('missing qualified GPU RGB frame')
        source = Image.frombytes('RGBA', (64, 64), raw)
        raw_path = destination.with_name(destination.stem + '-' + label + '.rgba')
        raw_path.write_bytes(raw)
        width, height = data['window_width'], data['window_height']
        left, top = data['window_screen_x'], data['window_screen_y']
        for attempt in range(3):
            png = subprocess.run(adb + ['exec-out', 'screencap', '-p'], check=True,
                                 capture_output=True, timeout=20).stdout
            screen = Image.open(io.BytesIO(png)).convert('RGB')
            path = destination.with_name(destination.stem + '-' + label + f'-{attempt}.png')
            path.write_bytes(png)
            checks = []
            if (width > 0 and height > 0 and left >= 0 and top >= 0 and
                    left + width <= screen.width and top + height <= screen.height):
                # SurfaceView reports its actual screen position, including
                # vendor window captions/insets; child starts at parent(0,0).
                # Interior samples tolerate one source texel of compositor
                # filtering/scaling. The independent raw GPU oracle stays exact.
                for sy in (8, 24, 40, 56):
                    for sx in (8, 24, 40, 56):
                        x = left + int((sx + 0.5) * width / 64)
                        y = top + int((sy + 0.5) * height / 64)
                        expected = source.getpixel((sx, sy))[:3]
                        actual = screen.getpixel((x, y))
                        delta = max(abs(a - b) for a, b in zip(actual, expected))
                        checks.append({'screen_xy': [x, y], 'source_xy': [sx, sy],
                                       'expected': expected, 'actual': actual, 'maximum_delta': delta})
            valid = len(checks) == 16 and all(c['maximum_delta'] <= 8 for c in checks)
            screenshots.append({'stage': label, 'attempt': attempt, 'path': str(path.resolve()),
                                'sha256': hashlib.sha256(png).hexdigest(),
                                'screen_size': list(screen.size), 'window_size': [width, height],
                                'window_origin': [left, top],
                                'gpu_rgba_path': str(raw_path.resolve()),
                                'gpu_rgba_sha256': hashlib.sha256(raw).hexdigest(),
                                'channel_tolerance': 8, 'sample_checks': checks, 'passed': valid})
            preserve()
            if valid:
                return
            time.sleep(0.15)
        raise RuntimeError('actual screenshot does not match the GPU-rendered frame')

    try:
        first = wait(lambda d: d.get('ready') is True, 'first-present')
        capture(first, 'first-present')
        run('shell', 'input', 'keyevent', 'KEYCODE_HOME')
        retired = wait(lambda d: d.get('window_destroyed', 0) >= 1 and
                       not d.get('worker_running') and d.get('buffers_released') == 1,
                       'first-window-destroyed')
        run('shell', 'am', 'start', '-W', '--activity-reorder-to-front', '-n',
            package + '/' + evidence['build'].get('activity', 'android.app.NativeActivity'))
        second = wait(lambda d: d.get('ready') is True and d.get('generation', 0) > first['generation'],
                      'recreated-window-present')
        if second.get('window_created', 0) <= retired['window_created']:
            raise RuntimeError('resume did not recreate a native window')
        capture(second, 'recreated-window-present')
        run('shell', 'input', 'keyevent', 'KEYCODE_HOME')
        final = wait(lambda d: d.get('window_destroyed', 0) >= 2 and not d.get('worker_running') and
                     d.get('buffers_released') == 2, 'second-window-destroyed')
        required = {'frames': 2, 'completed_sessions': 2, 'buffers_released': 2,
                    'show_completed': 2, 'detach_completed': 2,
                    'window_created': 2, 'window_destroyed': 2,
                    'window_acquired': 2, 'window_released': 2}
        if (any(final.get(k) != v for k, v in required.items()) or
                final.get('completion_callbacks', 0) < 4 or final.get('resumes', 0) < 2 or
                final.get('pauses', 0) < 2 or final.get('cpu_image_copy') is not False or
                final.get('passed') is not True):
            raise RuntimeError('window lifecycle did not balance native/GPU/compositor ownership')
        gpu = final.get('gpu', {})
        if (gpu.get('operation') != 'kgsl_ahb_present_frame' or gpu.get('passed') is not True or
                gpu.get('buffer_exported') is not True or gpu.get('buffer_released') is not False or
                gpu.get('channel_checks') != 65536 or gpu.get('cp_payload_byte_checks') != 16384 or
                gpu.get('draw_submissions') != 4 or gpu.get('cleanup_errno') or gpu.get('resources_retained')):
            raise RuntimeError('display did not consume a qualified original-shader AHB')
        evidence['screen_and_lifecycle_verified'] = True
        evidence['result'] = final
        return final
    except Exception as error:
        evidence['lifecycle_error'] = str(error)
        raise
    finally:
        preserve()
