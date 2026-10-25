"""Actual public CASC specialization, sharing existing nonce/UID/strict-stop transport."""
from pipeline_runner import exercise as transport
from public_depth_sampled_validation import NAMES,validate
def profile(record):
 c=record.get('public_depth_sampled_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-depth-sampled-qualification-v1'or c.get('submissions')!=144:raise RuntimeError('normal public CASC144 check required')
 for k,v in record.items():
  if k.endswith('_check')and k!='public_depth_sampled_check'and v:raise RuntimeError('public CASC144 is an independent mode')
 for rel in('tests/public_depth_sampled_probe.c','tests/nvn_pipeline_probe.c','src/nvn.c','src/depth_shadow.c','src/nvn_depth_draw.inc','src/pipeline_depth_archive.c','android/nvn_pipeline_probe_activity.c'):
  if rel not in record['source']:raise RuntimeError('missing actual public CASC source '+rel)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='public_depth_sampled_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID unchanged original CASC Gather+FragDepth via public NVN, authored depth-source R001 and actual SAMPLE/Gather resources and true zeroCOLOR; not captured D2R Draw state or game frame.'))
