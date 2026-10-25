/* Authored qualification oracle copied from the existing raw-native caller.
 * Original shader closed form only; no packet or instruction execution. */
static unsigned quantize(double f)
{if(f<0)f=0;if(f>1)f=1;return (unsigned)floor(f*255.0+.5);}
/* Independent original shader closed form: fullscreen triangle barycentrics.
 * Repeat moves clip x by .75, i.e.3/8 viewport width; the left region retains
 * prior actual GPU output. No pixel is read to generate an expected value. */
static void pixel(unsigned width,unsigned height,unsigned mode,unsigned repeat,unsigned x,unsigned y,unsigned rgba[4])
{
 double u=((double)x+.5)/(double)width,v=((double)y+.5)/(double)height;
 int shifted=repeat&&u>=.375;
 if(shifted)u-=.375;
 if(shifted&&mode==3){rgba[0]=255;rgba[1]=0;rgba[2]=255;rgba[3]=255;return;}
 double a=1-(u+v)*.5,b=u*.5,c=v*.5;
 rgba[0]=quantize(shifted?c:a);rgba[1]=quantize(shifted?a:b);rgba[2]=quantize(shifted?b:c);rgba[3]=255;
}
