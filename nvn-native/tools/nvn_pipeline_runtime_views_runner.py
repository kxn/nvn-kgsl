"""Use the existing Activity/nonce/inventory/strict-stop transport unchanged."""
from pipeline_runner import exercise as transport
from nvn_pipeline_runtime_views_validation import NAMES,validate
def profile(record):
 import numpy
 c=record.get('nvn_pipeline_runtime_views_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-runtime-target-qualification-v1'or c.get('submissions')!=72 or c.get('runtime_target_views')is not True:raise RuntimeError('checked runtime target profile required')
 for k,v in record.items():
  if k.endswith('_check')and k!='nvn_pipeline_runtime_views_check'and v:raise RuntimeError('runtime target qualifier must be independent')
 for k in ('src/pipeline_views.c','src/nvn_pipeline_draw_record.inc','src/nvn_pipeline_draw_submit.inc','tests/nvn_pipeline_probe.c','android/nvn_pipeline_probe_activity.c'):
  if k not in record['source']:raise RuntimeError('runtime target source closure missing '+k)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_pipeline_runtime_views_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID public NVN original shaders at noncatalogued target dimensions using actual FDL cache; no original game frame claim'))
