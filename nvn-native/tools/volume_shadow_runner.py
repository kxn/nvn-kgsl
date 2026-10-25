"""Thin true3D owner profile of existing collector/nonce/strict-stop transport."""
import public_copy_runner as shared
from volume_shadow_validation import validate_cases
stopped=shared.stopped

def profile(record):
 c=record.get('volume_shadow_check')
 if not isinstance(c,dict)or c.get('schema')!=1 or c.get('kind')!='native-volume-shadow-qualification-v1'or c.get('gpu_execution')is not False:raise RuntimeError('invalid checked volume-shadow build')
 validate_cases(c.get('cases'))
 for key,value in record.items():
  if key.endswith('_check')and key!='volume_shadow_check'and value:raise RuntimeError('mixed volume-shadow qualifier '+key)
 for key in ('nvn_api','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','d2r_host','graphics_draw_batch'):
  if record.get(key):raise RuntimeError('mixed volume-shadow mode '+key)
 if type(record.get('texture_descriptor_flags',0))is not int or record.get('texture_descriptor_flags',0)!=0:raise RuntimeError('descriptor subtype in volume-shadow')
 for key in ('src/texture_volume_shadow.c','include/nvn_native/texture_volume_shadow.h','src/texture_source.c','src/volume_copy.inc','src/compute.c','src/kgsl.c','tests/volume_shadow_probe.c','tests/volume_shadow_cases.h','android/volume_shadow_activity.c','android/public_copy_activity.c'):
  if key not in record.get('source',{}):raise RuntimeError('volume shadow lacks actual source '+key)

def exercise(args,build,record,adb,package,component,run):
 return shared.exercise(args,build,record,adb,package,component,run,shadow=True)
