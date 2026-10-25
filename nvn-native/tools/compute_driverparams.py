#!/usr/bin/env python3
"""Own SPIR-V NumWorkgroups qualifier producer/bundle; HOST compile only.

Uses the existing standalone compiler and Ninja dependency inventory. No game
shader identity, Maxwell envelope, Vulkan driver or runtime compiler is involved.
"""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path

def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def compiler_inputs(root,build):
    record_path=build/'build-record.json';record=json.loads(record_path.read_text());inputs={}
    def add(path,expected=None):
        path=Path(path).resolve();v=sha(path)
        if expected is not None and v!=expected:raise ValueError('compiler dependency changed: '+str(path))
        inputs[str(path)]=v;return path
    add(record_path);source=build/'source/mesa-26.2.5';obj=build/'build'
    manifest_path=add(build/'source-hashes.json',record['source_manifest_sha256']);manifest=json.loads(manifest_path.read_text())
    add(root/'nvn-native/tools/build_ir3.py',record['builder_sha256'])
    for p,v in record['artifacts'].items():add(p,v)
    for p,v in record['tools'].items():add(p,v)
    for p,v in record['source_rules'].items():add(root/'nvn-native'/p,v)
    for p,v in record['overlay'].items():add(source/p,v)
    ninja=next(Path(p) for p in record['tools'] if Path(p).name=='ninja')
    deps=subprocess.run([str(ninja),'-C',str(obj),'-t','deps'],check=True,text=True,capture_output=True).stdout
    commands=subprocess.run([str(ninja),'-C',str(obj),'-t','commands','src/freedreno/fdl/nvn-spirv-ir3'],check=True,text=True,capture_output=True).stdout
    paths={(obj/line.strip()).resolve() for line in deps.splitlines() if line.startswith('    ')}
    for line in commands.splitlines():
        for word in shlex.split(line):
            p=(obj/word).resolve()
            if p.is_file() and (p.is_relative_to(source) or p.is_relative_to(obj)):paths.add(p)
    count=0
    for p in sorted(paths):
        if p.is_relative_to(source):
            rel=str(p.relative_to(source))
            if (rel.startswith('src/') and '/vulkan/' in '/'+rel+'/') or rel.startswith('src/gallium/drivers/'):
                raise ValueError('forbidden driver in standalone compiler closure')
            if rel not in manifest:raise ValueError('compiler source absent from manifest')
            add(p,manifest[rel]);count+=1
        elif p.is_file():add(p)
    if not count:raise ValueError('empty actual compiler source inventory')
    compilers=[Path(p) for p in record['artifacts'] if Path(p).name=='nvn-spirv-ir3']
    if len(compilers)!=1:raise ValueError('ambiguous compiler artifact')
    return record,inputs,compilers[0],count

def produce(root,build,tools,source,out):
    if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('use fresh ignored output below ROOT/.local')
    out.mkdir(parents=True);record,inputs,compiler,count=compiler_inputs(root,build)
    source=source.resolve();inputs[str(source)]=sha(source);inputs[str(Path(__file__).resolve())]=sha(__file__)
    paths={n:tools/n for n in ['spirv-as','spirv-val','spirv-dis']}
    for p in paths.values():inputs[str(p)]=sha(p)
    prefix=out/'numworkgroups';env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip');commands=[]
    def run(argv):
        argv=list(map(str,argv));r=subprocess.run(argv,env=env,text=True,capture_output=True)
        commands.append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        if r.returncode:raise RuntimeError('controlled shader tool failed: '+str(argv))
    run([paths['spirv-as'],'--target-env','spv1.3',source,'-o',prefix.with_suffix('.spv')])
    run([paths['spirv-val'],'--target-env','spv1.3',prefix.with_suffix('.spv')])
    run([paths['spirv-dis'],prefix.with_suffix('.spv'),'-o',prefix.with_suffix('.spvasm')])
    run([compiler,prefix.with_suffix('.spv'),prefix,'0x06050002'])
    program=json.loads(prefix.with_suffix('.json').read_text());native=json.loads(prefix.with_suffix('.native.json').read_text())
    required={'chip_id':0x06050002,'local_size':[1,1,1],'num_uavs':1,'num_samplers':0,
              'ubo_table_slots':0,'ssbo_table_slots':1,'private_bytes':0,'shared_bytes':0}
    if any(program.get(k)!=v for k,v in required.items()) or program.get('driver_params_dwords',0)<3:raise ValueError('unexpected controlled CS reflection')
    if '@load_num_workgroups' not in prefix.with_suffix('.nir').read_text():raise ValueError('NumWorkgroups not consumed in NIR')
    if native.get('schema')!=1 or native.get('descriptor_fdl_checks')!=20:raise ValueError('unknown native compute descriptor contract')
    outputs={str(f.resolve()):sha(f) for f in sorted(out.iterdir()) if f.is_file()}
    check={'schema':1,'kind':'own-numworkgroups-driverparams-v1','scope':'SPIR-V/native compiler HOST ONLY; not Maxwell/NVN/game execution',
           'passed':True,'gpu_execution':False,'original_game_shader':False,'chip_id':0x06050002,
           'source':str(source),'source_sha256':sha(source),'compiler_build_path':str(build/'build-record.json'),
           'compiler_build':record,'compiler_source_files':count,'environment':{'IR3_SHADER_DEBUG':'asmroundtrip'},
           'commands':commands,'inputs':inputs,'artifacts':outputs,'program':program,'native':native,
           'prefix':str(prefix),'producer_sha256':sha(__file__)}
    for p,v in inputs.items():
        if sha(p)!=v:raise ValueError('source changed during controlled shader compilation')
    (out/'check.json').write_text(json.dumps(check,indent=2)+'\n');return out/'check.json'

def build(check_path,out,*,root=None):
    root=(root or Path(__file__).resolve().parents[2]).resolve();check_path=Path(check_path).resolve();out=Path(out).resolve()
    if not out.is_relative_to(root/'.local'):raise ValueError('bundle must stay ignored below ROOT/.local')
    c=json.loads(check_path.read_text());inputs={str(check_path):sha(check_path),str(Path(__file__).resolve()):sha(__file__)}
    if c.get('schema')!=1 or c.get('kind')!='own-numworkgroups-driverparams-v1' or c.get('passed') is not True or c.get('gpu_execution') is not False or c.get('original_game_shader') is not False or c.get('chip_id')!=0x06050002:raise ValueError('unknown own-shader check profile')
    if c['producer_sha256']!=sha(__file__):raise ValueError('controlled shader producer changed')
    source=Path(c['source']);canonical=root/'nvn-native/tests/shaders/numworkgroups.spvasm'
    if canonical.exists() and sha(canonical)!=c['source_sha256']:raise ValueError('controlled source differs from normal qualifier source')
    if sha(source)!=c['source_sha256']:raise ValueError('controlled source changed')
    record,actual,compiler,count=compiler_inputs(root,Path(c['compiler_build_path']).parent)
    if record!=c['compiler_build'] or count!=c['compiler_source_files'] or any(c['inputs'].get(p)!=v for p,v in actual.items()):raise ValueError('compiler check source closure differs')
    for mapping in [c['inputs'],c['artifacts']]:
        for p,v in mapping.items():
            if sha(p)!=v:raise ValueError('controlled shader input/product changed: '+p)
            inputs[p]=v
    prefix=Path(c['prefix']);p=json.loads(prefix.with_suffix('.json').read_text());n=json.loads(prefix.with_suffix('.native.json').read_text())
    if p!=c['program'] or n!=c['native'] or p['local_size']!=[1,1,1] or p['driver_params_dwords']<3:raise ValueError('controlled metadata changed')
    out.mkdir(parents=True,exist_ok=True);header=out/'numworkgroups.native.h';header.write_bytes(prefix.with_suffix('.native.h').read_bytes());inputs[str(header)]=sha(header)
    return header,c,inputs

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--ir3-build',type=Path,required=True);p.add_argument('--spirv-tools',type=Path,required=True);p.add_argument('--source',type=Path);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    source=a.source or a.root/'nvn-native/tests/shaders/numworkgroups.spvasm'
    print(produce(a.root.resolve(),a.ir3_build.resolve(),a.spirv_tools.resolve(),source,a.out.resolve()))
if __name__=='__main__':main()
