#!/usr/bin/env python3
"""Bounded original container inventory + existing Maxwell/IR3 graphics compilation.
No address/identity whitelist, runtime compiler, draw template or test oracle.
"""
import argparse,hashlib,importlib.util,json,os,struct,subprocess,sys
from pathlib import Path

def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(path,name):
    s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def declared_containers(raw,bounded):
    """Existing v5 contract shared by NSO and decoded resource inventories.
    Returns metadata only; unsupported native stages remain explicit records.
    Unrelated magic bytes are not promoted to validated containers.
    """
    pos=0
    while True:
        pos=raw.find(bytes.fromhex('34127698'),pos)
        if pos<0:break
        co=pos;pos+=4
        if not bounded(co,0x708):continue
        end,total=struct.unpack_from('<II',raw,co+0x700);b=co-total-20
        if not (0x80<=end<=total<=1048576 and total%8==0 and bounded(b,total+20)):continue
        v,n,cr,nb,br=struct.unpack_from('<IIiIi',raw,b)
        if v!=5 or n not in (0x880,0x898) or br!=4 or nb!=total or b+8+cr!=co or b+20+total!=co or not bounded(co,n):continue
        code=raw[b+20:b+20+end]
        if code[:4]!=bytes.fromhex('78563412'):continue
        st=(struct.unpack_from('<I',code,0x30)[0]>>10)&15
        yield {'container_offset':b,'container_version':v,'code_offset':b+20,'code_bytes':end,'caller_code_bytes':total,'c1_offset':b+20+end,'c1_bytes':total-end,'control_offset':co,'control_bytes':n,'native_stage':st,'stage':{1:'vs',5:'fs'}.get(st),'supported_stage':st in(1,5)}

def extract(root,nso,verification,states,out):
    if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('use fresh ignored output')
    record=json.loads(verification.read_text());raw_path=verification.parent/'main.raw';raw=raw_path.read_bytes();nso_bytes=nso.read_bytes()
    if record.get('segment_hashes_verified') is not True or sha(nso)!=record['input_sha256'] or sha(raw_path)!=record['raw_sha256'] or nso_bytes[:4]!=b'NSO0':raise ValueError('original verification changed')
    segments=[struct.unpack_from('<III',nso_bytes,0x10+i*16)[1:] for i in range(3)]
    def bounded(o,n):return any(o>=b and o-b<=s and n<=s-(o-b) for b,s in segments)
    for i,(base,size) in enumerate(segments):
        if hashlib.sha256(raw[base:base+size]).digest()!=nso_bytes[0xa0+32*i:0xc0+32*i]:raise ValueError('NSO segment mismatch')
    profile=json.loads(states.read_text());out.mkdir(parents=True);entries=[]
    for item in declared_containers(raw,bounded):
        if not item['supported_stage']:continue
        b=item['container_offset'];co=item['control_offset'];end=item['code_bytes'];total=item['caller_code_bytes'];n=item['control_bytes'];v=item['container_version'];stage=item['stage']
        code=raw[b+20:b+20+end];control=raw[co:co+n];c1=raw[b+20+end:co];key=hashlib.sha256(control).hexdigest()[:16]+'-'+format(b,'x')
        blobs={}
        for name,data in [('code',code),('control',control),('c1',c1)]:
            f=out/(key+'.'+name+'.bin');f.write_bytes(data);blobs[name]={'path':f.name,'offset':0,'size':len(data),'sha256':sha(f)}
        doc={'schema':1,'stage':stage,'profile':{'name':'embedded-graphics-container-v5','control_extent':{'kind':'caller-container-declared','bytes':n,'container_version':v}},'compiler':profile['compiler'],'state':profile['states'][stage],'blobs':blobs,
             'provenance':{'nso_sha256':sha(nso),'raw_sha256':sha(raw_path),'container_offset':b,'code_offset':b+20,'control_offset':co,'caller_code_bytes':total,'caller_control_bytes':n,'extractor_sha256':sha(__file__),'state_profile_sha256':sha(states),'state_basis':'explicit specialization input; not captured draw state'}}
        fixture=out/(key+'.fixture.json');fixture.write_text(json.dumps(doc,indent=2)+'\n')
        entries.append({'fixture':str(fixture),'fixture_sha256':sha(fixture),'stage':stage,'container_offset':b,'code_bytes':end,'c1_bytes':len(c1),'control_bytes':n})
    result={'schema':1,'kind':'declared-graphics-container-inventory-v1','source_inputs':{str(nso):sha(nso),str(verification):sha(verification),str(raw_path):sha(raw_path),str(states):sha(states),str(Path(__file__).resolve()):sha(__file__)},'entries':entries,'scope':'bounded embedded container inventory; not all dynamically loaded game shaders'}
    (out/'inventory.json').write_text(json.dumps(result,indent=2)+'\n');return out/'inventory.json'

def compile_all(root,inventory,frontend,ir3,tools,out,unit_render_scale=False,pair_keys=None):
    if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('use fresh ignored output')
    inv=json.loads(inventory.read_text());deps={str(inventory):sha(inventory),str(Path(__file__).resolve()):sha(__file__),**inv['source_inputs']}
    def verify(p,h):
        if sha(p)!=h:raise ValueError('changed input: '+str(p))
    for p,h in deps.items():verify(p,h)
    own=load(root/'nvn-native/tools/compute_driverparams.py','own_common');ir,extra,_,count=own.compiler_inputs(root,ir3);deps.update(extra)
    front=json.loads((frontend/'build-record.json').read_text());deps[str(frontend/'build-record.json')]=sha(frontend/'build-record.json')
    for mapping,base in [(front['sources'],root),(front['artifacts'],None),(front['restore_assets'],None)]:
        for p,h in mapping.items():
            path=(base/p if base else Path(p));verify(path,h);deps[str(path.resolve())]=h
    for p,h in front['compiler_overlay'].items():
        if p!='original_sha256':verify(p,h);deps[p]=h
    dotnet=Path(front['commands'][0][0]);verify(dotnet,front['dotnet_sha256']);deps[str(dotnet)]=sha(dotnet)
    for name in ['spirv-val','spirv-dis']:deps[str(tools/name)]=sha(tools/name)
    fixture_validator=load(root/'nvn-native/tools/shader_fixture.py','fixture');deps[str(root/'nvn-native/tools/shader_fixture.py')]=sha(root/'nvn-native/tools/shader_fixture.py')
    out.mkdir(parents=True);stages={};pairs=[];commands=[];env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip')
    def run(args):
        r=subprocess.run(list(map(str,args)),env=env,text=True,capture_output=True);commands.append({'argv':list(map(str,args)),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
    for e in inv['entries']:
        fixture=Path(e['fixture']);verify(fixture,e['fixture_sha256']);d=json.loads(fixture.read_text());envelope=fixture_validator.validate(d,fixture.parent);key=envelope['request_sha256']
        if key in stages:
            stages[key]['containers'].append(e['container_offset']);stages[key]['source_ids'].append(e.get('source_id',str(e['container_offset'])));continue
        prefix=out/key[:16];r=run([dotnet,front['entrypoint'],fixture,prefix]);entry={'fixture':str(fixture),'fixture_sha256':sha(fixture),'envelope':envelope,'stage':d['stage'],'prefix':str(prefix),'containers':[e['container_offset']],'source_ids':[e.get('source_id',str(e['container_offset']))],'frontend_passed':False,'failure':r.stderr}
        stages[key]=entry
        if r.returncode:continue
        r=run([tools/'spirv-val',str(prefix)+'.spv']);entry['frontend_passed']=r.returncode==0;entry['failure']=r.stderr
        if r.returncode==0:entry['resources']=json.loads(Path(str(prefix)+'.resources.json').read_text())
    for vk,vs in stages.items():
        if vs['stage']!='vs' or not vs['frontend_passed']:continue
        for fk,fs in stages.items():
            if fs['stage']!='fs' or not fs['frontend_passed']:continue
            if pair_keys is not None and not any(a in vs['source_ids'] and b in fs['source_ids'] for a,b in pair_keys):continue
            prefix=out/(vk[:8]+'-'+fk[:8]);r=run([ir3/'build/src/freedreno/fdl/nvn-graphics-ir3',*(['--unit-render-scale']if unit_render_scale else []),vs['prefix']+'.spv',fs['prefix']+'.spv',prefix,0,'0x06050002']);pair={'vs':vk,'fs':fk,'passed':r.returncode==0,'prefix':str(prefix),'failure':r.stderr}
            if r.returncode==0:pair['program']=json.loads(Path(str(prefix)+'.link.json').read_text())
            pairs.append(pair)
    artifacts={str(p):sha(p) for p in out.iterdir() if p.is_file()}
    for p,h in deps.items():verify(p,h)
    result={'schema':1,'kind':'offline-graphics-stage-compilation-v1','gpu_execution':False,'draw_pipeline_available':False,'native_backend_policy':{'unit_render_scale':unit_render_scale},'chip_id':0x06050002,'inventory':inv,'frontend_build':front,'ir3_build':ir,'compiler_source_files':count,'inputs':deps,'artifacts':artifacts,'stages':stages,'pairs':pairs,'commands':commands,'pair_selection':pair_keys}
    (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'

def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--root',type=Path,required=True);a.add_argument('--out',type=Path,required=True);s=a.add_subparsers(dest='mode',required=True)
    e=s.add_parser('extract');e.add_argument('--nso',type=Path,required=True);e.add_argument('--verification',type=Path,required=True);e.add_argument('--states',type=Path,required=True)
    c=s.add_parser('compile');c.add_argument('--inventory',type=Path,required=True);c.add_argument('--frontend',type=Path,required=True);c.add_argument('--ir3',type=Path,required=True);c.add_argument('--spirv-tools',type=Path,required=True);c.add_argument('--unit-render-scale',action='store_true',help='explicit source/native pixel identity policy; not guest CB0')
    args=a.parse_args();root=args.root.resolve();out=args.out.resolve()
    print(extract(root,args.nso.resolve(),args.verification.resolve(),args.states.resolve(),out) if args.mode=='extract' else compile_all(root,args.inventory.resolve(),args.frontend.resolve(),args.ir3.resolve(),args.spirv_tools.resolve(),out,args.unit_render_scale))
if __name__=='__main__':main()
