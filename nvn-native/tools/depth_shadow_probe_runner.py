"""Existing raw native Activity/UID/nonce/strictstop transport, no new renderer."""
from depth_shadow_probe_validation import NAMES,validate
def profile(record):
 import numpy
 c=record.get('native_depth_shadow_check')
 if type(c)is not dict or c.get('kind')!='raw-native-canonical-depth-qualification-v1'or c.get('submissions')!=648 or c.get('gpu_execution')is not False:raise RuntimeError('normal canonical depth qualification required')
 if any(k.endswith('_check')and k!='native_depth_shadow_check'and v for k,v in record.items()):raise RuntimeError('canonical depth must be independent operation')
 for rel in('tests/depth_shadow_probe.c','tests/depth_shadow_cases.h','src/depth_shadow.c','src/depth_transfer.c','src/block_copy.c','src/pipeline.c','src/pipeline_program.c','android/pipeline_probe_activity.c'):
  if rel not in record['source']:raise RuntimeError('actual canonical depth source missing '+rel)
def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='native_depth_shadow_check',prefix='pipeline-',nonce_file='pipeline-run-nonce',scope='Raw native canonical D16/D24X8/D32F full GOB-CS/A2DUPLOAD/actual depth Draw/A2DREADBACK/CS-GOB chain, selected mip/layer, nonuniform source, true zeroCOLOR and same-job updated-source replay. No public NVN/game.'))
