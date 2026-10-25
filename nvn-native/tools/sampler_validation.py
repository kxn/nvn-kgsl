"""Independent normalized RGBA8 sampling equations for the unchanged original FS."""
import json
import math
import re
from functools import lru_cache
NAMES=tuple(f'sampler-{n:02d}.rgba' for n in range(38))
TEXELS=((64,128,192,255),(192,64,128,255),(128,192,64,255),(96,160,224,255))
def parse_result(text):
    def pairs(items):
        result={}
        for k,v in items:
            if k in result:raise RuntimeError('duplicate JSON key: '+k)
            result[k]=v
        return result
    try:
        value=json.loads(text,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
    except (ValueError,TypeError) as e:raise RuntimeError('invalid sampler JSON') from e
    if type(value) is not dict:raise RuntimeError('sampler result is not an object')
    return value

def state(n):
    if type(n) is not int or not 0<=n<38:raise ValueError('case')
    if n==36:n=0
    if n==37:n=17
    s={'min':0,'mag':0,'s':7,'t':7,'offset':-1.5,'span':4,'constant':False,'minifying':False}
    if n<18:s.update(min=n//9,mag=n//9,s=(1,6,7)[n%9//3],t=(1,6,7)[n%3])
    elif n<22:s.update(min=0 if n<20 else 1,mag=1 if n<20 else 0,minifying=bool(n&1),s=1,t=6)
    elif n<28:s.update(min=n-22,mag=1,minifying=True,s=1,t=6)
    elif n<32:s.update(constant=True,u=.75 if n&1 else .25,v=.75 if n&2 else .25)
    else:s.update(min=1,mag=1,constant=True,u=.625 if n&1 else .375,v=.625 if n&2 else .375)
    if s['minifying']:s.update(offset=-.25,span=48)
    return s

def tap(i,wrap):
    if wrap==7:return min(1,max(0,i))
    if wrap==1:return i%2
    j=i%4;return j if j<2 else 3-j

def pixel(n,x,y):
    s=state(n)
    u=s['u'] if s['constant'] else s['offset']+s['span']*(x+.5)/64
    v=s['v'] if s['constant'] else s['offset']+s['span']*(y+.5)/64
    linear=(s['min']&1) if s['minifying'] else s['mag']
    a,b=u*2-(.5 if linear else 0),v*2-(.5 if linear else 0)
    ix,iy=math.floor(a),math.floor(b);fx,fy=(a-ix,b-iy) if linear else (0,0)
    sample=[sum((fx if i else 1-fx)*(fy if j else 1-fy)*TEXELS[tap(iy+j,s['t'])*2+tap(ix+i,s['s'])][c]
                for j in range(2) for i in range(2)) for c in range(4)]
    # Original FS: color=1, gamma=contrast=1, mix=0 -> sample; alpha255
    # avoids KIL. Tolerance applies only to the actual target RGB quantization.
    return tuple(min(255,max(0,math.floor(v+.5))) for v in sample)

@lru_cache(maxsize=38)
def expected(n):return bytes(c for y in range(64) for x in range(64) for c in pixel(n,x,y))

def validate(data,captured,nonce,uid):
    if type(data) is not dict or type(uid) is not int or uid<10000:raise RuntimeError('invalid sampler types/ordinary UID')
    if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('invalid run nonce')
    required={'schema':1,'uid':uid,'chip_id':0x06050002,'gmem_bytes':1179648,'api_major':53,'api_minor':105,
        'errno':0,'cleanup_errno':0,'completed_cases':38,'submissions':38,'finishes':38,'recorded_handles':2,
        'channel_checks':38*16384,'shaded_pixels':38*4096,'outside_pixels':0,'discard_pixels':0,
        'binding_rejections':3,'binding_array_copies':4,'texture_object_finalizations':3,
        'sampler_object_finalizations':39,'texture_registrations':4,'sampler_registrations':40,'inactive_border_checks':76,
        'backing_pool_busy_rejections':3,'descriptor_pool_busy_rejections':4,'program_busy_rejections':38,
        'width':64,'height':64,'pitch':256,'target_bytes':16384}
    for k,v in required.items():
        if type(data.get(k)) is not int or data[k]!=v:raise RuntimeError('sampler metadata mismatch: '+k)
    if data.get('operation')!='nvn_sampler' or data.get('stage')!='complete' or data.get('submission')!='accepted':raise RuntimeError('sampler operation/stage/submit mismatch')
    if data.get('run_nonce')!=nonce or data.get('run_directory')!='sampler-'+nonce:raise RuntimeError('stale sampler nonce/directory')
    for k,v in {'passed':True,'resources_retained':False,'device_matches_artifact':True,'reused_ib':True}.items():
        if type(data.get(k)) is not bool or data[k] is not v:raise RuntimeError('sampler lifecycle mismatch: '+k)
    for k in ('guard_byte_checks','readonly_byte_checks'):
        if type(data.get(k)) is not int or data[k]<=0:raise RuntimeError('missing sampler guard/read-only checks')
    if type(captured) is not dict or set(captured)!=set(NAMES):raise RuntimeError('sampler exact readback inventory mismatch')
    checks=0;maximum=0
    for n,name in enumerate(NAMES):
        raw=captured[name]
        if not isinstance(raw,bytes) or len(raw)!=16384:raise RuntimeError('sampler readback size/type: '+name)
        want=expected(n)
        for i,(actual,reference) in enumerate(zip(raw,want)):
            delta=abs(actual-reference);maximum=max(maximum,delta)
            if delta>(0 if i%4==3 else 1):raise RuntimeError(f'{name} byte{i}: actual{actual} expected{reference}')
            checks+=1
    for a,b in ((0,36),(17,37),(22,24),(22,26),(23,25),(23,27)):
        if captured[NAMES[a]]!=captured[NAMES[b]]:raise RuntimeError(f'exact sampler replay/single-mip mismatch: {a}/{b}')
    if len({captured[n] for n in NAMES[:18]})!=18:raise RuntimeError('base sampler filter/wrap states were not observably distinct')
    return {'cases':38,'base_filter_wrap_states':18,'channel_checks':checks,'max_channel_error':maximum,
            'exact_replay_pairs':2,'exact_single_mip_pairs':4,'mixed_min_mag_cases':4,'texel_center_cases':4,
            'interpolation_cases':4,'rgb_tolerance':1,'alpha_tolerance':0,'aniso_filter_quality_qualified':False,
            'scope':'A650 original2x2 single-mip normalized sampling; no game/fullsize/aniso quality claim'}
