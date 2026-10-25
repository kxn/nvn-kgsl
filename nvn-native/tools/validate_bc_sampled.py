#!/usr/bin/env python3
"""Validate supplied BC GPU-qualifier result/files; does not run ADB or prove origin."""
import argparse,hashlib,importlib.util,json,stat
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def verify(check):
 c=json.loads(check.read_text())
 if c.get('kind')!='raw-bc-sampled-qualification-v1'or not c.get('passed')or c.get('gpu_execution')is not False:raise ValueError('unknown build package')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('source/artifact changed '+p)
 if len(c['cases'])!=24 or len({x['file']for x in c['cases']})!=24:raise ValueError('invalid fixed case matrix')
 expected=[(index,f,w,h,r,f'bc-{index:02}-{r}.rgba',w*h*4)for index,(f,w,h)in enumerate((f,w,h)for f in range(0x41,0x45)for w,h in ((8,8),(37,21),(128,65)))for r in (0,1)]
 actual=[tuple(x[k]for k in ('index','format','width','height','repeat','file','bytes'))for x in c['cases']]
 if actual!=expected or c.get('channel_checks')!=sum(x[-1]for x in expected):raise ValueError('changed fixed case matrix')
 return c
def compare(check,result_path,files):
 c=verify(check);r=json.loads(result_path.read_text())
 exact=dict(schema=1,operation='raw_bc_sampled',stage='complete',errno=0,cleanup_errno=0,resources_retained=False,
  submissions=24,finishes=24,completed_cases=24,readbacks=24,same_job_replays=12,channel_checks=c['channel_checks'],
  cpu_decompression=False,cpu_image_conversion=False,public_nvn=False,game_frame=False,authored_bc_controls=True,passed=True)
 for k,v in exact.items():
  if r.get(k)!=v or type(r.get(k))is not type(v):raise ValueError('result contract failed '+k)
 if type(r.get('max_error'))is not int or not 0<=r['max_error']<=1:raise ValueError('reported error outside tolerance')
 if type(r.get('guard_byte_checks'))is not int or r['guard_byte_checks']<24*9216:raise ValueError('incomplete guard checks')
 if type(r.get('readonly_byte_checks'))is not int or r['readonly_byte_checks']<24*12288:raise ValueError('incomplete read-only checks')
 producer=Path(__file__).with_name('build_bc_sampled_qualification.py');s=importlib.util.spec_from_file_location('bc_closed_oracle',producer);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 rows=[];maximum=0;channels=0
 for case in c['cases']:
  p=files/case['file']
  if not p.exists()or p.is_symlink()or not stat.S_ISREG(p.stat().st_mode)or p.stat().st_size!=case['bytes']:raise ValueError('missing/nonregular/wrong-size RGBA '+str(p))
  actual=p.read_bytes();expected=b''.join(m.expected(case['format'],x,y,case['repeat'])for y in range(case['height'])for x in range(case['width']))
  if hashlib.sha256(expected).hexdigest()!=case['expected_sha256']:raise ValueError('oracle package changed')
  delta=max(abs(a-b)for a,b in zip(actual,expected));maximum=max(maximum,delta);channels+=len(actual)
  if delta>1:raise ValueError('pixel mismatch '+case['file'])
  rows.append(dict(file=case['file'],bytes=len(actual),sha256=sha(p),max_error=delta))
 return dict(schema=1,passed=True,scope='Supplied readback bytes/JSON revalidated; Android/GPU origin belongs to root deployment evidence',
  package_sha256=sha(check),result_sha256=sha(result_path),channel_checks=channels,max_error=maximum,images=rows)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('check','result','files','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();r=compare(a.check.resolve(),a.result.resolve(),a.files.resolve());a.out.write_text(json.dumps(r,indent=2)+'\n');print(a.out)
