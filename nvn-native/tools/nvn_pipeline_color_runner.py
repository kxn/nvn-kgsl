"""Public GOB22 variant of existing ordinary-UID/nonce/strict-stop transport."""
from pipeline_runner import exercise as transport
from nvn_pipeline_color_validation import NAMES,validate
def profile(record):
 c=record.get('nvn_color_target_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-gob-color-targets-qualification-v1'or c.get('submissions')!=132:raise RuntimeError('normal checked public GOB22 required')
 for k,v in record.items():
  if k.endswith('_check')and k!='nvn_color_target_check'and v:raise RuntimeError('public GOB22 must be independent')
 for name in('src/color_target_shadow.c','src/color_target.c','src/nvn_pipeline_draw_prepare.inc','src/nvn_pipeline_draw_submit.inc','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_color_probe.inc','android/nvn_pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing public GOB22 source '+name)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_color_target_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID authored public NVN original color/GOB22 overwrite blend mask/Copy Call same-handle replay; no D2R game frame claim.'))
