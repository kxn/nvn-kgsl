"""Use the existing public pipeline ordinary-UID/nonce/strict-stop collector."""
from pipeline_runner import exercise as transport
from nvn_pipeline_sampled_validation import NAMES,validate

def profile(record):
 import numpy
 c=record.get('nvn_pipeline_sampled_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-float2d-sampled-qualification-v1'or c.get('submissions')!=264:raise RuntimeError('normal public sampled22 bundle required')
 for k,v in record.items():
  if k.endswith('_check')and k!='nvn_pipeline_sampled_check'and v:raise RuntimeError('sampled22 must be the only qualification')
 for name in('src/sampled_image.c','src/texture_bc_shadow.c','src/block_copy.c','src/pipeline_views.c','src/nvn_pipeline_draw_prepare.inc','src/nvn_pipeline_draw_submit.inc','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_sampled_probe.inc','android/nvn_pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing actual sampled22 source '+name)
 if any('sampled_fixture_pack' in p for p in record.get('compiled_sources',[])):raise RuntimeError('HOST format oracle entered Android compilation')

def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_pipeline_sampled_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID public source-backed FLOAT2D22 SAMPLE/full-selected mip, actual GOB materialization, late-registration/replay. No game/mipfilter/mixedFETCH/Gather/depth claim.'))
