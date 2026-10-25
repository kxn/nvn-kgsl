"""Independent flags9 GOB public-NVN qualifier, using the existing APK transport."""
from pipeline_runner import exercise as transport
from nvn_pipeline_gob_validation import NAMES,validate

def profile(record):
 import numpy
 c=record.get('nvn_pipeline_gob_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-gob-pipeline-qualification-v1' or c.get('source_flags')!=9 or c.get('source_kind')!=254 or c.get('submissions')!=72:raise RuntimeError('separate checked public NVN flags9 GOB profile required')
 if any(record.get(k) for k in ('nvn_pipeline_check','native_pipeline_check','shader_check','maxwell_check','graphics_check','nvn_graphics_check','nvn_texture_check','texture_check','nvn_window_check','nvn_external_image_check','texture_conversion_check','fullsize_display_check','window_source_check','d2r_host','nvn_texture_bridge_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','compute_driverparams_check','compute_indirect','graphics_draw_batch','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','texture_bridge_diagnostics')):raise RuntimeError('GOB qualifier must be the only operation')
 for name in ('src/pipeline_frontend.c','src/nvn_pipeline_draw_record.inc','src/nvn_pipeline_draw_submit.inc','tests/nvn_pipeline_probe.c','android/nvn_pipeline_probe_activity.c','tools/texture_conversion_validation.py'):
  if name not in record['source']:raise RuntimeError('missing actual GOB source/oracle closure: '+name)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_pipeline_gob_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID public NVN original color Draw to actual flags9 canonical Generic16Bx2 storage; offline oracle only, no runtime CPU conversion/game frame claim'))
