"""Thin public BUFFER variant of the existing nonce/UID/raw/strict-stop transport."""
from pipeline_runner import exercise as transport
from nvn_pipeline_buffer_validation import NAMES,validate
def profile(record):
 c=record.get('nvn_pipeline_buffer_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-buffer-sampled-qualification-v1'or c.get('submissions')!=48 or c.get('index_domain')!=[0]:raise RuntimeError('checked original C1 public BUFFER12 required')
 for k,v in record.items():
  if k.endswith('_check')and k!='nvn_pipeline_buffer_check'and v:raise RuntimeError('public BUFFER must be the only qualification')
 for name in('src/nvn_pipeline_draw_prepare.inc','src/texture_buffer_sample.c','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_buffer_probe.inc','tests/nvn_pipeline_buffer_cases.h','android/nvn_pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing public BUFFER actual source closure '+name)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_pipeline_buffer_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID PUBLIC NVN original bounded C1 full256/two actual resources/FloatTXF12 index0 only/Copy+Call+late version replay. No real-game binding or frame claim.'))
