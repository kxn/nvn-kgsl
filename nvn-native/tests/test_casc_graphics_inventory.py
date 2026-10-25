#!/usr/bin/env python3
"""HOST ONLY v5 container/CASC bounds, identity and dedup tests; no shader execution."""
import argparse,hashlib,importlib.util,json,struct,zlib
from pathlib import Path

def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--root',type=Path,required=True);a.add_argument('--inventory',type=Path,required=True);a.add_argument('--out',type=Path,required=True);x=a.parse_args();root=x.root.resolve();out=x.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):a.error('fresh ignored out required')
 out.mkdir(parents=True);module=load(Path(__file__).resolve().parents[1]/'tools/casc_graphics_inventory.py','casc_inventory');common=load(Path(module.__file__).with_name('offline_graphics_stages.py'),'v5');reader=load(root/'dm/tools/casc_overlay.py','casc')
 inv=json.loads(x.inventory.read_text());inputs={str(x.inventory.resolve()):module.sha(x.inventory)};checks=negative=0
 def yes(v):
  nonlocal checks
  checks+=1
  if not v:raise AssertionError('check '+str(checks))
 def rejects(fn):
  nonlocal negative
  try:fn()
  except (ValueError,SystemExit):negative+=1;return
  raise AssertionError('negative accepted')
 sample=inv['entries'][0];fixture=Path(sample['fixture']);d=json.loads(fixture.read_text());yes(module.sha(fixture)==sample['fixture_sha256']);inputs[str(fixture)]=module.sha(fixture)
 # Build only a container-test wrapper around actual unmodified source bytes.
 # This synthetic wrapper is never passed to the translator or runtime.
 data={n:(fixture.parent/v['path']).read_bytes()for n,v in d['blobs'].items()};total=len(data['code'])+len(data['c1']);co=20+total
 raw=struct.pack('<IIiIi',5,len(data['control']),co-8,total,4)+data['code']+data['c1']+data['control']
 bounded=lambda o,n:0<=o<=len(raw)and 0<=n<=len(raw)-o
 found=list(common.declared_containers(raw,bounded));yes(len(found)==1);yes(found[0]['c1_bytes']==len(data['c1']));yes(found[0]['control_bytes']==len(data['control']))
 for offset,value in[(0,4),(4,0x879),(8,1),(12,total+8),(16,8),(20,0),(co+0x700,127),(co+0x704,total+8)]:
  bad=bytearray(raw);struct.pack_into('<I',bad,offset,value);yes(not list(common.declared_containers(bad,lambda o,n:0<=o<=len(bad)and 0<=n<=len(bad)-o)));negative+=1
 yes(not list(common.declared_containers(raw[:-1],lambda o,n:0<=o<=len(raw)-1 and 0<=n<=len(raw)-1-o)));negative+=1
 other=bytearray(raw);struct.pack_into('<I',other,20+0x30,3<<10);q=list(common.declared_containers(other,lambda o,n:0<=o<=len(other)and 0<=n<=len(other)-o));yes(len(q)==1 and q[0]['native_stage']==3 and not q[0]['supported_stage'])
 # The real existing CASC reader consumes a tiny zlib archive. Two logical
 # aliases must dedup the immutable full source, without a name whitelist.
 rom=out/'romfs';(rom/'data').mkdir(parents=True);enc=zlib.compress(raw);ekey=(hashlib.md5(enc).digest()[:9]+bytes([0,1])+(0).to_bytes(5,'big')).hex();ckey=hashlib.md5(raw).hexdigest();archive=rom/'data'/reader.data_filename(0,1);archive.write_bytes(enc)
 manifest=rom/'data/build_manifest.psv'
 def rows(length=len(enc),content_hash=ckey):
  manifest.write_text('\n'.join('|'.join([p,'0','0','3',content_hash,str(len(raw)),ekey,str(length),'z',p,'table'])for p in('data/hd/shaders/alias_v.bin','data/hd/shaders/another_v.bin'))+'\n')
 rows();states=out/'states.json';states.write_text(json.dumps({'compiler':d['compiler'],'states':{d['stage']:d['state']}}));selected=['data/hd/shaders/alias_v.bin','data/hd/shaders/another_v.bin']
 p=module.inventory(root,rom,states,out/'positive',selected);r=json.loads(p.read_text());yes(r['resource_count']==2);yes(len(r['entries'])==2 and r['unique_extracted_stages']==1);yes(r['entries'][0]['fixture']==r['entries'][1]['fixture']);yes(r['resources'][0]['ckey_md5']==ckey);yes(r['archives'][str(archive)]['sha256']==module.sha(archive))
 rows(len(enc)+1);rejects(lambda:module.inventory(root,rom,states,out/'bad-bounds',selected));rows(content_hash='00'*16);rejects(lambda:module.inventory(root,rom,states,out/'bad-ckey',selected));rows()
 rejects(lambda:module.inventory(root,rom,states,out/'bad-resource',['data/hd/shaders/absent.bin']));rejects(lambda:module.inventory(root,rom,states,out/'bad-id',selected,['00'*32]));rejects(lambda:module.inventory(root,rom,states,out/'positive',selected))
 rejects(lambda:module.inventory(root,rom,states,out/'id-without-resource',(),[r['entries'][0]['source_id']]))
 # A valid identity elsewhere in the archive is not a match inside the
 # requested resource subset; never silently emit fewer requested identities.
 other_enc=zlib.compress(bytes(other));second=hashlib.md5(other_enc).digest()[:9]+bytes([0,1])+len(enc).to_bytes(5,'big');archive.write_bytes(enc+other_enc)
 with manifest.open('a')as f:f.write('|'.join(['data/hd/shaders/foreign_v.bin','0','0','3',hashlib.md5(other).hexdigest(),str(len(other)),second.hex(),str(len(other_enc)),'z','data/hd/shaders/foreign_v.bin','table'])+'\n')
 allmeta=json.loads(module.inventory(root,rom,states,out/'all-meta').read_text())
 foreign_id=next(v['validated_containers'][0]['source_id']for v in allmeta['resources']if v['logical_resource'].endswith('foreign_v.bin'))
 rejects(lambda:module.inventory(root,rom,states,out/'id-outside-resources',selected,[r['entries'][0]['source_id'],foreign_id]))
 for p in[Path(__file__).resolve(),Path(module.__file__).resolve(),Path(common.__file__).resolve(),Path(reader.__file__).resolve()]:inputs[str(p)]=module.sha(p)
 for n,v in d['blobs'].items():inputs[str(fixture.parent/v['path'])]=v['sha256']
 result={'passed':True,'scope':__doc__,'gpu_execution':False,'checks':checks,'negative_cases':negative,'inputs':inputs,'artifacts':{str(p):module.sha(p)for p in out.rglob('*')if p.is_file()}}
 (out/'proof.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'passed':True,'checks':checks,'negative_cases':negative}));return 0
if __name__=='__main__':raise SystemExit(main())
