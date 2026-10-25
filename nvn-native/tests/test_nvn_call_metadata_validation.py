"""Offline Call metadata admission only; never a GPU qualification result."""
from pathlib import Path
import contextlib,copy,io,json,runpy
with contextlib.redirect_stdout(io.StringIO()):
 fixture=runpy.run_path(str(Path(__file__).with_name('test_nvn_pipeline_validation.py')))
v=fixture['v'];data=fixture['data'];check=fixture['check'];nonce=fixture['nonce'];checks=0
try:v.metadata(data,check,nonce,10001,call=True)
except RuntimeError:checks+=1
else:raise AssertionError('ordinary result accepted as Call qualification')
fields=dict(call_commands=True,call_source_ubo_bound=False,call_parent_ubo_index=0,call_wrappers=12,call_children_finalized_before_submit=12)
valid=copy.deepcopy(data);valid.update(fields);assert v.metadata(valid,check,nonce,10001,call=True);checks+=1
for key,value in fields.items():
 for mode in ('missing','wrong'):
  bad=copy.deepcopy(valid)
  if mode=='missing':del bad[key]
  else:bad[key]=not value if type(value)is bool else value+1
  try:v.metadata(bad,check,nonce,10001,call=True)
  except RuntimeError:checks+=1
  else:raise AssertionError((key,mode))
print(json.dumps(dict(passed=True,checks=checks,legacy_oracle_checks=fixture['checks'],GPU_execution=False,scope=__doc__)))
