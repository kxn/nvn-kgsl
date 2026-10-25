"""Closed original SAMPLE/CASC algebra; HOST only, no shader execution."""
import math,struct
from casc_gather_depth_oracle import f32,coordinates

def cases():
 out=[]
 for role in range(2):
  for fmt in (50,51,52):
   for shape,(width,height) in enumerate(((64,64),(73,41))):
    for origin in range(2):
     for route in range(3):
      out.append(dict(case=len(out),role=role,format=fmt,width=width,height=height,origin=origin,wrap_index=route,selected_mip=shape,compare=2))
 return out

def source(c,x,y,repeat):
 value=(1+(x+2*y+3*c['selected_mip']+5*repeat)%7)/8
 if c['format']==52:return f32(value)
 scale=65535 if c['format']==50 else 16777215
 return f32(math.floor(value*scale+.5)/scale)

def red(c,x,y,repeat):
 w,h=c['width'],c['height'];origin=c['origin']
 if not c['role']:
  u=(x+.5)/w;v=(h-y-.5)/h if origin else(y+.5)/h
  return source(c,min(16,math.floor(17*u)),min(10,math.floor(11*v)),repeat)
 u,v=coordinates(w,h,x,y,origin,repeat);i=math.floor(17*u-.5);j=math.floor(11*v-.5)
 values=[source(c,min(16,max(0,i+dx)),min(10,max(0,j+dy)),repeat)for dx,dy in((0,0),(1,0),(0,1),(1,1))]
 return min(values)if((x+y+1)&1)else max(values)
