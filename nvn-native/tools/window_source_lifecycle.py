"""NVN canonical GOB Window qualifier using the existing lifecycle collector."""
from pathlib import Path
import importlib.util,json
spec=importlib.util.spec_from_file_location('nn_window_source_lifecycle_common',Path(__file__).with_name('fullsize_display_lifecycle.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
common.OP='nvn_window_source';common.PREFIX='window-source';common.SLOTS_KEY='window_source_slots'
common.SESSIONS_KEY='window_source_sessions';common.VERIFIED_KEY='window_source_verification'
common.SCOPE='Standard NVN Window canonical GOB source -> production GPU conversion -> independent AHB/SC; no Draw/game frame'

def validate_probe(probe,data,slots):
 requested_interval=_interval_request
 W,H=common.W,common.H
 want=dict(schema=1,operation=common.OP,width=W,height=H,slots=slots,uid=data['uid'],chip_id=0x06050002,errno=0,cleanup_errno=0,
  completed_cases=slots*4,window_presentations=slots*4,presented_callbacks=slots*4,queue_acquires=slots*4,canonical_pools_released=slots,
  channel_checks=slots*4*W*H*4,source_pattern_byte_writes=slots*4*W*H*4,padding_baseline_locks=slots,readback_locks=slots*4,readback_unlocks=slots*5,current_lock_rejections=1,target_cpu_writes=0,nvn_draw_calls=0,raw_draw_calls=0)
 if type(probe) is not dict or any(type(probe.get(k)) is not type(v) or probe[k]!=v for k,v in want.items()):raise RuntimeError('NVN source Window producer count/identity contract failed')
 if any(probe.get(k) is not True for k in ('window_finalized','session_retired','canonical_address_unchanged','same_ib_replay','passed')) or any(probe.get(k) is not False for k in ('resources_retained','game_modules_executed','cpu_image_copy')):raise RuntimeError('NVN source Window did not prove canonical ownership/retirement')
 if not common.integer(probe.get('readonly_byte_checks'),slots*4*(19169280+8192)):raise RuntimeError('source and pool guard read-only proof missing')
 sc=probe.get('sc_before_retire');wanted=dict(generation=data['generation'],shown=slots*4,releases=slots*4-1,current_index=data['current_slot'],first_error=0)
 if type(sc) is not dict or any(type(sc.get(k)) is not int or sc[k]!=v for k,v in wanted.items()):raise RuntimeError('SC final generation/counters mismatch')
 targets=probe.get('targets');identities=set();source_addresses=set();guards=0
 if type(targets) is not list or len(targets)!=slots:raise RuntimeError('target inventory missing')
 for i,t in enumerate(targets):
  fields=('slot','fd_index','fd_at_import','fd_bytes','pixel_offset','payload_bytes','pitch','inode','device_major','device_minor','source_address','source_offset','source_extent')
  if type(t) is not dict or any(not common.integer(t.get(k)) for k in fields):raise RuntimeError('target metadata invalid type')
  extent=(t['pitch']*((H+3)//4*4)+4095)//4096*4096
  if t['slot']!=i or not 0<=t['fd_index']<8 or not 0<=t['fd_at_import']<(1<<31) or not 0<t['inode']<(1<<64) or any(t[k]>=(1<<32) for k in ('device_major','device_minor')) or t['pitch']<W*4 or t['pitch']>1048576 or t['pitch']%64 or t['payload_bytes']!=extent or t['fd_bytes']%4096 or t['pixel_offset']%64 or t['pixel_offset']>t['fd_bytes'] or extent>t['fd_bytes']-t['pixel_offset'] or not t['source_address'] or t['source_address']%64 or t['source_offset']!=4096 or t['source_extent']!=19169280:raise RuntimeError('target/canonical extent or identity unqualified')
  if i and t['pitch']!=targets[0]['pitch']:raise RuntimeError('mixed geometry')
  identities.add((t['device_major'],t['device_minor'],t['inode']));source_addresses.add(t['source_address']);guards+=extent-W*H*4
 if len(identities)!=slots or len(source_addresses)!=slots or type(probe.get('guard_byte_checks')) is not int or probe['guard_byte_checks']!=4*guards or guards<=0:raise RuntimeError('target alias/padding/canonical independence mismatch')
 from window_interval_runtime import validate
 validate(probe,requested_interval,slots)
 return slots*4*W*H*4
common.validate_probe=validate_probe
parse=common.parse
check_image=common.check_image
_interval_request=None
def exercise(run,adb,package,evidence,destination):
 global _interval_request
 # This module owns one serial collector call; the batch creates fresh processes.
 _interval_request=evidence.get('window_source_present_interval')
 common.exercise(run,adb,package,evidence,destination)
 from window_interval_runtime import validate
 evidence['window_interval_verification']=[validate(s['result'],_interval_request,evidence['build'][common.SLOTS_KEY]) for s in evidence[common.SESSIONS_KEY]]
 Path(destination).write_text(json.dumps(evidence,indent=2)+'\n')
