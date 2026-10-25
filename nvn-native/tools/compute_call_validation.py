"""Strict existing-runner metadata admission for the original four-CS Call qualifier."""
COUNTS=dict(completed_cases=32,submissions=32,finishes=32,direct_submissions=24,indirect_submissions=8,prep_submissions=8,prep_finishes=8,handles_recorded=16,handles_released=16,handle_replays=16,state_only_seeds_recorded=16,source_handles_recorded=16,source_handles_finalized_before_submit=16,CallCommands_recorded=16,program_busy_rejections=32,cpu_zero_group_checks=8,gpu_group_readbacks=8,uniform_readbacks=32,data_byte_checks=524288,old_binding_pool_byte_checks=524288,command_byte_checks=524288,output_byte_checks=9440,readonly_byte_checks=507520,errno=0,cleanup_errno=0)
FLAGS=dict(passed=True,state_only_seed_submitted=False,copy_jobs_in_call=False,ordinary_prep_queue=True,GPU_effective_bindings_verified=True,CPU_shader_execution=False,runtime_compiler=False,game_modules_executed=False,resources_retained=False)
def validate(record,data):
 check=record.get('maxwell_check');manifest=record.get('compute_indirect_validation')
 if(record.get('compute_call')is not True or record.get('compute_indirect')is not True or type(check)is not dict or check.get('chip_id')!=0x06050002 or len(check.get('cases',[]))!=4 or type(manifest)is not dict or manifest.get('chip_id')!=0x06050002 or len(manifest.get('entries',[]))!=4 or manifest.get('runtime_compiler')is not False):raise RuntimeError('checked original four-CS Call build required')
 if type(data)is not dict or type(data.get('schema'))is not int or data['schema']!=1 or data.get('operation')!='nvn_maxwell_compute_call' or data.get('stage')!='complete' or data.get('submission')!='accepted' or type(data.get('uid'))is not int or data['uid']<10000:raise RuntimeError('ordinary UID completed Call operation required')
 if any(type(data.get(k))is not int or data[k]!=v for k,v in COUNTS.items()):raise RuntimeError('compute Call count/cleanup mismatch')
 if any(type(data.get(k))is not bool or data[k]is not v for k,v in FLAGS.items()):raise RuntimeError('compute Call behavior/evidence mismatch')
 if type(data.get('guard_byte_checks'))is not int or data['guard_byte_checks']<=0:raise RuntimeError('missing object/control guards')
 old,current=data.get('source_resource_base'),data.get('parent_resource_base')
 if any(type(v)is not int or not 0<v<=0xffffffffffffffff-16384 for v in (old,current)) or not(old+16384<=current or current+16384<=old):raise RuntimeError('distinct source/parent resource identities required')
 cases=data.get('cases')
 if type(cases)is not list or len(cases)!=32:raise RuntimeError('complete compute Call case matrix required')
 expected=[]
 for p in range(4):
  for shape in range(4):
   indirect=shape==3;count=(1,33,65,65)[shape]
   for repeat in range(2):
    groups=3 if indirect and repeat else 1 if indirect else(count+31)//32
    expected.append(dict(program=p,source_program=(p+1)%4,shape=shape,indirect=indirect,replay=repeat,groups=[groups,1,1],limit=count,output_elements=(65 if repeat else 32)if indirect else count,source_ubo=old+10240,source_ssbo=[old+260,old+4356],parent_ubo=current+10240,parent_ssbo=[current+260,current+4356],prep_queue_used=indirect,passed=True))
 for case,want in zip(cases,expected):
  if type(case)is not dict or set(case)!=set(want):raise RuntimeError('unexpected compute Call case fields')
  for key,value in want.items():
   got=case[key]
   if type(got)is not type(value) or got!=value or(type(value)is list and any(type(v)is not int for v in got)):raise RuntimeError('compute Call case/state/resource mismatch '+key)
 return dict(passed=True,Call_submissions=32,ordinary_prep_submissions=8,total_GPU_submissions=40,scope='Actual original four-CS public Call/direct/indirect/replay with distinct recorded/parent bindings; no game module or same-handle copy qualification')
