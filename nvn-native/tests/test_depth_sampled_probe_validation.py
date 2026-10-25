#!/usr/bin/env python3
"""HOST oracle controls only. Synthetic successful images are never GPU evidence."""
import argparse,copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from depth_sampled_probe_validation import NAMES,case,pixels,validate
p=argparse.ArgumentParser();p.add_argument('--check',type=Path,required=True);p.add_argument('--host-result',type=Path,required=True);p.add_argument('--host-raw',type=Path,required=True);a=p.parse_args();q=json.loads(a.check.read_text());data=json.loads(a.host_result.read_text());images={};guards=ro=0
for i,name in enumerate(NAMES):
 c=case(i);r=q['cases'][i//2];b=bytearray([0xa5])*r['bo_bytes'];expected=pixels(c,i%2)
 for y in range(c['height']):b[4096+y*r['pitch']:4096+y*r['pitch']+c['width']*4]=expected[y].tobytes()
 images[name]=bytes(b);guards+=len(b)-c['height']*c['width']*4;ro+=r['readonly_bytes']
assert data['cases']==q['cases'], 'actual HOST matrix differs from independently produced catalogue'
assert data['guard_byte_checks']==guards and data['readonly_byte_checks']==ro, 'actual HOST byte-check totals differ'
synthetic=copy.deepcopy(data);synthetic['cases']=q['cases'];synthetic['guard_byte_checks']=guards;synthetic['readonly_byte_checks']=ro;answer=validate(synthetic,images,q)
negatives=[]
def reject(label,d,b):
 try:validate(d,b,q)
 except(ValueError,KeyError):negatives.append(label);return
 raise AssertionError('accepted '+label)
bad=dict(images);b=bytearray(bad[NAMES[0]]);b[4096]^=64;bad[NAMES[0]]=bytes(b);reject('wrong depth channel',synthetic,bad)
bad=dict(images);bad[NAMES[0]]=images[NAMES[1]];reject('stale source replay',synthetic,bad)
bad=dict(images);b=bytearray(bad[NAMES[0]]);b[0]=0;bad[NAMES[0]]=bytes(b);reject('target prefix',synthetic,bad)
bad=dict(images);bad[NAMES[0]]=b'error';reject('short missing raw',synthetic,bad)
for key in('submissions','finishes','completed_cases','readonly_byte_checks','guard_byte_checks','inflight_owner_checks'):
 d=copy.deepcopy(synthetic);d[key]-=1;reject('wrong '+key,d,images)
d=copy.deepcopy(synthetic);d['cases'][0]['source_width']+=1;reject('wrong selected FDL metadata',d,images)
d=copy.deepcopy(synthetic);d['same_job_replay']=False;reject('no actual same job replay',d,images)
reject('nonexecuting HOST pixels',{**data,'cases':q['cases']},{n:(a.host_raw/n).read_bytes()for n in NAMES})
print(json.dumps(dict(passed=True,HOST_synthetic_only=True,nonexecuting_HOST_pixels_rejected=True,negatives=negatives,synthetic_oracle=answer)))
