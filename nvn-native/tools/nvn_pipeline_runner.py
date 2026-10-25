"""Public-NVN qualifier profile using the existing verified APK/nonce/stop collector."""
from pipeline_runner import exercise as transport
from nvn_pipeline_validation import NAMES,validate

def profile(record):
 import numpy  # Resolve the offline oracle dependency before any device work.
 c=record.get('nvn_pipeline_check')
 if type(c)is not dict or c.get('kind')!='public-nvn-dynamic-pipeline-qualification-v1' or c.get('source_layout')!='linear' or c.get('submissions')!=24:raise RuntimeError('checked public NVN LINEAR64 profile required')
 if any(record.get(k) for k in ('native_pipeline_check','shader_check','maxwell_check','graphics_check','nvn_graphics_check','nvn_texture_check','texture_check','nvn_window_check','nvn_external_image_check','texture_conversion_check','fullsize_display_check','window_source_check','d2r_host','nvn_texture_bridge_check','nvn_copy_check','nvn_counter_check','nvn_sampler_check','compute_driverparams_check','compute_indirect','graphics_draw_batch','device_inspect','iocoherent_inspect','ahb_inspect','ahb_graphics_check','ahb_present_check','texture_bridge_diagnostics')):raise RuntimeError('public NVN pipeline must be the only operation')
 for name in ('src/pipeline_frontend.c','src/nvn_pipeline_draw_record.inc','src/nvn_pipeline_draw_submit.inc','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_oracle.h','android/nvn_pipeline_probe_activity.c'):
  if name not in record['source']:raise RuntimeError('missing actual public NVN source closure: '+name)
def exercise(args,build,record,adb,package,component,run):
 call=record.get('nvn_pipeline_call',False)
 if type(call)is not bool:raise RuntimeError('Call profile flag must be boolean')
 def selected_validate(data,captured,check,nonce,uid):return validate(data,captured,check,nonce,uid,call=call)
 return transport(args,build,record,adb,package,component,run,qualification=dict(names=NAMES,validate=selected_validate,profile=profile,check_key='nvn_pipeline_check',prefix='nvn-pipeline-',nonce_file='nvn-pipeline-run-nonce',scope='Ordinary UID public NVN Program/source matching/actual UBO/VFD/dynamic native Draw; no original game Draw/frame claim'))
