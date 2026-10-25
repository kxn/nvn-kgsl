"""Synthetic metadata/oracle/runner tests. Every subprocess is mocked: NO ADB."""
import contextlib,copy,hashlib,importlib.util,io,json,math,os,re,shutil,subprocess,sys,tempfile,types,unittest
from pathlib import Path
from unittest.mock import patch
NATIVE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(NATIVE/'tools'))
import copy_validation as cv

def load(name):
 spec=importlib.util.spec_from_file_location('copy_test_'+name,NATIVE/'tools'/(name+'.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def check():return {'modes':True,'program':{'chip_id':0x06050002},'image':{'schema':3},'inputs':{s:{'envelope':{'inputs':{'code':{'size':n}}}} for s,n in [('vs',512),('fs',256)]}}
def data(nonce):
 d={'schema':1,'operation':'nvn_gpu_copy','chip_id':0x06050002,'image_schema':3,'stage':'complete','errno':0,'cleanup_errno':0,'passed':True,'resources_retained':False,
 'completed_cases':8,'channel_checks':131072,'nvn_submissions_observed':8,'copy_submissions_observed':8,'copy_submissions':8,'copy_finishes':8,'intermediate_producer_finishes':4,
 'source_import_observations':1,'source_map_rejections':9,'source_private_vma_verified':True,'source_backing_replaced':False,'source_pool_cpu_no_access':True,'source_cpu_reads':0,
 'source_cpu_writes_after_import':0,'source_offset':4096,'source_pool_bytes':24576,'target_cpu_writes':0,'cpu_image_copy':False,'raw_draw_calls':0,'readback_locks':8,'readback_unlocks':8,
 'identification_read_locks':1,'identification_read_unlocks':1,'owner_pin_calls':8,'owner_unpin_calls':8,'copy_command_readonly_checks':786592,'copy_command_guard_checks':32608,
 'source_guard_gpu_checked':False,'padded_stride_gpu_checked':False,'window_presented':False,'game_modules_executed':False,'run_nonce':nonce,'run_directory':'copy-'+nonce,
 'uid':10234,'source_gpu_address':0x10001000,'input_readonly_checks':294912,'bad_mode':0xffffffff,'bad_case':0xffffffff,'bad_x':0xffffffff,'bad_y':0xffffffff,'bad_channel':0xffffffff,'bad_actual':0,'bad_expected':0,
 'target':{'fd_at_import':7,'fd_index':0,'fd_bytes':16384,'pixel_offset':0,'inode':123,'device_major':0,'device_minor':1,'pitch':256}}
 # source shader size: VS at4096, FS at12288, 20480 total, plus3*4096.
 d['input_readonly_checks']=8*(20480+12288)
 return d

def images():
 result={}
 for mode in range(2):
  for case in range(4):
   raw=bytearray()
   for y in range(64):
    for x in range(64):
     rgb=(255,0,0)
     if case==1:rgb=(0,255,0) if x>=32 else (0,0,0)
     if case==2:rgb=(0,0,0)
     if case==3:
      g=(x+.5)/128;b=(y+.5)/128;rgb=tuple(math.floor(v*255+.5) for v in (1-g-b,g,b))
     raw.extend((*rgb,255))
   result[f'copy-mode-{mode}-case-{case}.rgba']=bytes(raw)
 return result
IMAGES=images();NONCE='0123456789abcdef0123456789abcdef'
class Validator(unittest.TestCase):
 def test_normal(self):self.assertEqual(cv.validate(data(NONCE),IMAGES,NONCE,check(),10234)['channel_checks'],131072)
 def test_mutants(self):
  variants=[('bool-count',lambda d:d.update(completed_cases=True)),('float-uid',lambda d:d.update(uid=10234.0)),('uid-mismatch',lambda d:d.update(uid=10235)),('nonce',lambda d:d.update(run_nonce='f'*32)),('directory',lambda d:d.update(run_directory='copy-old')),('retained',lambda d:d.update(resources_retained=True)),('failed',lambda d:d.update(passed=False)),('cpu-target',lambda d:d.update(target_cpu_writes=1)),('source-read',lambda d:d.update(source_cpu_reads=1)),('rawdraw',lambda d:d.update(raw_draw_calls=1)),('stale-counter',lambda d:d.update(copy_submissions=7)),('bad-case',lambda d:d.update(bad_case=0)),('bad-pitch',lambda d:d['target'].update(pitch=320)),('bool-index',lambda d:d['target'].update(fd_index=False)),('index-bound',lambda d:d['target'].update(fd_index=8)),('inode',lambda d:d['target'].update(inode=0)),('devtype',lambda d:d['target'].update(device_major='0')),('fdsize',lambda d:d['target'].update(fd_bytes=16385)),('unaligned-offset',lambda d:d['target'].update(pixel_offset=4,fd_bytes=20480)),('gpuva-overflow',lambda d:d.update(source_gpu_address=(1<<64)-64)),('bad-input-count',lambda d:d.update(input_readonly_checks=1))]
  for name,mutate in variants:
   with self.subTest(name=name):
    d=data(NONCE);mutate(d)
    with self.assertRaises((ValueError,TypeError)):cv.validate(d,IMAGES,NONCE,check(),10234)
 def test_readback_mutants(self):
  for name in ('missing','extra','short','changed','wrong-mode-content'):
   with self.subTest(name=name):
    im=IMAGES.copy()
    if name=='missing':im.pop(cv.NAMES[0])
    if name=='extra':im['extra.rgba']=b''
    if name=='short':im[cv.NAMES[0]]=im[cv.NAMES[0]][:-1]
    if name=='changed':im[cv.NAMES[0]]=b'\0'+im[cv.NAMES[0]][1:]
    if name=='wrong-mode-content':im[cv.NAMES[-1]]=im[cv.NAMES[0]]
    with self.assertRaises(ValueError):cv.validate(data(NONCE),im,NONCE,check(),10234)
 def test_duplicate_json(self):
  with self.assertRaises(ValueError):cv.parse_result('{"passed":true,"passed":false}')
  with self.assertRaises(ValueError):cv.parse_result('[]')

class BuildArguments(unittest.TestCase):
 def test_copy_is_exclusive_before_tools(self):
  mod=load('build_android_probe')
  modes=[['--nvn-api'],['--cs-check','x'],['--maxwell-check','x'],['--graphics-check','x'],['--nvn-graphics-check','x'],['--nvn-external-image-check','x'],['--nvn-window-check','x'],['--texture-check','x'],['--nvn-texture-check','x'],['--device-inspect'],['--iocoherent-inspect'],['--ahb-inspect'],['--ahb-graphics-check','x'],['--ahb-present-check','x'],['--d2r-host-record','x'],['--texture-descriptor-flags','0x12'],['--texture-modes'],['--texture-noaccess-pool'],['--texture-uncached-pools'],['--texture-device-flags','0x100'],['--window-slots','2'],['--external-image-slots','2']]
  for flags in modes:
   with self.subTest(flags=flags),patch.object(sys,'argv',['build','--sdk','missing','--ndk','missing','--out','missing','--nvn-copy-check','missing',*flags]),patch.object(subprocess,'run') as run,contextlib.redirect_stderr(io.StringIO()):
    with self.assertRaises(SystemExit) as error:mod.main()
    self.assertEqual(error.exception.code,2);run.assert_not_called()

class ExistingDescriptorAudit(unittest.TestCase):
 def test_shared_ubo_eight_exclusions(self):
  mod=load('run_android_probe')
  for flags in (0x12,0x14):
   for modes in (False,True):
    n=17 if modes else 5
    record={'texture_descriptor_flags':flags,'nvn_texture_check':{},'texture_modes':modes}
    record['nvn_texture_check']={'checked':True}
    d={'descriptor_software_tables':True,'descriptor_metadata_flags':flags,'descriptor_metadata_map_checks':4,'descriptor_metadata_ranges':4,'descriptor_audit_attempts':n,'descriptor_audit_delegated':n,'descriptor_audit_accepted':n,'descriptor_audit_rejections':0,'descriptor_audit_errno':0,'descriptor_excluded_object_checks':8*n,'descriptor_private_jobs':n,'descriptor_private_base_checks':4*n,'descriptor_metadata_invalidate_skips':4*n,'public_cache_calls':1+14*n}
    mod.validate_descriptor_audit(record,d)
    d['descriptor_excluded_object_checks']=9*n
    with self.assertRaises(RuntimeError):mod.validate_descriptor_audit(record,d)

class Runner(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.base=Path(self.temp.name)
  self.native=self.base/'native';self.build=self.base/'build';self.build.mkdir();(self.native/'tools').mkdir(parents=True)
  for name in ('run_android_probe.py','copy_validation.py'):shutil.copyfile(NATIVE/'tools'/name,self.native/'tools'/name)
  self.compiler=self.base/'compiler';self.compiler.write_bytes(b'MOCK TOOL')
  self.pm4=self.build/'adreno_pm4.xml.h';self.pm4.write_bytes(b'MOCK INPUT')
  self.apk=self.build/'probe.apk';self.apk.write_bytes(b'MOCK APK NOT EXECUTABLE')
  sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
  self.record={'source':{str(p.relative_to(self.native)):sha(p) for p in self.native.rglob('*') if p.is_file()},'tools':{str(self.compiler):sha(self.compiler)},'compiled_inputs':{str(self.pm4):sha(self.pm4)},'artifacts':{str(self.apk):sha(self.apk)},'nvn_copy_check':check(),'texture_descriptor_flags':0}
  self.record_path=self.build/'build-record.json';self.record_path.write_text(json.dumps(self.record));self.evidence=self.base/'evidence.json'
  self.runner=load('run_android_probe');self.runner.__file__=str(self.native/'tools/run_android_probe.py');self.variant='normal';self.commands=[];self.finds=0
 def fake(self,argv,**kw):
  args=list(map(str,argv));self.commands.append(args);self.assertEqual(args[:3],['adb','-s','MOCK']);a=args[3:];text=kw.get('text',False)
  rc=0;out='';err=''
  if a[:3]==['shell','pm','path']:out='package:/data/app/mock/base.apk\n'
  elif a[:2]==['shell','sha256sum']:out=hashlib.sha256(self.apk.read_bytes()).hexdigest()+'  /data/app/mock/base.apk\n'
  elif a[:3]==['shell','am','force-stop']:pass
  elif a[:2]==['shell','pidof']:
   rc=1
   if self.variant=='still-alive':out='321\n';rc=0
   if self.variant=='pidof-transport':rc=1;err='error: device offline\n'
   if self.variant=='pidof-unavailable':rc=127;err='pidof: not found\n'
   if self.variant=='pidof-empty-success':rc=0
   if self.variant=='pidof-error':rc=2
  elif a[:4]==['shell','run-as','org.nvn.nativeprobe','rm']:pass
  elif len(a)==2 and a[0]=='shell' and 'copy-run-nonce' in a[1]:self.assertIn(NONCE,a[1]);self.assertIn('test ! -L',a[1])
  elif a[:3]==['shell','am','start']:out='Status: ok\n'
  elif a[:4]==['shell','run-as','org.nvn.nativeprobe','cat']:
   d=data(NONCE)
   if self.variant=='failed':d.update(passed=False,errno=5,stage='copy-finish',resources_retained=True,cleanup_errno=16)
   if self.variant=='wrong-uid':d['uid']=10235
   out=json.dumps(d)
   if self.variant=='duplicate-json':out=out[:-1]+',"passed":true}'
  elif a[:3]==['shell','getprop','ro.build.fingerprint']:out='MOCK ONLY'
  elif a[:3]==['shell','uname','-r']:out='MOCK ONLY'
  elif a[:5]==['shell','cmd','package','list','packages']:out='package:org.nvn.nativeprobe uid:10234\n'
  elif a[:4]==['shell','run-as','org.nvn.nativeprobe','stat']:out='symbolic link\n' if self.variant=='namespace-symlink' else 'directory\n'
  elif a[:4]==['shell','run-as','org.nvn.nativeprobe','find']:
   regular='-type' in a;rows=['files/copy-'+NONCE+'/'+n for n in cv.NAMES]
   if not regular:self.finds+=1
   if self.variant in ('missing','failed'):rows=rows[:-1]
   if self.variant=='extra-dir' and not regular:rows+=['files/copy-'+NONCE+'/extra-dir']
   if self.variant=='symlink' and regular:rows=rows[:-1]
   if self.variant=='changed-inventory' and self.finds>1:rows=rows[:-1]
   if self.variant=='duplicate-entry' and not regular:rows+=[rows[0]]
   out='\n'.join(rows)+'\n'
  elif a[:3]==['exec-out','run-as','org.nvn.nativeprobe']:
   n=Path(a[-1]).name;out=IMAGES[n]
   if self.variant=='bad-pixel' and n==cv.NAMES[0]:out=b'\0'+out[1:]
  else:self.fail('unmocked subprocess (MUST NEVER EXECUTE): '+repr(args))
  if not text and isinstance(out,str):out=out.encode()
  return subprocess.CompletedProcess(args,rc,stdout=out,stderr=err if text else err.encode())
 def invoke(self):
  argv=['runner','--serial','MOCK','--build',str(self.build),'--evidence',str(self.evidence)]
  with patch.object(sys,'argv',argv),patch.object(self.runner.subprocess,'run',side_effect=self.fake),patch.object(self.runner.uuid,'uuid4',return_value=types.SimpleNamespace(hex=NONCE)),contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):self.runner.main()
 def test_positive(self):
  self.invoke();d=json.loads(self.evidence.read_text());self.assertTrue(d['copy_verified']);self.assertTrue(d['probe_stopped']);self.assertEqual(len(d['copy_images']),8)
 def test_runner_negatives(self):
  for variant in ('missing','extra-dir','symlink','namespace-symlink','changed-inventory','duplicate-entry','bad-pixel','wrong-uid','failed','duplicate-json','still-alive'):
   with self.subTest(variant=variant):
    self.setUp();self.variant=variant
    with self.assertRaises((RuntimeError,ValueError)):self.invoke()
    d=json.loads(self.evidence.read_text());self.assertFalse(d.get('copy_verified',False))
    self.assertEqual(d['probe_stopped'],variant!='still-alive')
    self.assertTrue(any(a[3:6]==['shell','am','force-stop'] for a in self.commands))
    if variant not in ('namespace-symlink',):self.assertTrue(d.get('copy_images'))
 def test_stop_check_rejects_unproven_absence(self):
  for variant in ('pidof-transport','pidof-unavailable','pidof-empty-success','pidof-error'):
   with self.subTest(variant=variant):
    self.setUp();self.variant=variant
    with self.assertRaisesRegex(RuntimeError,'pidof did not prove'):self.invoke()
    d=json.loads(self.evidence.read_text());self.assertIs(d['probe_stopped'],False);self.assertIs(d['copy_verified'],False)
    self.assertEqual(len(d['copy_images']),8);self.assertIn('pidof did not prove',d['stop_error'])

 def test_stale_readback_before_adb(self):
  (self.base/('evidence-'+cv.NAMES[0])).write_bytes(b'OLD')
  with self.assertRaises(SystemExit):self.invoke()
  self.assertFalse(self.commands)
 def test_compiled_input_mismatch_before_adb(self):
  self.pm4.write_bytes(b'CHANGED')
  with self.assertRaises(SystemExit):self.invoke()
  self.assertFalse(self.commands)
 def test_artifact_mismatch_before_adb(self):
  self.apk.write_bytes(b'CHANGED')
  with self.assertRaises(SystemExit):self.invoke()
  self.assertFalse(self.commands)

if __name__=='__main__':unittest.main()
