"""Thin native BUFFER TXF/TXS profile; original ordinaryUID/nonce/collector/strict-stop."""
import public_copy_runner as shared
from buffer_texture_validation import validate_catalogue
stopped=shared.stopped
def profile(record):
 c=record.get('buffer_texture_check')
 if not isinstance(c,dict)or c.get('schema')!=1 or c.get('kind')!='native-buffer-texture-qualification-v1'or c.get('gpu_execution')is not False or c.get('original_game_shader')is not False:raise RuntimeError('invalid checked native BUFFER texture build')
 validate_catalogue(c.get('cases'),c.get('target'))
 for k,v in record.items():
  if k.endswith('_check')and k!='buffer_texture_check'and v:raise RuntimeError('mixed native BUFFER texture mode '+k)
 for k in ('nvn_api','compute_indirect','device_inspect','iocoherent_inspect','ahb_inspect','d2r_host','graphics_draw_batch'):
  if record.get(k):raise RuntimeError('mixed native BUFFER texture operation '+k)
 if type(record.get('texture_descriptor_flags',0))is not int or record.get('texture_descriptor_flags',0)!=0:raise RuntimeError('descriptor subtype in raw BUFFER qualification')
 for k in ('src/pipeline.c','src/pipeline_program.c','src/texture_buffer_sample.c','src/order.c','src/kgsl.c','tests/buffer_texture_probe.c','tests/buffer_texture_cases.h','android/buffer_texture_activity.c','android/public_copy_activity.c'):
  if k not in record.get('source',{}):raise RuntimeError('missing actual BUFFER source '+k)
def exercise(args,build,record,adb,package,component,run):
 return shared.exercise(args,build,record,adb,package,component,run,buffer_texture=True)
