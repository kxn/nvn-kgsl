"""Existing pipeline Activity/nonce/fullraw/strictstop transport; authored native TG4 only."""
from gather_probe_validation import NAMES,validate

def profile(record):
 import numpy
 c=record.get('native_gather_check')
 if type(c)is not dict or c.get('kind')!='raw-native-gather-qualification-v1'or c.get('submissions')!=96 or c.get('gpu_execution')is not False:raise RuntimeError('normal Gather qualifier required')
 if any(record.get(k)for k in('nvn_api','d2r_host','native_pipeline_check','native_color_target_check','nvn_pipeline_check','public_copy_check','public_volume_copy_check','public_copy_call','clear_color_check','buffer_texture_check','volume_texture_check')):raise RuntimeError('Gather must be independent')
 for name in('tests/gather_probe.c','tests/gather_probe_cases.h','tests/pipeline_probe.c','src/pipeline.c','src/pipeline_program.c','android/pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing Gather source closure '+name)

def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification={'names':NAMES,'validate':validate,'check_key':'native_gather_check','prefix':'pipeline-','nonce_file':'pipeline-run-nonce','profile':profile,'scope':'ordinary Android authored raw-native TG4 component/wrap/sparsebinding/replay; not public NVN/Maxwell/game qualification'})
