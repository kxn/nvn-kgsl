"""Actual public CASC specialization, sharing existing nonce/UID/strict-stop transport."""
from pipeline_runner import exercise as transport
from casc_gather_depth_validation import NAMES,validate
def profile(record):
 c=record.get('casc_gather_depth_check')
 if type(c)is not dict or c.get('kind')!='public-casc-gather-fragdepth-qualification-v1'or c.get('submissions')!=72:raise RuntimeError('normal public CASC72 check required')
 for k,v in record.items():
  if k.endswith('_check')and k!='casc_gather_depth_check'and v:raise RuntimeError('public CASC72 is an independent mode')
 for rel in('tests/nvn_gather_depth_probe.c','tests/nvn_pipeline_probe.c','src/nvn.c','src/depth_shadow.c','src/nvn_depth_draw.inc','src/pipeline_depth_archive.c','android/nvn_pipeline_probe_activity.c'):
  if rel not in record['source']:raise RuntimeError('missing actual public CASC source '+rel)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='casc_gather_depth_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID unchanged original CASC Gather+FragDepth via public NVN, authored R32F/T10/S3/CB6 and true zeroCOLOR; not captured D2R Draw state or game frame.'))
