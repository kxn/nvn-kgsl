"""NumPy closed authored endpoint oracle; never reads compressed input bytes."""
import numpy as np

def base_count(view,repeat):
 return (((0,4),(1,3)),((1,2),(2,2)),((3,1),(0,1)))[view][repeat]
def pixels(case,repeat):
 f=case['format'];base,_=base_count(case['view'],repeat)
 w=max(1,case['width']>>base);h=max(1,case['height']>>base)
 yy,xx=np.indices((64,64),dtype=np.int64)
 x=((2*xx+1)*w)//128;y=((2*yy+1)*h)//128
 bx,by,lx,ly=x//4,y//4,x%4,y%4;phase=base+repeat;kind=(f-0x41)%4
 if kind<2:codes=np.array([0,1,3,1])[((lx+2*ly+bx+3*by+phase)%4)];high=codes==1
 else:codes=(lx+2*ly+by+phase)%2;high=codes==0
 bank=(base+repeat)%4;value=np.where(high,16+2*bank,8+2*bank)
 channels=np.stack((value/31.,2*value/63.,value/31.),axis=-1)
 if f>=0x45:channels=np.where(channels<=.04045,channels/12.92,((channels+.055)/1.055)**2.4)
 out=np.empty((64,64,4),dtype=np.uint8);out[:,:,:3]=np.floor(channels*255+.5).astype(np.uint8);out[:,:,3]=255
 if kind<2:
  out[:,:,:3][codes==3]=0
  if kind==1:out[:,:,3][codes==3]=0
 elif kind==2:out[:,:,3]=((lx+ly+bx+phase)%4)*85
 else:out[:,:,3]=np.array([0,255,0,255],dtype=np.uint8)[(lx+ly+bx+phase)%4]
 out[out[:,:,3]==0]=[17,29,43,59]
 return out.tobytes()
