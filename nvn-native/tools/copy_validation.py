"""Independent original-shader oracle and strict copy qualification boundary."""
import math
import json
import re
NAMES=tuple(f'copy-mode-{mode}-case-{case}.rgba' for mode in range(2) for case in range(4))
def parse_result(raw):
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result: raise ValueError('duplicate JSON field: '+key)
            result[key]=value
        return result
    value=json.loads(raw,object_pairs_hook=unique)
    if not isinstance(value,dict): raise ValueError('copy result must be an object')
    return value

def validate(data,images,nonce,check,expected_uid):
    if not isinstance(data,dict) or not isinstance(images,dict) or not isinstance(nonce,str) or not re.fullmatch('[0-9a-f]{32}',nonce):
        raise ValueError('invalid copy qualification namespace')
    if type(expected_uid) is not int or not 10000<=expected_uid<=0xffffffff: raise ValueError('expected an ordinary package UID')
    if data.get('uid')!=expected_uid: raise ValueError('copy result UID differs from package UID')
    exact={'schema':1,'operation':'nvn_gpu_copy','chip_id':0x06050002,'image_schema':3,
        'stage':'complete','errno':0,'cleanup_errno':0,'passed':True,'resources_retained':False,
        'completed_cases':8,'channel_checks':131072,'nvn_submissions_observed':8,
        'copy_submissions_observed':8,'copy_submissions':8,'copy_finishes':8,'intermediate_producer_finishes':4,
        'source_import_observations':1,'source_map_rejections':9,'source_private_vma_verified':True,
        'source_backing_replaced':False,'source_pool_cpu_no_access':True,'source_cpu_reads':0,
        'source_cpu_writes_after_import':0,'source_offset':4096,'source_pool_bytes':24576,
        'target_cpu_writes':0,'cpu_image_copy':False,'raw_draw_calls':0,'readback_locks':8,'readback_unlocks':8,
        'identification_read_locks':1,'identification_read_unlocks':1,'owner_pin_calls':8,'owner_unpin_calls':8,
        'copy_command_readonly_checks':786592,'copy_command_guard_checks':32608,
        'source_guard_gpu_checked':False,'padded_stride_gpu_checked':False,'window_presented':False,
        'game_modules_executed':False,'bad_mode':0xffffffff,'bad_case':0xffffffff,'bad_x':0xffffffff,'bad_y':0xffffffff,'bad_channel':0xffffffff,'bad_actual':0,'bad_expected':0,'run_nonce':nonce,'run_directory':'copy-'+nonce}
    for k,v in exact.items():
        if type(data.get(k)) is not type(v) or data[k]!=v: raise ValueError('copy result mismatch: '+k)
    for k in ('uid','source_gpu_address','input_readonly_checks'):
        if type(data.get(k)) is not int or data[k]<=0: raise ValueError('invalid '+k)
    if data['uid']<10000 or data['source_gpu_address']%64 or data['source_gpu_address']+16384>1<<64: raise ValueError('copy UID/GPUVA mismatch')
    size=lambda stage:check['inputs'][stage]['envelope']['inputs']['code']['size']
    align=lambda n:(n+4095)&~4095
    shader_bytes=align(align(4096+size('vs'))+4096+size('fs'))+4096
    if data['input_readonly_checks']!=8*(shader_bytes+3*4096): raise ValueError('input guard/read-only count mismatch')
    target=data.get('target')
    if not isinstance(target,dict) or set(target)!={'fd_at_import','fd_index','fd_bytes','pixel_offset','inode','device_major','device_minor','pitch'} or any(type(target.get(k)) is not int for k in ('fd_at_import','fd_index','fd_bytes','pixel_offset','inode','device_major','device_minor','pitch')):
        raise ValueError('invalid AHB backing metadata')
    if (not 0<=target['fd_at_import']<=0x7fffffff or not 0<=target['fd_index']<8 or not 0<target['inode']<=0xffffffffffffffff or not 0<=target['device_major']<=0xffffffff or not 0<=target['device_minor']<=0xffffffff or target['pitch']!=256 or target['pixel_offset']<0 or target['pixel_offset']%64 or target['fd_bytes']%4096 or target['fd_bytes']>1048576 or target['fd_bytes']<target['pixel_offset']+16384):
        raise ValueError('invalid identified AHB range')
    if set(images)!=set(NAMES): raise ValueError('missing/unexpected copy readbacks')
    channels=0
    for mode in range(2):
        for case in range(4):
            raw=images[f'copy-mode-{mode}-case-{case}.rgba']
            if not isinstance(raw,bytes) or len(raw)!=16384:raise ValueError('wrong RGBA size')
            for y in range(64):
                for x in range(64):
                    pixel=(255,0,0,255)
                    if case==1:pixel=(0,255,0,255) if x>=32 else (0,0,0,255)
                    if case==2:pixel=(0,0,0,255)
                    if case==3:
                        g=(x+.5)/128;b=(y+.5)/128
                        pixel=tuple(math.floor(v*255+.5) for v in (1-g-b,g,b))+(255,)
                    for c,expected in enumerate(pixel):
                        if abs(raw[(y*64+x)*4+c]-expected)>(1 if case==3 else 0):
                            raise ValueError(f'copy oracle mismatch mode{mode} case{case} ({x},{y},{c})')
                        channels+=1
    return {'passed':True,'channel_checks':channels,'files':8,'source':'independent original VS/FS analytic oracle','window_presented':False,'game_frame':False}
