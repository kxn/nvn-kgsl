#!/usr/bin/env python3
"""Offline Khronos GLES3-HQ reference for saved authored native mip results.

No device/API execution. This does not replace the original exact oracle or
establish an NVN specification. The C++ verifier accepts generic RGBA8 affine
cases; this first evidence provider admits only the explicitly versioned authored
native sampler-mip fixture. A future public provider needs its own exact shader,
resource, and closed-math identity and may reuse the same pinned utility.
"""
import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import urllib.request


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(path):return json.loads(Path(path).read_text())
def write(path,value):Path(path).write_text(json.dumps(value,indent=2)+'\n')
def regular(path):
 path=Path(path)
 if path.is_symlink()or not path.is_file():raise ValueError('regular non-symlink input required: '+str(path))
 return path

def module(path):
 spec=importlib.util.spec_from_file_location('original_sampler_mip_validation',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def evidence_inputs(root,path,manifest):
 evidence=load(regular(path));data=evidence['result'];build=evidence['build'];check=build['native_sampler_mip_check']
 if check.get('kind')!='raw-native-sampler-mip-qualification-v1'or check.get('gpu_execution')is not False:raise ValueError('unsupported versioned authored qualification')
 for relative,digest in manifest['authored_native_mip_v1_source'].items():
  if build['source'].get(relative)!=digest:raise ValueError('authored fixture identity differs: '+relative)
 validator=root/'nvn-native/tools/sampler_mip_validation.py'
 if sha(regular(validator))!=manifest['authored_native_mip_v1_source']['tools/sampler_mip_validation.py']:raise ValueError('original reference source changed; explicit new provider required')
 original=module(validator);nonce=evidence['run_nonce'];uid=data.get('uid');original.metadata(data,check,nonce,uid)
 stop=evidence.get('force_stop',{});pid=evidence.get('final_pidof',{})
 if evidence.get('stopped')is not True or type(stop.get('returncode'))is not int or stop['returncode']!=0 or type(pid.get('returncode'))is not int or pid['returncode']!=1 or pid.get('stdout')or pid.get('stderr'):raise ValueError('actual owner process strict stop not proved')
 if type(check.get('target'))is not dict or check['target'].get('pitch')!=256 or check['target'].get('width')!=64 or check['target'].get('height')!=64:raise ValueError('unsupported authored target profile')
 if set(evidence.get('copy_images',{}))!=set(original.NAMES):raise ValueError('exact actual fullraw image inventory required')
 captured={};inputs={str(path.resolve()):sha(path),str(validator):sha(validator)};guards=0
 for name in original.NAMES:
  item=evidence['copy_images'][name];p=regular(item['path']);blob=p.read_bytes()
  if type(item.get('bytes'))is not int or len(blob)!=item['bytes']or len(blob)!=data['bo_bytes']or sha(p)!=item['sha256']:raise ValueError('actual raw size/SHA mismatch: '+name)
  if blob[:4096]!=b'\xa5'*4096 or blob[20480:]!=b'\xa5'*(len(blob)-20480):raise ValueError('actual fullraw target guard changed')
  guards+=len(blob)-16384;captured[name]=blob;inputs[str(p.resolve())]=item['sha256']
 if guards!=data['guard_byte_checks']:raise ValueError('actual guard coverage mismatch')
 legacy_error=None
 try:original.validate(data,captured,check,nonce,uid)
 except RuntimeError as error:legacy_error=str(error)
 return evidence,check,captured,inputs,legacy_error,guards

def prepare(path,check,captured):
 # Only declared fixture INPUT texels are produced here for the independent
 # offline reference texture. No actual target bytes are used as expectations;
 # no data is uploaded or converted for a native runtime.
 with Path(path).open('xb')as stream:
  stream.write(struct.pack('<2I',0x32535443,576))
  for case in check['cases']:
   v=check['views'][case['view']]
   for repeat in range(2):
    ox=-721/2048+repeat*.125;oy=-1297/4096-repeat*.25
    bits=lambda x:struct.unpack('<I',struct.pack('<f',x))[0]
    values=[case['id'],repeat,case['min_filter'],case['mag_filter'],case['wrap_index'],v['view_width'],v['view_height'],v['base_level'],v['level_count'],v['width'],v['height'],*case['scale_bits'],case['lod_min_bits'],case['lod_max_bits'],case['lod_bias_bits'],64,64,bits(ox),bits(oy)]
    stream.write(struct.pack('<20I',*values))
    for relative in range(v['level_count']):
     physical=v['base_level']+relative;w=max(1,v['width']>>physical);h=max(1,v['height']>>physical)
     stream.write(bytes(32+(x*13+y*7+c*37+physical*29+repeat*17)%192 for y in range(h)for x in range(w)for c in range(4)))
    stream.write(captured['sampler-mip-%03d.raw'%(case['id']*2+repeat)][4096:20480])

def sources(manifest,out,existing):
 if existing is None:
  base=out/'cts-source';base.mkdir()
  def fetch(item):
   relative,entry=item;p=base/relative;p.parent.mkdir(parents=True,exist_ok=True)
   with urllib.request.urlopen(entry['url'],timeout=30)as f:raw=f.read()
   if len(raw)!=entry['bytes']or hashlib.sha256(raw).hexdigest()!=entry['sha256']:raise ValueError('pinned CTS download mismatch '+relative)
   p.write_bytes(raw)
  with concurrent.futures.ThreadPoolExecutor(max_workers=8)as pool:list(pool.map(fetch,manifest['files'].items()))
 else:base=Path(existing).resolve()
 hashes={}
 for relative,entry in manifest['files'].items():
  p=regular(base/relative)
  if p.stat().st_size!=entry['bytes']or sha(p)!=entry['sha256']:raise ValueError('pinned CTS source changed '+relative)
  hashes[str(p)]=entry['sha256']
 return base,hashes

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('root','evidence','out'):p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--cts-source',type=Path,help='Existing exact pinned checkout/cache; all required sources still SHA verified')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();native=Path(__file__).resolve().parents[1]
 if out.exists()or out.is_symlink()or not out.is_relative_to(root/'.local'):p.error('fresh worktree/.local output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='offline-khronos-gles3-HQ-sampler-mip-check-v1',passed=False,GPU_execution=False,ADB_invocations=0,NVN_normative_claim=False,runs=runs)
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True);runs.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));write(out/'commands.json',runs)
  if r.returncode:raise ValueError(r.stdout[-1500:]+r.stderr[-4000:])
  return r.stdout
 try:
  manifest_path=native/'tools/cts_texture_sources.json';manifest=load(manifest_path);adapter=native/'tools/cts_texture_lookup.cc'
  if manifest['schema']!=1 or manifest['commit']!='a119e62de3bb38dae67f05abee8acd011e0c729c'or manifest['HQ_profile']!=dict(derivateBits=18,lodBits=6,coordBits=[20,20,0],uvwBits=[7,7,0],filtered_storage_units=2,nearest_storage_units=0):raise ValueError('fixed independent HQ profile changed')
  for path in (Path(__file__).resolve(),manifest_path,adapter,Path(sys.executable).resolve()):inputs[str(path)]=sha(regular(path))
  evidence,check,captured,extra,legacy_error,guards=evidence_inputs(root,a.evidence.resolve(),manifest);inputs.update(extra)
  data=out/'actual-cases.bin';prepare(data,check,captured)
  pinned,extra=sources(manifest,out,a.cts_source);inputs.update(extra)
  objects=[];flags=['-D_POSIX_C_SOURCE=200809L','-O2','-g','-ffunction-sections','-fdata-sections',*['-I'+str(pinned/x)for x in manifest['include_directories']]]
  for tool in ('g++','gcc','readelf','nm','ldd'):
   path=Path(shutil.which(tool)).resolve();inputs[str(path)]=sha(path);run([tool,'--version'])
  for tool in ('cc1plus','cc1','as','ld','collect2'):
   value=run(['g++','-print-prog-name='+tool]).strip();path=Path(value)if '/'in value else Path(shutil.which(value));path=path.resolve();inputs[str(path)]=sha(path)
  for i,relative in enumerate(manifest['cpp_sources']+manifest['c_sources']):
   cpp=relative.endswith('.cpp');obj=out/(str(i)+'.o');run(['g++'if cpp else'gcc','-std=c++17'if cpp else'-std=c11',*flags,'-MD','-MF',str(obj)+'.d','-c',pinned/relative,'-o',obj]);objects.append(obj)
  obj=out/'adapter.o';run(['g++','-std=c++17',*flags,'-Wall','-Wextra','-Werror','-MD','-MF',str(obj)+'.d','-c',adapter,'-o',obj]);exe=out/'cts_texture_lookup';run(['g++','-Wl,--gc-sections',obj,*objects,'-lm','-o',exe])
  deps=set()
  for path in out.glob('*.d'):
   for token in shlex.split(path.read_text().replace('\\\n',' '))[1:]:
    f=Path(token).resolve();deps.add(str(f));inputs[str(f)]=sha(f)
  required={str((pinned/name).resolve())for name in manifest['files']}
  third_party={name for name in deps if Path(name).is_relative_to(pinned)}
  if not required.issuperset(third_party):raise ValueError('unrecorded third-party source dependency')
  text=run([exe,data]);result=json.loads(text);write(out/'result.json',result)
  if result.get('passed')is not True or result.get('files')!=576 or result.get('channels')!=9437184 or result.get('failed_pixels')!=0 or result.get('independent_negative_controls')!=7 or result.get('independent_positive_controls')!=2:raise ValueError('incomplete actual-raw CTS verification')
  dynamic=run(['readelf','-d',exe]);(out/'dynamic.txt').write_text(dynamic)
  if any(s in dynamic.lower()for s in ('libegl','libgles','libvulkan','libmetal')):raise ValueError('unexpected graphics API runtime')
  linked=run(['ldd',exe]);(out/'linked-libraries.txt').write_text(linked)
  for row in linked.splitlines():
   for value in row.split():
    if value.startswith('/'):
     path=Path(value).resolve();inputs[str(path)]=sha(path)
  symbols=run(['nm','-C',exe]);(out/'symbols.txt').write_text(symbols)
  for symbol in ('tcu::computeLodBoundsFromDerivates','tcu::clampLodBounds','tcu::isLookupResultValid(tcu::Texture2DView'):
   if symbol not in symbols:raise ValueError('actual CTS verifier not linked')
  for name,digest in inputs.items():
   if sha(name)!=digest:raise ValueError('source/actual raw changed during offline verification: '+name)
  proof.update(passed=True,reference_result=result,original_evidence=str(a.evidence.resolve()),original_evidence_sha256=sha(a.evidence),original_exact_oracle_passed=legacy_error is None,original_exact_oracle_error=legacy_error,original_error_record_unchanged=True,actual_raw_files=576,actual_raw_bytes=sum(map(len,captured.values())),actual_guard_bytes=guards,actual_raw_SHA={k:v for k,v in inputs.items()if k in {item['path']for item in evidence['copy_images'].values()}},CTS_commit=manifest['commit'],third_party_sources=len(manifest['files']),third_party_compiled_sources=len(objects),required_source_files=sorted(deps),independent_profile=manifest['HQ_profile'],scope='Independent offline GL-CTS HQ accuracy reference for actual authored native RGBA8 mip results; not an NVN specification or public/game claim. CTS rho approximation allows up to +0.5 LOD plus fixed-point/derivative bounds; no GPU fit is used.')
 except Exception as error:proof['error']=str(error)
 proof.update(inputs=inputs,artifacts={str(path):sha(path)for path in out.rglob('*')if path.is_file()});write(out/'check.json',proof);print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())
