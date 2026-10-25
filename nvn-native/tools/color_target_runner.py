"""Reuse existing pipeline Activity/nonce/collection/strictstop; raw COLOR only."""
from color_target_validation import NAMES,validate

def profile(record):
 import numpy
 check=record.get('native_color_target_check')
 if type(check)is not dict or check.get('kind')!='raw-native-color-target-qualification-v1'or check.get('submissions')!=132 or check.get('gpu_execution')is not False:raise RuntimeError('normal COLOR qualification required')
 if record.get('native_pipeline_check')is not None or any(record.get(k)for k in('nvn_api','d2r_host','nvn_pipeline_check','buffer_texture_check','volume_texture_check','public_copy_check','public_volume_copy_check','public_copy_call')):raise RuntimeError('COLOR qualifier must be independent')
 for name in('src/color_target.c','src/pipeline.c','tests/pipeline_probe.c','tests/color_target_probe.inc','tests/color_target_cases.h','android/pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing COLOR source closure '+name)

def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification={'names':NAMES,'validate':validate,'check_key':'native_color_target_check','prefix':'pipeline-','nonce_file':'pipeline-run-nonce','profile':profile,'scope':'ordinary Android raw-native22 COLOR RT original bounded color VS/FS; not public NVN/GOB/game qualification'})
