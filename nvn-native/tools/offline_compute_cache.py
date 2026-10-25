"""Build an optional finite native compute cache from verified offline outputs.

No shader identities, game addresses, CPU/GPU test oracles or runtime compiler.
Original inputs/generated code remain private ignored build artifacts.
"""
import hashlib, importlib.util, json, re, shlex, subprocess
from pathlib import Path


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build(check_path, out, *, root=None, verify=False):
    root = (root or Path(__file__).resolve().parents[2]).resolve()
    check_path, out = Path(check_path).resolve(), Path(out).resolve()
    if not out.is_relative_to(root / '.local'):
        raise ValueError('offline cache output must be ignored below ROOT/.local')
    inputs, outputs = {}, {}
    def checked(path, expected=None):
        path = Path(path).resolve()
        if not path.is_file(): raise ValueError('missing cache dependency: '+str(path))
        actual = sha(path)
        if expected is not None and actual != expected: raise ValueError('offline cache integrity mismatch: '+str(path))
        inputs[str(path)] = actual
        return path
    def inventory(mapping, *, base=None):
        if not isinstance(mapping, dict) or not mapping: raise ValueError('missing compiler inventory')
        for name, expected in mapping.items():
            checked((base / name) if base else Path(name), expected)
    def emit(name, data):
        path = out / name
        if verify:
            if not path.is_file() or path.read_bytes() != data: raise ValueError('generated cache artifact mismatch: '+str(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(data)
        outputs[str(path)] = hashlib.sha256(data).hexdigest()
        return path
    checked(check_path);checked(Path(__file__))
    check = json.loads(check_path.read_text())
    if check.get('schema') != 1 or check.get('chip_id') not in (0x06050002, 0x43030b00): raise ValueError('unsupported compute cache check/chip')
    cases = check.get('cases')
    if not isinstance(cases,list) or not 1 <= len(cases) <= 64: raise ValueError('compute cache requires1..64 explicit cases')
    checked(root/'nvn-native/tools/check_maxwell.py',check['check_source_sha256'])
    validator=checked(root/'nvn-native/tools/shader_fixture.py')
    spec=importlib.util.spec_from_file_location('nn_offline_shader_fixture',validator)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    front,ir3 = check['frontend_build'],check['ir3_build']
    inventory(front['sources'],base=root);inventory(front['artifacts']);inventory(front['restore_assets'])
    for name,value in front['compiler_overlay'].items():
        if name != 'original_sha256': checked(name,value)
    dotnet=Path(front['commands'][0][0]);checked(dotnet,front['dotnet_sha256'])
    front_dir=Path(front['entrypoint']).parents[4]
    checked(front_dir/'source-hashes.json',front['source_manifest_sha256'])
    front_record=checked(front_dir/'build-record.json')
    if json.loads(front_record.read_text())!=front: raise ValueError('frontend record differs from checked compiler')
    inventory(ir3['artifacts']);inventory(ir3['tools']);inventory(ir3['source_rules'],base=root/'nvn-native')
    checked(root/'nvn-native/tools/build_ir3.py',ir3['builder_sha256'])
    compiler_paths=[Path(p) for p in ir3['artifacts'] if Path(p).name=='nvn-spirv-ir3']
    if len(compiler_paths)!=1: raise ValueError('missing unique recorded compute compiler')
    ir3_dir=compiler_paths[0].parents[4];source=ir3_dir/'source/mesa-26.2.5';build_dir=ir3_dir/'build'
    ir3_record=checked(ir3_dir/'build-record.json')
    if json.loads(ir3_record.read_text())!=ir3: raise ValueError('IR3 record differs from checked compiler')
    manifest_path=checked(ir3_dir/'source-hashes.json',ir3['source_manifest_sha256']);manifest=json.loads(manifest_path.read_text())
    inventory(ir3['overlay'],base=source)
    # Ask the existing build tool for actual C/C++ source/header dependencies;
    # do not inspect or load any driver runtime outside the standalone compiler.
    ninja=next((Path(p) for p in ir3['tools'] if Path(p).name=='ninja'),None)
    if ninja is None: raise ValueError('recorded ninja tool required')
    dependencies=subprocess.run([str(ninja),'-C',str(build_dir),'-t','deps'],check=True,text=True,capture_output=True).stdout
    commands=subprocess.run([str(ninja),'-C',str(build_dir),'-t','commands','src/freedreno/fdl/nvn-spirv-ir3'],check=True,text=True,capture_output=True).stdout
    compiler_files=set()
    for line in dependencies.splitlines():
        if line.startswith('    '): compiler_files.add((build_dir/line.strip()).resolve())
    for line in commands.splitlines():
        for word in shlex.split(line):
            path=(build_dir/word).resolve()
            if path.is_file() and (path.is_relative_to(source) or path.is_relative_to(build_dir)): compiler_files.add(path)
    source_count=0
    for path in sorted(compiler_files):
        if path.is_relative_to(source):
            relative=str(path.relative_to(source))
            if (relative.startswith('src/') and '/vulkan/' in '/'+relative+'/') or relative.startswith('src/gallium/drivers/'):
                raise ValueError('driver runtime unexpectedly present in standalone compiler dependency closure')
            if relative not in manifest: raise ValueError('unrecorded compiler source: '+relative)
            checked(path,manifest[relative]);source_count+=1
        elif path.is_relative_to(build_dir): checked(path)
        elif path.is_file(): checked(path) # declared system compiler headers
    if not source_count: raise ValueError('empty actual compiler dependency closure')
    for tool in check['spirv_tools'].values(): checked(tool['path'],tool['sha256'])
    lines=['/* Generated finite offline compute cache; no runtime compiler or test oracle. */','#pragma once','#include "nvn_native/nvn.h"']
    entries,identities,images,symbols=[],set(),{},set()
    summaries=[]
    for index,case in enumerate(cases):
        fixture=checked(case['fixture'],case['fixture_sha256']);document=json.loads(fixture.read_text())
        envelope=module.validate(document,fixture.parent)
        if envelope!=case['fixture_check'] or document['stage']!='cs' or document['state']['precision']!={'mode':'strict'}: raise ValueError('compute envelope/stage/precision changed')
        if document['profile'] != {'name': 'nvn-embedded-cs-v5-full-control', 'control_extent': {'kind': 'caller-container-declared', 'bytes': 0x898, 'container_version': 5}} or envelope['inputs']['control']['size'] != 0x898: raise ValueError('truncated/unknown full-control provenance profile')
        if envelope['inputs']['c1']['size']!=0: raise ValueError('nonempty C1 is unqualified')
        prefix=check_path.parent/envelope['request_sha256'][:16]
        for suffix,expected in case['artifacts'].items(): checked(Path(str(prefix)+suffix),expected)
        required={'.spv','.bin','.json','.resources.json','.native.h','.native.json','.import.nir','.nir','.asm'}
        if set(case['artifacts'])!=required: raise ValueError('incomplete compute artifacts')
        resources=json.loads(Path(str(prefix)+'.resources.json').read_text());program=json.loads(Path(str(prefix)+'.json').read_text());native=json.loads(Path(str(prefix)+'.native.json').read_text())
        if resources!=case['resources'] or program!=case['program']: raise ValueError('compiler metadata changed')
        if (resources.get('schema')!=1 or resources.get('stage')!='cs' or resources.get('textures')!=[] or resources.get('samplers')!=[] or
            resources.get('code_sha256')!=envelope['inputs']['code']['sha256'] or resources.get('spv_sha256')!=case['artifacts']['.spv']): raise ValueError('unqualified compute resources')
        if (program.get('schema')!=1 or program.get('chip_id')!=check['chip_id'] or program.get('local_size')!=document['state']['compute']['local_size'] or program.get('num_samplers')!=0 or
            program.get('binary_bytes')!=Path(str(prefix)+'.bin').stat().st_size or program.get('shared_bytes')!=document['state']['compute']['shared_bytes'] or program.get('private_bytes')!=document['state']['compute']['local_bytes'] or
            native.get('schema')!=1 or not 0<native.get('command_words',0)<=1024 or not 0<native.get('patches',0)<=native['command_words']): raise ValueError('native compute schema/chip/state mismatch')
        for query in resources['queries']:
            name=query['name'];state=document['state']
            if name=='binding':
                collection=resources.get({'cbuf':'cbufs','sbuf':'sbufs'}.get(query.get('kind'),''))
                if collection is None or query.get('SetIndex')!=0 or not any(item['Slot']==query['index'] and item['Binding']==query['Binding'] for item in collection): raise ValueError('unqualified binding query')
                continue
            if name=='local_size': answer=state['compute']['local_size'][query['axis']]
            elif name in ('shared_bytes','local_bytes'): answer=state['compute'][name]
            elif name in state['host']: answer=state['host'][name]
            elif name in state['resources'].get('queries',{}): answer=state['resources']['queries'][name]
            else: raise ValueError('unresolved compute query: '+name)
            if type(answer)!=type(query['value']) or answer!=query['value']: raise ValueError('compute query changed: '+name)
        blobs={}
        for name,desc in document['blobs'].items():
            path=checked(fixture.parent/desc['path'])
            with path.open('rb') as stream:stream.seek(desc['offset']);data=stream.read(desc['size'])
            if hashlib.sha256(data).hexdigest()!=desc['sha256']:raise ValueError('source blob changed')
            blobs[name]=data
        code,control=blobs['code'],blobs['control']
        if not 0<len(code)<=1048576 or len(control)!=0x898 or int.from_bytes(control[:4],'little')!=0x98761234: raise ValueError('unqualified source/control profile')
        if (int.from_bytes(control[0x700:0x704],'little')!=len(code) or int.from_bytes(control[0x704:0x708],'little')!=len(code) or
            [int.from_bytes(control[0x748+i*4:0x74c+i*4],'little') for i in range(3)]!=program['local_size'] or int.from_bytes(control[0x754:0x758],'little')!=program['shared_bytes']): raise ValueError('control/code/C1/compute state mismatch')
        identity=(envelope['inputs']['code']['sha256'],envelope['inputs']['control']['sha256'])
        if identity in identities:raise ValueError('duplicate/ambiguous full source identity')
        identities.add(identity)
        source_header=Path(str(prefix)+'.native.h');source_data=source_header.read_bytes();header_hash=sha(source_header)
        macros=re.findall(rb'^#define NVN_COMPILED_IMAGE ([A-Za-z_][A-Za-z_0-9]*)$',source_data,re.M)
        if len(macros)!=1:raise ValueError('missing unique native image symbol')
        symbol=macros[0].decode()
        # Read the existing compiler-generated C data grammar, not shader
        # instructions: ensure native image bytes/metadata agree with .bin/json.
        image_text=source_data.decode('ascii')
        body=re.search(r'static const struct nn_cs_image '+re.escape(symbol)+r' = \{(.*?)\};',image_text,re.M|re.S)
        if body is None:raise ValueError('missing native image definition')
        image_body=body.group(1)
        def number(field):
            value=re.findall(r'\.'+field+r'=(0x[0-9a-fA-F]+|[0-9]+)u?',image_body)
            if len(value)!=1:raise ValueError('ambiguous native image field: '+field)
            return int(value[0],0)
        if (number('schema')!=1 or number('chip')!=program['chip_id'] or number('code_bytes')!=program['binary_bytes'] or
                number('command_words')!=native['command_words'] or number('patch_count')!=native['patches'] or number('ssbo_slots')!=program['ssbo_table_slots']):raise ValueError('native image numeric metadata mismatch')
        local=re.findall(r'\.local=\{([0-9,]+)\}',image_body)
        if len(local)!=1 or list(map(int,local[0].split(',')))!=program['local_size']:raise ValueError('native image workgroup mismatch')
        code_name=re.findall(r'\.code=([A-Za-z_][A-Za-z_0-9]*),',image_body)
        if len(code_name)!=1:raise ValueError('ambiguous native code array')
        array=re.findall(r'static const uint32_t '+re.escape(code_name[0])+r'\[\] = \{([^}]*)\};',image_text)
        if len(array)!=1:raise ValueError('missing native code array')
        words=[token for token in array[0].split(',') if token]
        if any(re.fullmatch(r'0x[0-9a-fA-F]+u',token) is None for token in words):raise ValueError('invalid native code data grammar')
        if b''.join(int(token[:-1],16).to_bytes(4,'little') for token in words)!=Path(str(prefix)+'.bin').read_bytes():raise ValueError('native image machine code differs from checked binary')
        if header_hash not in images:
            if symbol in symbols:raise ValueError('conflicting native image symbol')
            symbols.add(symbol);name='image_'+str(len(images))+'.h';emit(name,source_data);images[header_hash]=name
            lines+=['#include "'+name+'"','#undef NVN_COMPILED_IMAGE']
        for name in ('code','control'):lines.append(f'static const unsigned char nn_offline_{name}_{index}[]={{'+','.join(map(str,blobs[name]))+'};')
        cbufs,sbufs=resources['cbufs'],resources['sbufs']
        if len(cbufs)>16 or len(sbufs)>16:raise ValueError('excessive reflection')
        if any(not 3<=b['Slot']<16 or not 0<=b['Binding']<16 for b in cbufs) or len({b['Binding'] for b in cbufs})!=len(cbufs):raise ValueError('cbuf reflection contract')
        if any(not 0<=b['Binding']<program['ssbo_table_slots'] or not 0<=b['SbCbSlot']<16 or not 0<=b['SbCbOffset']<=16380 for b in sbufs) or len({b['Binding'] for b in sbufs})!=len(sbufs):raise ValueError('sbuf reflection contract')
        for label,ctype,items in [('cbufs','nn_nvn_cbuf',[f"{{{b['Slot']},{b['Binding']}}}" for b in cbufs]),('sbufs','nn_nvn_sbuf',[f"{{{b['Binding']},{b['SbCbSlot']},{b['SbCbOffset']}}}" for b in sbufs])]:
            if items:lines.append(f'static const struct {ctype} nn_offline_{label}_{index}[]={{'+','.join(items)+'};')
        lines.append(f'static const struct nn_nvn_compiled_cs nn_offline_cs_{index}={{&{symbol},nn_offline_code_{index},nn_offline_control_{index},sizeof(nn_offline_code_{index}),sizeof(nn_offline_control_{index}),'+(f'nn_offline_cbufs_{index}' if cbufs else 'NULL')+f',{len(cbufs)},'+(f'nn_offline_sbufs_{index}' if sbufs else 'NULL')+f',{len(sbufs)}}};')
        entries.append('&nn_offline_cs_'+str(index));summaries.append({'request_sha256':envelope['request_sha256'],'inputs':envelope['inputs'],'local_size':program['local_size'],'cbufs':cbufs,'sbufs':sbufs})
    lines+=['static const struct nn_nvn_compiled_cs *const nn_offline_compute_entries[]={'+','.join(entries)+'};','#define NN_OFFLINE_COMPUTE_COUNT (sizeof(nn_offline_compute_entries)/sizeof(nn_offline_compute_entries[0]))']
    header=emit('offline_compute_cache.h',('\n'.join(lines)+'\n').encode())
    manifest={'schema':1,'policy':'finite-offline-compute-v1','runtime_compiler':False,'fixture_oracle':False,'chip_id':check['chip_id'],'entries':summaries,'inputs':inputs,'outputs':outputs.copy(),'compiler_source_files':source_count}
    emit('cache-manifest.json',(json.dumps(manifest,sort_keys=True,indent=2)+'\n').encode())
    # Catch mutation during generation or verification; never repair records.
    for name,expected in inputs.items():
        if sha(name)!=expected:raise ValueError('cache dependency changed while packaging: '+name)
    return header,manifest,outputs
