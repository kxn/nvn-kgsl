"""HOST synthetic metadata admission only; no GPU or shader execution."""
from pathlib import Path
import copy,json,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import compute_call_validation as v
record=dict(compute_call=True,compute_indirect=True,maxwell_check=dict(chip_id=0x06050002,cases=[{}]*4),compute_indirect_validation=dict(chip_id=0x06050002,entries=[{}]*4,runtime_compiler=False))
data=dict(schema=1,operation='nvn_maxwell_compute_call',uid=10001,stage='complete',submission='accepted',guard_byte_checks=59392,source_resource_base=0x1000000,parent_resource_base=0x2000000,cases=[],**v.COUNTS,**v.FLAGS)
for p in range(4):
 for shape in range(4):
  indirect=shape==3;count=(1,33,65,65)[shape]
  for repeat in range(2):
   groups=3 if indirect and repeat else 1 if indirect else(count+31)//32
   data['cases'].append(dict(program=p,source_program=(p+1)%4,shape=shape,indirect=indirect,replay=repeat,groups=[groups,1,1],limit=count,output_elements=(65 if repeat else 32)if indirect else count,source_ubo=0x1000000+10240,source_ssbo=[0x1000000+260,0x1000000+4356],parent_ubo=0x2000000+10240,parent_ssbo=[0x2000000+260,0x2000000+4356],prep_queue_used=indirect,passed=True))
assert v.validate(record,data)['passed'];checks=1
for key,value in {**v.COUNTS,**v.FLAGS}.items():
 bad=copy.deepcopy(data);bad[key]=not value if type(value)is bool else value+1
 try:v.validate(record,bad)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
for key,value in [('schema',True),('schema',2),('uid',0),('source_resource_base',0x2000000),('source_resource_base',0x2000000-4096),('guard_byte_checks',0),('passed',1),('submissions',True),('operation','nvn_maxwell_compute_indirect')]:
 bad=copy.deepcopy(data);bad[key]=value
 try:v.validate(record,bad)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
for key,value in [('source_program',0),('source_ubo',0x2000000+10240),('parent_ssbo',[1,2]),('groups',[True,1,1]),('prep_queue_used',True),('passed',False)]:
 bad=copy.deepcopy(data);bad['cases'][0][key]=value
 try:v.validate(record,bad)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
for key in ['compute_call','compute_indirect']:
 bad=copy.deepcopy(record);bad[key]=False
 try:v.validate(bad,data)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
print(json.dumps(dict(passed=True,checks=checks,GPU_execution=False,CPU_shader_execution=False,scope=__doc__)))
