"""HOST ONLY validator oracle fixture, never GPU output or execution evidence."""
import copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import color_target_validation as validator

def synthetic(check):
 cases=[];blobs={};guards=readonly=active=0
 for fi,f in enumerate(check['formats']):
  for state in range(3):
   i=fi*3+state;n=(f['span']+8192+4095)//4096*4096
   c=dict(case=i,format_index=fi,format=f['format'],state=state,width=73,height=41,cpp=f['cpp'],component_mask=f['component_mask'],pitch=f['pitch'],span=f['span'],bo_bytes=n,job_bytes=49152,command_bytes=1024,resource_count=6,rb_mrt_buf_info=f['rb_mrt_buf_info'],sp_ps_mrt_reg=f['sp_ps_mrt_reg'],srgb=f['srgb']);cases.append(c)
   for repeat in range(2):
    tight=(Path(check['expected'])/('expected-%03d.raw'%(i*2+repeat))).read_bytes();raw=bytearray([0xa5])*n
    for y in range(41):raw[4096+y*f['pitch']:4096+y*f['pitch']+73*f['cpp']]=tight[y*73*f['cpp']:(y+1)*73*f['cpp']]
    blobs[validator.NAMES[i*2+repeat]]=bytes(raw);guards+=n-73*41*f['cpp'];readonly+=4*4096+49152-4;active+=73*41*f['cpp']
 data=dict(schema=1,operation='native_color_targets',uid=10001,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,jobs=66,submissions=132,finishes=132,files=132,completed_cases=132,replays=66,pixel_checks=0,target_prefix=4096,native_ubo_binding=1,original_maxwell_cb=3,ubo_bytes=64,sequence_offset=45056,sequence_bytes_exempt=4,passed=True,resources_retained=False,pixel_oracle_on_host=True,same_job_replay=True,cpu_image_conversion=False,nvn_api=False,game_modules_executed=False,legacy_image_template=False,run_nonce='1'*32,run_directory='pipeline-'+'1'*32,cases=cases,guard_byte_checks=guards,readonly_byte_checks=readonly,active_bytes_saved=active,inflight_owner_checks=132*8)
 return data,blobs

def main():
 check=json.loads(Path(sys.argv[1]).read_text());data,blobs=synthetic(check);ok=validator.validate(data,blobs,check,'1'*32,10001);assert ok['passed']
 negatives=0
 def reject(d,b):
  nonlocal negatives
  try:validator.validate(d,b,check,'1'*32,10001)
  except(RuntimeError,KeyError,ValueError):negatives+=1;return
  raise AssertionError('invalid evidence accepted')
 for key,value in [('readonly_byte_checks',1),('sequence_bytes_exempt',8),('sequence_offset',45060),('submissions',131),('pixel_checks',1),('pixel_oracle_on_host',False),('resources_retained',True),('cpu_image_conversion',True),('nvn_api',True),('uid',0),('run_nonce','2'*32),('completed_cases',131)]:
  bad=copy.deepcopy(data);bad[key]=value;reject(bad,blobs)
 for key,value in [('format',37),('rb_mrt_buf_info',0),('srgb',0),('cpp',16),('job_bytes',1<<30),('resource_count',7)]:
  bad=copy.deepcopy(data);bad['cases'][0][key]=value;reject(bad,blobs)
 missing=blobs.copy();del missing[validator.NAMES[-1]];reject(data,missing)
 for number,off in [(6,4096),(0,0),(7,4096),(10,4097)]:
  bad=blobs.copy();value=bytearray(bad[validator.NAMES[number]]);value[off]^=1 if number in(7,10)else 32;bad[validator.NAMES[number]]=bytes(value);reject(data,bad)
 print(json.dumps({'passed':True,'host_only':True,'negative_cases':negatives,'synthetic_verification':ok,'gpu_execution':False}))
if __name__=='__main__':main()
