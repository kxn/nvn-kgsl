#!/usr/bin/env python3
"""Independent hand-calculated controls, not GPU or shader execution."""
import argparse,json,math,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import casc_gather_depth_oracle as o
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
if a.out.exists():raise ValueError('fresh output required')
checks=0
for mode,want in [(0,(16,0,0,16)),(1,(0,0,16,0)),(2,(0,0,16,16))]:
 for i,v in zip((-1,0,17,33),want):assert o.wrap(i,17,mode)==v;checks+=1
assert o.source(0,0,0)==.125 and o.source(6,0,0)==.875 and o.source(0,0,1)==.75;checks+=3
# At (0,0), source17x11 with the specified rational offsets: u*17-.5
# is approximately -13.234375, v*11-.5 approximately -7.328125.
# Edge clamp yields four source(0,0)=1/8; repeat uses x3,4/y3,4.
assert o.gather(64,64,0,0,0,0,2)==(.125,.125,.125,.125);checks+=1
assert set(o.gather(64,64,0,0,0,0,0))=={.375,.5,.625,.75};checks+=1
assert o.exported(64,64,0,0,0,0,0)==.375;checks+=1
# Odd parity C1=-1 must select MAX, making the wrong algebra observable.
g=o.gather(64,64,1,0,0,0,0);assert len(set(g))>1 and o.exported(64,64,1,0,0,0,0)==max(g)and max(g)!=min(g);checks+=1
matrix=o.cases();assert len(matrix)==36 and all(c['zero_color']for c in matrix);checks+=1
pixels=minpath=maxpath=0;margin=1.
for c in matrix:
 for repeat in range(2):
  for y in range(c['height']):
   for x in range(c['width']):
    g=o.gather(c['width'],c['height'],x,y,c['origin'],repeat,c['wrap_index']);v=o.exported(c['width'],c['height'],x,y,c['origin'],repeat,c['wrap_index']);odd=(x+y)&1
    assert v==(max(g)if odd else min(g));assert v*8==int(v*8)and 0<v<1
    minpath+=not odd;maxpath+=bool(odd);pixels+=1
    u,w=o.coordinates(c['width'],c['height'],x,y,c['origin'],repeat)
    for coord,dim in((u,17),(w,11)):
     q=coord*dim-.5;margin=min(margin,abs(q-round(q)))
assert margin>1/256
result=dict(schema=1,passed=True,hand_calculated_checks=checks,fixture_cases=36,replays=2,planned_draws=72,independent_scalar_pixels=pixels,min_path_pixels=minpath,max_path_pixels=maxpath,minimum_texel_boundary_margin=margin,actual_GPU_submits=0,public_NVN_executed=False,original_C1_first_float=-1.,source_format='R32F dyadic; exact signs/min/max and D32F storage, no conversion')
a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
