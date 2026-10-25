"""Independent closed authored endpoint colors; no compressed input/codec."""
import math
def expected(f,x,y,r):
 bx,by,lx,ly=x//4,y//4,x%4,y%4
 code=(0,1,3,1)[(lx+2*ly+bx+3*by+r)%4]if f<0x47 else(lx+2*ly+by+r)%2
 high=code==1 if f<0x47 else code==0
 endpoint=(16,32,8)if high else(8,16,16)
 def linear(v,b):
  s=v/(2**b-1);return math.floor(255*(s/12.92 if s<=.04045 else((s+.055)/1.055)**2.4)+.5)
 out=[linear(v,b)for v,b in zip(endpoint,(5,6,5))]+[255]
 if f<0x47 and code==3:
  out[:3]=[0,0,0]
  if f==0x46:out[3]=0
 if f==0x47:out[3]=((lx+ly+bx+r)%4)*85
 if f==0x48:out[3]=(0,255,0,255)[(lx+ly+bx+r)%4]
 return bytes([17,29,43,59]if out[3]==0 else out)
