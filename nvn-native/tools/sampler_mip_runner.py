"""Existing fresh-nonce Activity/fullraw/ordinary UID/strict-stop transport."""
from sampler_mip_validation import NAMES,validate

def profile(record):
 import numpy
 c=record.get('native_sampler_mip_check')
 if type(c)is not dict or c.get('kind')!='raw-native-sampler-mip-qualification-v1'or c.get('submissions')!=576 or c.get('gpu_execution')is not False:raise RuntimeError('normal authored sampler mip qualifier required')
 if any(record.get(k)for k in('nvn_api','d2r_host','native_gather_check','native_pipeline_check','native_color_target_check','nvn_pipeline_check','public_copy_check','public_volume_copy_check','public_copy_call','clear_color_check','buffer_texture_check','volume_texture_check')):raise RuntimeError('sampler mip mode must be independent')
 for name in('tests/sampler_mip_probe.c','tests/sampler_mip_cases.h','tests/pipeline_probe.c','src/pipeline.c','src/pipeline_program.c','src/sampled_image.c','src/sampler_state.c','android/pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing actual source closure '+name)

def exercise(args,build,record,adb,package,component,run):
 from pipeline_runner import exercise as transport
 return transport(args,build,record,adb,package,component,run,qualification={'names':NAMES,'validate':validate,'check_key':'native_sampler_mip_check','prefix':'pipeline-','nonce_file':'pipeline-run-nonce','profile':profile,'scope':'ordinary Android authored native normalized2D isotropic mip/minmag/wrap/LOD sampler; not public NVN/Maxwell/game/anisotropy qualification'})
