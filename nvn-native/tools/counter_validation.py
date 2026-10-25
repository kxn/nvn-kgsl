"""Independent Counter values/original color-shader oracle; no GPU simulator."""
import json
import re
NAMES=('counter-replay-0.rgba','counter-replay-1.rgba','counter-cross-0.rgba','counter-cross-1.rgba',
       'counter-reset-zero.rgba','counter-after-reset.rgba','counter-other-context.rgba',
       'counter-primary-gap.rgba','counter-overlap-primary.rgba','counter-overlap-other.rgba')
OBS=('replay0','replay1','cross0','cross1','reset_zero','after_reset','other_context','primary_gap','primary_after_gap',
     'overlap_primary_top','overlap_primary_bottom','overlap_primary_samples','overlap_other_top','overlap_other_bottom','overlap_other_samples')
EXPECTED=(4096,4096,8192,12288,0,4096,12288,0,4096,None,None,4096*4096,None,None,8*4096)
def parse_result(raw):
    def unique(pairs):
        out={}
        for k,v in pairs:
            if k in out:raise ValueError('duplicate result key '+k)
            out[k]=v
        return out
    out=json.loads(raw,object_pairs_hook=unique)
    if not isinstance(out,dict):raise ValueError('expected Counter result object')
    return out

def validate(data,images,nonce,expected_uid):
    if not isinstance(data,dict) or not isinstance(images,dict) or type(expected_uid) is not int or expected_uid<10000:
        raise ValueError('invalid Counter evidence types/ordinary UID')
    if not isinstance(nonce,str) or re.fullmatch('[0-9a-f]{32}',nonce) is None:raise ValueError('invalid fresh namespace')
    exact={'schema':1,'operation':'nvn_counter','uid':expected_uid,'chip_id':0x06050002,'image_schema':3,
        'stage':'complete','errno':0,'cleanup_errno':0,'passed':True,'resources_retained':False,
        'submissions':12,'finishes':12,'handles_recorded':8,'handles_released':8,'readbacks':10,
        'channel_checks':163840,'sample_checks':11,'clock_checks':4,'conversion_checks':15,
        'queues_created':2,'context_overlap_observed':True,'between_context_checks':1,
        'primary_stress_draws':4096,'secondary_stress_draws':8,'cpu_counter_estimates':0,
        'cpu_target_writes_after_import':0,'game_modules_executed':False,'window_presented':False,
        'run_nonce':nonce,'run_directory':'counter-'+nonce}
    for k,v in exact.items():
        if type(data.get(k)) is not type(v) or data[k]!=v:raise ValueError('Counter result mismatch '+k)
    for k in ('guard_checks','readonly_checks'):
        if type(data.get(k)) is not int or data[k]<=0:raise ValueError('missing strict '+k)
    observations=data.get('observations')
    if not isinstance(observations,list) or len(observations)!=15:raise ValueError('missing actual Counter output')
    for i,v in enumerate(observations):
        if not isinstance(v,dict) or set(v)!={'name','observed','counter','timestamp','nanoseconds'} or v['name']!=OBS[i] or v['observed'] is not True:
            raise ValueError('invalid ordered Counter result metadata')
        if any(type(v[k]) is not int or not 0<=v[k]<1<<64 for k in ('counter','timestamp','nanoseconds')):
            raise ValueError('invalid uint64 Counter result')
        if not v['timestamp'] or v['nanoseconds']!=v['timestamp']*625//12:
            raise ValueError('actual timestamp/conversion mismatch')
        expected=EXPECTED[i]
        if v['counter']!=(v['timestamp'] if expected is None else expected):
            raise ValueError('actual GPU count mismatch '+OBS[i])
    t=lambda i:observations[i]['timestamp']
    if any(t(i)>=t(i+1) for i in range(8)):
        raise ValueError('sequential GPU report timestamps not increasing')
    if not (t(9)<t(12)<t(13)<t(10)<=t(11) and t(13)<=t(14)):
        raise ValueError('secondary context GPU execution was not inside primary query interval')
    if set(images)!=set(NAMES):raise ValueError('missing/unexpected Counter readback files')
    for i,name in enumerate(NAMES):
        raw=images[name]
        if not isinstance(raw,bytes) or len(raw)!=16384:raise ValueError('wrong target readback size')
        expected=bytes((0,255,0,255) if i in (6,9) else (255,0,0,255))*4096
        if raw!=expected:raise ValueError('original VS/FS target oracle mismatch '+name)
    return {'passed':True,'actual_sample_reports':11,'actual_timestamp_reports':4,'conversion_checks':15,
        'files':10,'channel_checks':163840,'second_context_inside_query_observed':True,
        'scope':'This actual tested two-context execution only; not general firmware/system-context isolation proof',
        'game_frame':False,'window_presented':False}
