"""Independent raw-byte qualifier oracle. No production/CPU image conversion.

The inverse GOB sector order follows public minigbm tegra transfer_tile(k),
independent from the shader's direct coordinate equation. See MINIGBM-LICENSE.
Native extents/pitches are checked actual-FDL producer artifacts, never guessed.
"""
import json
import math
import re
from functools import lru_cache
NAMES=tuple(f'layout-{n:02d}-r{r}.bin' for n in range(55) for r in range(2))
BASE_SOURCE=4096;BASE_NATIVE=8192
FORMATS={1:(1,1),6:(2,1),0x25:(4,1),0x2a:(8,1),0x2e:(16,1),0x41:(8,4),0x44:(16,4)}
def up(v,a):return (v+a-1)//a*a

def parse_result(text):
    def pairs(items):
        out={}
        for k,v in items:
            if k in out:raise RuntimeError('duplicate conversion JSON key: '+k)
            out[k]=v
        return out
    try:result=json.loads(text,object_pairs_hook=pairs,parse_constant=lambda v:(_ for _ in ()).throw(ValueError(v)))
    except (TypeError,ValueError) as e:raise RuntimeError('invalid conversion JSON') from e
    if type(result) is not dict:raise RuntimeError('conversion result is not an object')
    return result

def pattern(offset,n,repeat):
    v=(offset^((n*0x9e3779b9)&0xffffffff)^((repeat*0x6a09e667)&0xffffffff))&0xffffffff
    v^=v>>16;v=(v*0x7feb352d)&0xffffffff;v^=v>>15;v=(v*0x846ca68b)&0xffffffff;v^=v>>16
    return v&255

def geometry(c):
    """Independent source geometry from original SDK/public source facts."""
    d=c['desc'];cpp,block=FORMATS[d['format']];linear=bool(d['flags']&0x50)
    if any(type(v) is not int for v in d.values()):raise RuntimeError('noninteger source descriptor')
    levels=[];offset=0;rows=(d['height']+block-1)//block;base=0
    if d['block_height_log2']>=0:base=d['block_height_log2']
    else:
        while base<4 and rows+rows//2>=16<<base:base+=1
    for level in range(d['levels']):
        width=max(1,d['width']>>level);height=max(1,d['height']>>level)
        row=((width+block-1)//block)*cpp;rows=(height+block-1)//block;bh=base
        while bh and rows<=8<<(bh-1):bh-=1
        pitch=d['stride'] if linear else up(row,64);size=pitch*rows if linear else pitch*up(rows,8<<bh)
        levels.append(dict(source_offset=offset,source_bytes=size,row_bytes=row,rows=rows,source_pitch=pitch,block_height_log2=0 if linear else bh));offset+=size
    stride=offset if linear or d['target']==1 else up(offset,512<<levels[0]['block_height_log2'])
    total=stride*d['layers']
    exact={'source_bytes':total,'source_layer_stride':stride,'linear':int(linear),'levels':d['levels'],'layers':d['layers'],'cpp':cpp,'block_w':block,'block_h':block,'source_alignment':(128 if d['flags']&0x40 else 32) if linear else 0,'source_public_extent_known':int(linear),'gob_bytes':0 if linear else 512}
    for k,v in exact.items():
        if type(c.get(k)) is not int or c[k]!=v:raise RuntimeError('source geometry mismatch: '+k)
    if len(c['level'])!=len(levels):raise RuntimeError('source mip inventory mismatch')
    intervals=[]
    for l,m in zip(levels,c['level']):
        for k,v in l.items():
            if type(m.get(k)) is not int or m[k]!=v:raise RuntimeError('source mip geometry mismatch: '+k)
        for k in ('native_offset','native_pitch','native_layer_stride','native_bytes'):
            if type(m.get(k)) is not int or not 0<=m[k]<=0xffffffff:raise RuntimeError('FDL metadata type/range')
        if m['native_pitch']%4 or m['native_pitch']<m['row_bytes'] or m['native_bytes']!=m['native_pitch']*m['rows'] or m['native_layer_stride']<m['native_bytes']:raise RuntimeError('FDL invalid native pitch/extent')
        for layer in range(c['layers']):
            a=m['native_offset']+layer*m['native_layer_stride'];b=a+m['native_bytes']
            if b>c['native_bytes'] or any(a<y and x<b for x,y in intervals):raise RuntimeError('FDL subresource range alias/out-of-bounds')
            intervals.append((a,b))
    return levels

def source_addresses(c,level):
    """Yield xbyte,y,source-relative offset using inverse dword sector order."""
    m=c['level'][level]
    if c['linear']:
        for y in range(m['rows']):
            for x in range(m['row_bytes']):yield x,y,y*m['source_pitch']+x
    else:
        bh=1<<m['block_height_log2'];width_gobs=m['source_pitch']//64
        for br in range((m['rows']+8*bh-1)//(8*bh)):
            for bc in range(width_gobs):
                for k in range(128*bh):
                    x=bc*64+(((k>>3)&8)|((k>>1)&4)|(k&3))*4
                    y=br*8*bh+((k>>7<<3)|((k>>3)&6)|((k>>2)&1))
                    if y>=m['rows']:continue
                    for lane in range(4):
                        if x+lane<m['row_bytes']:
                            yield x+lane,y,((br*width_gobs+bc)*512*bh)+k*4+lane

def sizes(c):
    return (up(c['source_bytes']+2*BASE_SOURCE,4096),up(c['native_bytes']+BASE_NATIVE+4096,4096),up(c['source_bytes']+2*BASE_SOURCE,4096))

@lru_cache(maxsize=110)
def cached_expected(document,repeat):
    c=json.loads(document);geometry(c);n=c['id'];sb,nb,rb=sizes(c)
    source=bytearray([0xa5])*sb;native=bytearray([0xa5])*nb;roundtrip=bytearray([0x5a])*rb
    for offset in range(c['source_bytes']):source[BASE_SOURCE+offset]=pattern(offset,n,repeat)
    seen_source=set();seen_native=set();written=0
    for layer in range(c['layers']):
        for level,m in enumerate(c['level']):
            count=0
            for x,y,relative in source_addresses(c,level):
                offset=layer*c['source_layer_stride']+m['source_offset']+relative
                to=BASE_NATIVE+layer*m['native_layer_stride']+m['native_offset']+y*m['native_pitch']+x
                if offset in seen_source or to in seen_native:raise RuntimeError('inverse reference is not injective')
                seen_source.add(offset);seen_native.add(to);count+=1;written+=1
                value=pattern(offset,n,repeat);native[to]=value;roundtrip[BASE_SOURCE+offset]=value
            if count!=m['row_bytes']*m['rows']:raise RuntimeError('inverse reference missed payload bytes')
    return bytes(source+native+roundtrip),written,nb+rb-2*written

def expected(c,repeat):return cached_expected(json.dumps(c,sort_keys=True),repeat)

def validate(data,captured,nonce,uid,check):
    if type(data) is not dict or type(uid) is not int or uid<10000 or type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('conversion ordinary UID/types/nonce invalid')
    exact={'schema':1,'operation':'texture_conversion','uid':uid,'chip_id':0x06050002,'stage':'complete','errno':0,'cleanup_errno':0,'submission':'accepted','completed_cases':110,'submissions':110,'finishes':110,'dispatches':612,'sequence_word_checks':612,'replays':55,'inflight_destroy_rejections':110,'files':110,'resources_retained':False,'original_game_shader':False,'nvn_api':False,'runtime_compiler':False,'cpu_image_conversion':False,'public_tiled_storage_getters_qualified':False,'compressible_qualified':False,'passed':True,'run_nonce':nonce,'run_directory':'conversion-'+nonce}
    for k,v in exact.items():
        if type(data.get(k)) is not type(v) or data[k]!=v:raise RuntimeError('conversion result mismatch: '+k)
    cases=check['cases'];records=data.get('cases')
    if len(cases)!=55 or type(records) is not list or len(records)!=55:raise RuntimeError('conversion case inventory mismatch')
    if type(captured) is not dict or set(captured)!=set(NAMES):raise RuntimeError('conversion exact raw inventory mismatch')
    all_checks=0;payload=0;guards=0;readonly=0
    for n,(c,r) in enumerate(zip(cases,records)):
        geometry(c);sb,nb,rb=sizes(c);jobs=2*c['layers']*c['levels']
        target={'case':n,'source_bo_bytes':sb,'native_bo_bytes':nb,'roundtrip_bo_bytes':rb,'jobs':jobs,'repeats':2}
        if type(r) is not dict or set(r)!=set(target) or any(type(r[k]) is not int or r[k]!=v for k,v in target.items()):raise RuntimeError('conversion actual allocation/dispatch metadata mismatch')
        for repeat in range(2):
            raw=captured[NAMES[n*2+repeat]];want,written,guard=expected(c,repeat)
            if type(raw) is not bytes or len(raw)!=len(want):raise RuntimeError('conversion raw file size/type mismatch')
            if raw!=want:
                index=next(i for i,(a,b) in enumerate(zip(raw,want)) if a!=b)
                raise RuntimeError(f'{NAMES[n*2+repeat]} byte{index}: {raw[index]} != {want[index]}')
            all_checks+=len(raw);payload+=written*2;guards+=guard
            readonly+=8192+sb+8192+jobs*(8192-4)
    exact_counts={'data_byte_checks':sum((sizes(c)[1]+sizes(c)[2])*2 for c in cases),'guard_byte_checks':guards,'readonly_byte_checks':readonly}
    for k,v in exact_counts.items():
        if type(data.get(k)) is not int or data[k]!=v:raise RuntimeError('conversion exact guard/read-only counter mismatch: '+k)
    return {'passed':True,'layout_cases':55,'replays':55,'GPU_submits':110,'GPU_dispatches':612,'raw_files':110,'raw_byte_checks':all_checks,'active_bidirectional_byte_checks':payload,'guard_byte_checks':guards,'readonly_byte_checks':readonly,'byte_tolerance':0,'BC_decompression_qualified':False,'NVN_public_storage_getters_qualified':False,'COMPRESSIBLE_qualified':False,'scope':'Actual authored SSBO raw-byte conversion only; no original game shader/NVN texture/compositor claim'}
