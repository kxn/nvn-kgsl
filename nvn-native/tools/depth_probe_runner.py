"""Reuse existing ordinary-UID pipeline Activity/nonce/strictstop transport."""
from depth_probe_validation import NAMES,validate
def profile(record):
 import numpy
 c=record.get('native_depth_check')
 if type(c)is not dict or c.get('kind')!='raw-native-depth-qualification-v2'or c.get('submissions')!=432 or c.get('gpu_execution')is not False:raise RuntimeError('normal native depth qualifier required')
 if any(record.get(k)for k in ('native_pipeline_check','native_gather_check','native_color_target_check','nvn_pipeline_check','d2r_host','nvn_api')):raise RuntimeError('independent depth operation required')
 for name in ['tests/depth_probe.c','tests/pipeline_probe.c','src/depth_transfer.c','src/depth_attachment.c','src/pipeline.c','src/pipeline_program.c','android/pipeline_probe_activity.c']:
  if name not in record['source']:raise RuntimeError('missing depth source closure '+name)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification={'names':NAMES,'validate':validate,'check_key':'native_depth_check','prefix':'pipeline-','nonce_file':'pipeline-run-nonce','profile':profile,'scope':'ordinary Android authored raw-native D16/D24X8/D32F tiled clear/write/compare/disable/scissor/origin/replay and GPU CP_BLIT readback; not public NVN/game'})
