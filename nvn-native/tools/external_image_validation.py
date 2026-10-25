"""Independent metadata and pixel checks for recorded NVN-to-AHB qualifiers."""
import math


def names(slots):
    if type(slots) is not int or slots not in (2, 3):
        raise ValueError('external image qualifier requires exactly2 or3 targets')
    return tuple(f'external-slot-{slot}-case-{case}.rgba' for slot in range(slots) for case in range(4))


def validate(data, images, slots, nonce):
    expected_names = names(slots)
    required = {'schema': 1, 'slots': slots, 'image_schema': 3, 'chip_id': 0x06050002,
                'errno': 0, 'cleanup_errno': 0, 'nvn_submissions': slots * 4,
                'completed_cases': slots * 4, 'channel_checks': slots * 65536,
                'readback_locks': slots * 4, 'readback_unlocks': slots * 4,
                'mapping_identification_locks': slots, 'negative_checks': slots * 11,
                'external_pools_imported': slots, 'external_pools_released': slots,
                'target_cpu_writes': 0, 'raw_draw_calls': 0}
    if (data.get('operation') != 'nvn_external_image' or data.get('submission') != 'accepted' or
            data.get('run_nonce') != nonce or data.get('owner_thread') != 'worker' or
            any(type(data.get(k)) is not int or data[k] != v for k, v in required.items()) or
            data.get('passed') is not True or
            any(data.get(k) is not False for k in ('resources_retained', 'cpu_image_copy',
                                                  'window_presented', 'game_modules_executed')) or
            any(type(data.get(k)) is not int or data[k] <= 0 for k in
                ('readonly_byte_checks', 'guard_byte_checks', 'guarded_object_checks'))):
        raise ValueError('external target counters, ownership, thread or run identity mismatch')
    targets = data.get('targets')
    if not isinstance(targets, list) or len(targets) != slots:
        raise ValueError('missing actual external target identities')
    identities = set()
    for slot, target in enumerate(targets):
        keys = ('slot', 'fd_at_import', 'fd_index', 'fd_bytes', 'pixel_offset', 'inode',
                'device_major', 'device_minor', 'pitch')
        if (not isinstance(target, dict) or any(type(target.get(k)) is not int or target[k] < 0 for k in keys) or
                target['slot'] != slot or target['pitch'] != 256 or not target['inode'] or
                not 16384 <= target['fd_bytes'] <= 1048576 or target['fd_bytes'] % 4096 or
                target['pixel_offset'] % 64 or target['pixel_offset'] > target['fd_bytes'] - 16384):
            raise ValueError('invalid actual dma-buf identity or pixel span')
        identity = (target['device_major'], target['device_minor'], target['inode'])
        if identity in identities:
            raise ValueError('external targets alias the same backing')
        identities.add(identity)
    if set(images) != set(expected_names) or any(len(pixels) != 16384 for pixels in images.values()):
        raise ValueError('missing or malformed fresh external target readbacks')
    checks = 0
    for slot in range(slots):
        for case in range(4):
            pixels = images[f'external-slot-{slot}-case-{case}.rgba']
            for y in range(64):
                for x in range(64):
                    if case == 0:
                        expected = (255, 0, 0, 255)
                    elif case == 1:
                        expected = (0, 255, 0, 255) if x >= 32 else (0, 0, 0, 255)
                    elif case == 2:
                        expected = (0, 0, 0, 255)
                    else:
                        green, blue = (x + .5) / 128, (y + .5) / 128
                        expected = tuple(math.floor(v * 255 + .5) for v in (1 - green - blue, green, blue, 1))
                    offset = (y * 64 + x) * 4
                    for channel, value in enumerate(expected):
                        if abs(pixels[offset + channel] - value) > (1 if case == 3 else 0):
                            raise ValueError(f'external GPU pixel mismatch: slot{slot} case{case} ({x},{y}) channel{channel}')
                        checks += 1
    return {'passed': True, 'images': len(images), 'channel_checks': checks,
            'unique_target_backings': len(identities), 'original_shader_oracle': True,
            'scope': 'NVN offscreen direct AHB images, not Window PFNs, presentation or game frames'}
