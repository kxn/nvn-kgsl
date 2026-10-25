"""Existing ordinary-UID Activity/nonce/fullraw/strictstop transport, no new timer."""
from front_facing_validation import NAMES,validate
def profile(record):
 import numpy
 c=record.get('native_front_facing_check')
 if type(c)is not dict or c.get('kind')!='raw-native-front-facing-qualification-v1'or c.get('submissions')!=640 or c.get('gpu_execution')is not False:raise RuntimeError('normal FACE qualification required')
 if any(record.get(k)for k in('d2r_host','nvn_api','native_depth_check','native_depth_sampled_check','native_pipeline_check','nvn_pipeline_check')):raise RuntimeError('independent authored FACE operation')
 for name in('tests/front_facing_probe.c','tests/pipeline_probe.c','src/pipeline.c','src/render_state.c','src/depth_transfer.c','android/pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing actual FACE source '+name)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification={'names':NAMES,'validate':validate,'check_key':'native_front_facing_check','prefix':'pipeline-','nonce_file':'pipeline-run-nonce','profile':profile,'scope':'ordinary Android authored native halfFACE/FragCoord/FragDepth; full winding/frontface/origin/cull/direct/indexed/sameIB replay; not public NVN/game or original12FS'})
