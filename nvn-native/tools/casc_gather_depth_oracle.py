"""Closed algebra for unchanged CASC Gather+FragDepth source, not a shader VM.

Only the declared authored fixture is modeled: R32F dyadic input, fullscreen
triangle, actual original C1[0]=-1. No GLSL/GL/Vulkan execution is used.
"""
import math
import struct


def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
def source(x,y,repeat):return (1+(x+2*y+5*repeat)%7)/8.
def wrap(index,size,mode):
 if mode==0:return index%size
 if mode==1:
  phase=index%(2*size);return phase if phase<size else 2*size-1-phase
 if mode==2:return min(size-1,max(0,index))
 raise ValueError('unknown wrap')
def coordinates(width,height,x,y,origin,repeat):
 if origin not in(0,1)or repeat not in(0,1):raise ValueError('fixture origin/repeat')
 u=(x+.5)/width;v=((height-y-.5)if origin else(y+.5))/height
 # VS source CB6 byte40/44 offsets, byte48/52 scales, actual float words.
 return f32(2*u+f32((-13+2*repeat)/17)),f32(2*v+f32((-7-2*repeat)/11))
def gather(width,height,x,y,origin,repeat,mode):
 u,v=coordinates(width,height,x,y,origin,repeat);i=math.floor(u*17-.5);j=math.floor(v*11-.5)
 # Reduction is permutation-invariant. This oracle intentionally makes no
 # Gather component-order claim; existing raw TG4 qualification covers that.
 return tuple(source(wrap(i+dx,17,mode),wrap(j+dy,11,mode),repeat)for dx,dy in((0,0),(1,0),(0,1),(1,1)))
def exported(width,height,x,y,origin,repeat,mode,c1=-1.):
 values=gather(width,height,x,y,origin,repeat,mode)
 # Actual NIR: min(R_i*k)*k. Negative original C1 changes min into max.
 # Do not simplify to min(R_i)*k*k, which is wrong for C1=-1.
 fraction=((x+.5+y+.5)*.5)%1
 k=1. if fraction>=.5 else c1
 return f32(min(f32(r*k)for r in values)*k)
def cases():
 out=[]
 for format in(0x32,0x33,0x34):
  for shape,(width,height)in enumerate(((64,64),(73,41))):
   for origin in range(2):
    for mode in range(3):
     out.append(dict(case=len(out),format=format,width=width,height=height,origin=origin,wrap_index=mode,selected_mip=shape,parent_width=width<<shape,parent_height=height<<shape,levels=shape+1,source_format=10,source_width=17,source_height=11,source_levels=1,source_texture_selector=10,source_sampler_selector=3,VS_uniform_index=3,VS_native_binding=1,public_depth_mode=0,compare=(8,2,5,1)[shape*2+origin],clear_depth=0. if shape*2+origin==2 else 1.,clear_depth_bits=0 if shape*2+origin==2 else 0x3f800000,zero_color=True))
 return out

def stored(case,x,y,repeat):
 """After explicit public clear0/1, scissored Gather+depth Draw."""
 clear_depth=case['clear_depth']
 if not(5<=x<case['width']-7 and 3<=y<3+case['height']//2):return clear_depth
 z=exported(case['width'],case['height'],x,y,case['origin'],repeat,case['wrap_index'])
 compare=case['compare'];accept=compare==8 or(compare==2 and z<clear_depth)or(compare==5 and z>clear_depth)
 return z if accept else clear_depth
