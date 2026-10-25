"""Existing Activity ordinary-UID/nonce/strictstop transport; no new measurement."""
from depth_sampled_probe_validation import NAMES,validate as check_pixels

def profile(record):
 import numpy
 c=record.get('native_depth_sampled_check')
 if type(c)is not dict or c.get('kind')!='raw-native-depth-sampled-qualification-v1'or c.get('submissions')!=180 or c.get('gpu_execution')is not False:raise RuntimeError('normal sampled depth qualification required')
 if any(k.endswith('_check')and k!='native_depth_sampled_check'and v for k,v in record.items()):raise RuntimeError('sampled depth must be an independent mode')
 for rel in('tests/depth_sampled_probe.c','src/depth_sampled.c','src/depth_sampled_native.c','src/pipeline.c','src/pipeline_program.c','android/pipeline_probe_activity.c'):
  if rel not in record['source']:raise RuntimeError('actual sampled depth closure missing '+rel)
def validate(data,captured,check,nonce,uid):
 import re
 if type(uid)is not int or uid<10000 or data.get('uid')!=uid or re.fullmatch('[0-9a-f]{32}',nonce or '')is None or data.get('run_nonce')!=nonce or data.get('run_directory')!='pipeline-'+nonce:raise RuntimeError('ordinary UID/fresh nonce required')
 if set(captured)!=set(NAMES):raise RuntimeError('exact sampled depth raw inventory')
 return check_pixels(data,captured,check)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='native_depth_sampled_check',prefix='pipeline-',nonce_file='pipeline-run-nonce',scope='Raw native noncompare D16/D24X8/D32F sampled actual TILE6_3, GOB/CS/A2DUPLOAD, SAMPLE nearest/linear, FETCH, SIZE, GatherR and changed-canonical same-job replay. No public NVN/game.'))
