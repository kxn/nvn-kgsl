"""Public BC profile on the existing ordinary-UID Activity/nonce/strict-stop transport."""
from pipeline_runner import exercise as transport
from nvn_pipeline_bc_validation import NAMES,validate
def profile(record):
 c=record.get('nvn_pipeline_bc_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-bc-sampled-qualification-v1'or c.get('submissions')!=24:raise RuntimeError('checked public NVN BC profile required')
 if any(record.get(k)for k in ('nvn_pipeline_check','nvn_pipeline_gob_check','native_pipeline_check','shader_check','maxwell_check','graphics_check','nvn_graphics_check','nvn_texture_check','texture_check','nvn_window_check','nvn_external_image_check','texture_conversion_check','fullsize_display_check','window_source_check','d2r_host','nvn_texture_bridge_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','compute_driverparams_check','compute_indirect','graphics_draw_batch','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','texture_bridge_diagnostics','bc_sampled_check','public_copy_check','public_volume_copy_check','texture_texels_check','block_copy_check')):raise RuntimeError('public BC must be the only operation')
 for name in ('src/pipeline_frontend.c','src/nvn_pipeline_draw_prepare.inc','src/nvn_pipeline_draw_submit.inc','src/texture_bc_shadow.c','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_bc_probe.inc','tests/bc_sampled_cases.h','android/nvn_pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing public BC source closure: '+name)
def exercise(args,build,record,adb,package,component,run):
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=validate,profile=profile,check_key='nvn_pipeline_bc_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID PUBLIC NVN actual descriptor/query/recipe BC sampling and same-handle late registration. No game frame or raw-native substitute.'))
