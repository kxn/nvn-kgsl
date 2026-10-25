"""Existing fresh nonce Activity/full raw/ordinary UID/strict stop transport."""
from graphics_storage_probe_validation import NAMES,validate
def profile(record):
 c=record.get('native_graphics_storage_check')
 if type(c)is not dict or c.get('kind')!='raw-native-graphics-storage-qualification-v1'or c.get('submissions')!=192 or c.get('gpu_execution')is not False:raise RuntimeError('normal full authored storage qualifier required')
 if any(record.get(k)for k in('nvn_api','d2r_host','native_pipeline_check','native_gather_check','native_depth_check','native_depth_shadow_check','native_depth_sampled_check','native_sampler_mip_check','native_color_target_check','nvn_pipeline_check','public_copy_check')):raise RuntimeError('storage must be independent')
 for name in('tests/graphics_storage_probe.c','tests/graphics_storage_cases.h','tests/pipeline_probe.c','src/pipeline_storage.c','src/pipeline.c','src/pipeline_program.c','android/pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing actual source closure '+name)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,check_key='native_graphics_storage_check',prefix='pipeline-',nonce_file='pipeline-run-nonce',profile=profile,scope='Ordinary Android authored native VS/FS storage read/write/size/sparse15, not public NVN/original source/game'))
