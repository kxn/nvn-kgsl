"""Reuse existing parameterized Activity/nonce/full-raw/strict-stop transport."""
from clear_color_validation import NAMES,validate
def profile(record):
 c=record.get('clear_color_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-clear-color-qualification-v1'or c.get('gpu_execution')is not False or c.get('files')!=536:raise RuntimeError('normal clear qualification required')
 if any(record.get(k)for k in ('d2r_host','public_copy_check','public_volume_copy_check','public_copy_call','native_color_target_check','native_pipeline_check','nvn_pipeline_check','buffer_texture_check','volume_texture_check')):raise RuntimeError('clear must be independent')
 for name in ['src/clear_color.c','src/clear_color_record.inc','tests/clear_color_probe.c','tests/clear_color_cases.h','android/clear_color_activity.c','android/public_copy_activity.c']:
  if name not in record['source']:raise RuntimeError('missing clear source closure '+name)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,check_key='clear_color_check',prefix='clear-color-',nonce_file='clear-color-run-nonce',profile=profile,scope='ordinary Android public NVN ClearColor + Call,22 finite-float formats/full canonical guards/two origins/2880x1620 R11; no game/frame/performance claim'))
