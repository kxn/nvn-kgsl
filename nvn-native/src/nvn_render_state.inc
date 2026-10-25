/* SPDX-License-Identifier: MIT
 * Included after recording_command(). Memcpy-safe opaque encoding: generally
 * private; multisample's packed XY table follows independently recovered SDK
 * fields. Scalar defaults/getters are checked against SDK53.314 methods;
 * see render-defaults-sdk-research.zh-CN.md. */
#include <math.h>
static uint64_t rs_load(const void *p,size_t n){uint64_t v=0;memcpy(&v,p,n);return v;}
static uint32_t rs_get(const void *p,size_t n,unsigned shift,unsigned width){return (uint32_t)((rs_load(p,n)>>shift)&((UINT64_C(1)<<width)-1));}
static void rs_put(void *p,size_t n,unsigned shift,unsigned width,uint32_t value){uint64_t mask=((UINT64_C(1)<<width)-1)<<shift;uint64_t v=(rs_load(p,n)&~mask)|(((uint64_t)value<<shift)&mask);memcpy(p,&v,n);}
static int rs_factor(int v){switch(v){case 1:case 2:case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:case 11:case 16:case 17:case 18:case 19:case 97:case 98:case 99:case 100:return 1;default:return 0;}}
static int rs_samples(int v){return v==0||v==1||v==2||v==4||v==8||v==16;}
static int rs_face(int v){return v>=1&&v<=3;}
static int rs_binding_valid(const struct nn_render_state *s){const struct nn_render_target_info t={0x06050002,0,1,0,1};int e=nn_render_state_validate(s,&t);if(e==EINVAL){error(e);return 0;}return 1;}
static void api_nvnBlendStateSetDefaults(NVNblendState *key)
{
    if(!key){error(EINVAL);return;}
    memset(key,0,sizeof(*key)); uint64_t value=UINT64_C(19344393232); memcpy(key,&value,sizeof(*key)<8?sizeof(*key):8);
}
static void api_nvnColorStateSetDefaults(NVNcolorState *key)
{
    if(!key){error(EINVAL);return;}
    memset(key,0,sizeof(*key)); uint64_t value=UINT64_C(33536); memcpy(key,&value,sizeof(*key)<8?sizeof(*key):8);
}
static void api_nvnChannelMaskStateSetDefaults(NVNchannelMaskState *key)
{
    if(!key){error(EINVAL);return;}
    memset(key,0,sizeof(*key)); uint64_t value=UINT64_C(4294967295); memcpy(key,&value,sizeof(*key)<8?sizeof(*key):8);
}
static void api_nvnPolygonStateSetDefaults(NVNpolygonState *key)
{
    if(!key){error(EINVAL);return;}
    memset(key,0,sizeof(*key)); uint64_t value=UINT64_C(20); memcpy(key,&value,sizeof(*key)<8?sizeof(*key):8);
}
static void api_nvnDepthStencilStateSetDefaults(NVNdepthStencilState *key)
{
    if(!key){error(EINVAL);return;}
    memset(key,0,sizeof(*key)); uint64_t value=UINT64_C(36650402826); memcpy(key,&value,sizeof(*key)<8?sizeof(*key):8);
}
static void api_nvnBlendStateSetBlendTarget(NVNblendState *key, int value)
{
    int v=(int)value; if(!key||!(v>=0&&v<8)){error(EINVAL);return;}
    rs_put(key,8,0,3,(uint32_t)v);
}
static int api_nvnBlendStateGetBlendTarget(const NVNblendState *key)
{
    if(!key){error(EINVAL);return (int)0;} return (int)rs_get(key,8,0,3);
}
static void api_nvnColorStateSetLogicOp(NVNcolorState *key, NVNlogicOp value)
{
    int v=(int)value; if(!key||!(v>=0&&v<16)){error(EINVAL);return;}
    rs_put(key,4,8,4,(uint32_t)v);
}
static NVNlogicOp api_nvnColorStateGetLogicOp(const NVNcolorState *key)
{
    if(!key){error(EINVAL);return (NVNlogicOp)0;} return (NVNlogicOp)rs_get(key,4,8,4);
}
static void api_nvnColorStateSetAlphaTest(NVNcolorState *key, NVNalphaFunc value)
{
    int v=(int)value; if(!key||!(v>=1&&v<=8)){error(EINVAL);return;}
    rs_put(key,4,12,4,(uint32_t)v);
}
static NVNalphaFunc api_nvnColorStateGetAlphaTest(const NVNcolorState *key)
{
    if(!key){error(EINVAL);return (NVNalphaFunc)0;} return (NVNalphaFunc)rs_get(key,4,12,4);
}
static void api_nvnPolygonStateSetCullFace(NVNpolygonState *key, NVNface value)
{
    int v=(int)value; if(!key||!(v>=0&&v<=3)){error(EINVAL);return;}
    rs_put(key,4,0,2,(uint32_t)v);
}
static NVNface api_nvnPolygonStateGetCullFace(const NVNpolygonState *key)
{
    if(!key){error(EINVAL);return (NVNface)0;} return (NVNface)rs_get(key,4,0,2);
}
static void api_nvnPolygonStateSetFrontFace(NVNpolygonState *key, NVNfrontFace value)
{
    int v=(int)value; if(!key||!(v>=0&&v<=1)){error(EINVAL);return;}
    rs_put(key,4,2,1,(uint32_t)v);
}
static NVNfrontFace api_nvnPolygonStateGetFrontFace(const NVNpolygonState *key)
{
    if(!key){error(EINVAL);return (NVNfrontFace)0;} return (NVNfrontFace)rs_get(key,4,2,1);
}
static void api_nvnPolygonStateSetPolygonMode(NVNpolygonState *key, NVNpolygonMode value)
{
    int v=(int)value; if(!key||!(v>=0&&v<=2)){error(EINVAL);return;}
    rs_put(key,4,3,2,(uint32_t)v);
}
static NVNpolygonMode api_nvnPolygonStateGetPolygonMode(const NVNpolygonState *key)
{
    if(!key){error(EINVAL);return (NVNpolygonMode)0;} return (NVNpolygonMode)rs_get(key,4,3,2);
}
static void api_nvnPolygonStateSetPolygonOffsetEnables(NVNpolygonState *key, int value)
{
    int v=(int)value; if(!key||!(v>=0&&v<=7)){error(EINVAL);return;}
    rs_put(key,4,5,3,(uint32_t)v);
}
static NVNpolygonOffsetEnable api_nvnPolygonStateGetPolygonOffsetEnables(const NVNpolygonState *key)
{
    if(!key){error(EINVAL);return (NVNpolygonOffsetEnable)0;} return (NVNpolygonOffsetEnable)rs_get(key,4,5,3);
}
static void api_nvnDepthStencilStateSetDepthTestEnable(NVNdepthStencilState *key, NVNboolean value)
{
    int v=(int)value; if(!key||!(1)){error(EINVAL);return;}
    rs_put(key,8,0,1,!!v);
}
static NVNboolean api_nvnDepthStencilStateGetDepthTestEnable(const NVNdepthStencilState *key)
{
    if(!key){error(EINVAL);return (NVNboolean)0;} return (NVNboolean)rs_get(key,8,0,1);
}
static void api_nvnDepthStencilStateSetDepthWriteEnable(NVNdepthStencilState *key, NVNboolean value)
{
    int v=(int)value; if(!key||!(1)){error(EINVAL);return;}
    rs_put(key,8,1,1,!!v);
}
static NVNboolean api_nvnDepthStencilStateGetDepthWriteEnable(const NVNdepthStencilState *key)
{
    if(!key){error(EINVAL);return (NVNboolean)0;} return (NVNboolean)rs_get(key,8,1,1);
}
static void api_nvnDepthStencilStateSetDepthFunc(NVNdepthStencilState *key, NVNdepthFunc value)
{
    int v=(int)value; if(!key||!(v>=1&&v<=8)){error(EINVAL);return;}
    rs_put(key,8,2,4,(uint32_t)v);
}
static NVNdepthFunc api_nvnDepthStencilStateGetDepthFunc(const NVNdepthStencilState *key)
{
    if(!key){error(EINVAL);return (NVNdepthFunc)0;} return (NVNdepthFunc)rs_get(key,8,2,4);
}
static void api_nvnDepthStencilStateSetStencilTestEnable(NVNdepthStencilState *key, NVNboolean value)
{
    int v=(int)value; if(!key||!(1)){error(EINVAL);return;}
    rs_put(key,8,6,1,!!v);
}
static NVNboolean api_nvnDepthStencilStateGetStencilTestEnable(const NVNdepthStencilState *key)
{
    if(!key){error(EINVAL);return (NVNboolean)0;} return (NVNboolean)rs_get(key,8,6,1);
}
static void api_nvnBlendStateSetBlendFunc(NVNblendState *key, NVNblendFunc a,NVNblendFunc b,NVNblendFunc c,NVNblendFunc d)
{
    if(!key||!rs_factor(a)||!rs_factor(b)||!rs_factor(c)||!rs_factor(d)){error(EINVAL);return;}
    rs_put(key,8,3,7,a);rs_put(key,8,10,7,b);rs_put(key,8,17,7,c);rs_put(key,8,24,7,d);
}
static void api_nvnBlendStateGetBlendFunc(const NVNblendState *key,NVNblendFunc *a,NVNblendFunc *b,NVNblendFunc *c,NVNblendFunc *d)
{
    if(!key||!a||!b||!c||!d){error(EINVAL);return;}
    *a=(NVNblendFunc)rs_get(key,8,3,7);*b=(NVNblendFunc)rs_get(key,8,10,7);*c=(NVNblendFunc)rs_get(key,8,17,7);*d=(NVNblendFunc)rs_get(key,8,24,7);
}
static void api_nvnBlendStateSetBlendEquation(NVNblendState *key,NVNblendEquation a,NVNblendEquation b)
{
    if(!key||a<1||a>5||b<1||b>5){error(EINVAL);return;}rs_put(key,8,31,3,a);rs_put(key,8,34,3,b);
}
static void api_nvnBlendStateGetBlendEquation(const NVNblendState *key,NVNblendEquation *a,NVNblendEquation *b)
{
    if(!key||!a||!b){error(EINVAL);return;}*a=(NVNblendEquation)rs_get(key,8,31,3);*b=(NVNblendEquation)rs_get(key,8,34,3);
}
static void api_nvnColorStateSetBlendEnable(NVNcolorState *key,int target,NVNboolean value)
{
    if(!key||target<0||target>7){error(EINVAL);return;}rs_put(key,4,(unsigned)target,1,!!value);
}
static NVNboolean api_nvnColorStateGetBlendEnable(const NVNcolorState *key,int target)
{
    if(!key||target<0||target>7){error(EINVAL);return 0;}return (NVNboolean)rs_get(key,4,(unsigned)target,1);
}
static void api_nvnChannelMaskStateSetChannelMask(NVNchannelMaskState *key,int target,NVNboolean r,NVNboolean g,NVNboolean b,NVNboolean a)
{
    if(!key||target<0||target>7){error(EINVAL);return;}rs_put(key,4,(unsigned)target*4,4,(!!r)|((!!g)<<1)|((!!b)<<2)|((!!a)<<3));
}
static void api_nvnChannelMaskStateGetChannelMask(const NVNchannelMaskState *key,int target,NVNboolean *r,NVNboolean *g,NVNboolean *b,NVNboolean *a)
{
    if(!key||target<0||target>7||!r||!g||!b||!a){error(EINVAL);return;}uint32_t v=rs_get(key,4,(unsigned)target*4,4);*r=v&1;*g=(v>>1)&1;*b=(v>>2)&1;*a=(v>>3)&1;
}
/* SDK53.314: scalar word0, sixteen packed XY bytes4..19, reserved20..23.
 * The sample-position grid covers16 cells: increasing samples reduces the
 * number of pixels in that grid, rather than enlarging a samples matrix. */
_Static_assert(sizeof(NVNmultisampleState)==24,"verified NVN multisample value size");
static void api_nvnMultisampleStateSetDefaults(NVNmultisampleState *key)
{
    if(!key){error(EINVAL);return;}memset(key,0,sizeof(*key));
    uint32_t value=5;memcpy(key,&value,4);memset((unsigned char*)key+4,0x88,16);
}
#define MS_FIELD(name,type,shift,width,valid,normalized,qualifier) \
static void api_nvnMultisampleStateSet##name(NVNmultisampleState *key,type value) \
{ if(!key||!(valid)){error(EINVAL);return;}rs_put(key,4,shift,width,(normalized)); } \
static type api_nvnMultisampleStateGet##name(qualifier NVNmultisampleState *key) \
{ if(!key){error(EINVAL);return (type)0;}return (type)rs_get(key,4,shift,width); }
MS_FIELD(MultisampleEnable,NVNboolean,0,1,1,!!value,const)
MS_FIELD(AlphaToCoverageEnable,NVNboolean,1,1,1,!!value,const)
MS_FIELD(AlphaToCoverageDither,NVNboolean,2,1,1,!!value,const)
MS_FIELD(SampleLocationsEnable,NVNboolean,3,1,1,!!value,const)
MS_FIELD(SampleLocationsGridEnable,NVNboolean,4,1,1,!!value,const)
MS_FIELD(Samples,int,8,5,rs_samples(value),(uint32_t)value,const)
MS_FIELD(CoverageModulationMode,NVNcoverageModulationMode,13,2,value>=0&&value<=3,(uint32_t)value,const)
MS_FIELD(RasterSamples,int,15,5,rs_samples(value),(uint32_t)value,)
MS_FIELD(CoverageToColorEnable,NVNboolean,20,1,1,!!value,const)
MS_FIELD(CoverageToColorOutput,int,21,3,value>=0&&value<=7,(uint32_t)value,const)
#undef MS_FIELD
static void api_nvnMultisampleStateSetSampleLocations(NVNmultisampleState *key,int start,int count,const float *values)
{
    if(!key||start<0||count<0||start>16||count>16-start||(count&&!values)){error(EINVAL);return;}
    unsigned char packed[16];
    for(int i=0;i<count;i++){
        unsigned xy[2];
        for(unsigned c=0;c<2;c++){
            float v=values[i*2+c];if(isnan(v)){error(EINVAL);return;}
            /* SDK finite/infinite clamp followed by nearest 1/16 quantization.
             * NaN has no stable scalar/SIMD agreement and is rejected. */
            if(v<0)v=0;
            if(v>0.9375f)v=0.9375f;
            xy[c]=(unsigned)(v*16.0f+0.5f);
        }
        packed[i]=(unsigned char)(xy[0]|(xy[1]<<4));
    }
    if(count)memcpy((unsigned char*)key+4+start,packed,(size_t)count);
}
static void api_nvnMultisampleStateGetSampleLocationsGrid(NVNmultisampleState *key,int *width,int *height)
{
    if(!key||!width||!height){error(EINVAL);return;}
    unsigned a=rs_get(key,4,8,5),b=rs_get(key,4,15,5),n=a>b?a:b;
    int w,h;
    switch(n){case 0:case 1:w=4;h=4;break;case 2:w=2;h=4;break;
        case 4:w=2;h=2;break;case 8:w=1;h=2;break;case 16:w=1;h=1;break;
        default:error(EINVAL);return;}
    *width=w;*height=h;
}
static void api_nvnDepthStencilStateSetStencilFunc(NVNdepthStencilState *key,NVNface face,NVNstencilFunc func)
{
    if(!key||!rs_face(face)||func<1||func>8){error(EINVAL);return;}for(unsigned i=0;i<2;i++)if(face&(1<<i))rs_put(key,8,7+4*i,4,func);
}
static NVNstencilFunc api_nvnDepthStencilStateGetStencilFunc(const NVNdepthStencilState *key,NVNface face)
{
    if(!key||(face!=1&&face!=2)){error(EINVAL);return (NVNstencilFunc)0;}return (NVNstencilFunc)rs_get(key,8,face==1?7:11,4);
}
static void api_nvnDepthStencilStateSetStencilOp(NVNdepthStencilState *key,NVNface face,NVNstencilOp fail,NVNstencilOp zfail,NVNstencilOp zpass)
{
    if(!key||!rs_face(face)||fail<1||fail>8||zfail<1||zfail>8||zpass<1||zpass>8){error(EINVAL);return;}for(unsigned i=0;i<2;i++)if(face&(1<<i)){rs_put(key,8,15+12*i,4,fail);rs_put(key,8,19+12*i,4,zfail);rs_put(key,8,23+12*i,4,zpass);}
}
static void api_nvnDepthStencilStateGetStencilOp(const NVNdepthStencilState *key,NVNface face,NVNstencilOp *fail,NVNstencilOp *zfail,NVNstencilOp *zpass)
{
    if(!key||(face!=1&&face!=2)||!fail||!zfail||!zpass){error(EINVAL);return;}unsigned i=face==1?0:1;*fail=(NVNstencilOp)rs_get(key,8,15+12*i,4);*zfail=(NVNstencilOp)rs_get(key,8,19+12*i,4);*zpass=(NVNstencilOp)rs_get(key,8,23+12*i,4);
}
static void api_nvnCommandBufferBindBlendState(NVNcommandBuffer *command,const NVNblendState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;unsigned target=rs_get(key,8,0,3);s.blend[target]=(struct nn_render_blend){rs_get(key,8,3,7),rs_get(key,8,10,7),rs_get(key,8,17,7),rs_get(key,8,24,7),rs_get(key,8,31,3),rs_get(key,8,34,3)};
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_BLEND(target),&s))o->record->render=s;
}
static void api_nvnCommandBufferBindColorState(NVNcommandBuffer *command,const NVNcolorState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;s.blend_enable=rs_get(key,4,0,8);s.logic_op=rs_get(key,4,8,4);s.alpha_func=rs_get(key,4,12,4);
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_COLOR,&s))o->record->render=s;
}
static void api_nvnCommandBufferBindChannelMaskState(NVNcommandBuffer *command,const NVNchannelMaskState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;s.color_mask=(uint32_t)rs_load(key,4);
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_MASK,&s))o->record->render=s;
}
static void api_nvnCommandBufferBindPolygonState(NVNcommandBuffer *command,const NVNpolygonState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;s.cull_face=rs_get(key,4,0,2);s.front_face=rs_get(key,4,2,1);s.polygon_mode=rs_get(key,4,3,2);s.polygon_offset_enables=rs_get(key,4,5,3);
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_POLYGON,&s))o->record->render=s;
}
static void api_nvnCommandBufferBindDepthStencilState(NVNcommandBuffer *command,const NVNdepthStencilState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;s.depth.depth_test=rs_get(key,8,0,1);s.depth.depth_write=rs_get(key,8,1,1);s.depth.depth_func=rs_get(key,8,2,4);s.depth.stencil_test=rs_get(key,8,6,1);for(unsigned i=0;i<2;i++){s.depth.stencil_func[i]=rs_get(key,8,7+4*i,4);s.depth.stencil_fail[i]=rs_get(key,8,15+12*i,4);s.depth.stencil_depth_fail[i]=rs_get(key,8,19+12*i,4);s.depth.stencil_depth_pass[i]=rs_get(key,8,23+12*i,4);}
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_DEPTH_STENCIL,&s))o->record->render=s;
}
static void api_nvnCommandBufferBindMultisampleState(NVNcommandBuffer *command,const NVNmultisampleState *key)
{
    struct object *o=recording_command(command);if(!o)return;if(!key){error(EINVAL);return;}
    struct nn_render_state s=o->record->render;
    s.multisample=(struct nn_render_multisample){.enable=rs_get(key,4,0,1),.samples=rs_get(key,4,8,5),
        .alpha_to_coverage=rs_get(key,4,1,1),.alpha_to_coverage_dither=rs_get(key,4,2,1),
        .raster_samples=rs_get(key,4,15,5),.coverage_modulation=rs_get(key,4,13,2),
        .coverage_to_color=rs_get(key,4,20,1),.coverage_output=rs_get(key,4,21,3),
        .locations_enable=rs_get(key,4,3,1),.locations_grid_enable=rs_get(key,4,4,1)};
    memcpy(s.multisample.sample_locations,(const unsigned char*)key+4,16);
    if(rs_binding_valid(&s)&&!command_capture_render(o,NN_COMMAND_STATE_MULTISAMPLE,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetBlendColor(NVNcommandBuffer *command,const float *color)
{
    struct object *o=recording_command(command);if(!o)return;if(!color){error(EINVAL);return;}float value[4];memcpy(value,color,sizeof(value));for(unsigned i=0;i<4;i++)if(!isfinite(value[i])){error(EINVAL);return;}struct nn_render_state s=o->record->render;memcpy(s.blend_color,value,sizeof(value));if(!command_capture_render(o,NN_COMMAND_STATE_BLEND_COLOR,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetDepthBounds(NVNcommandBuffer *command,NVNboolean enable,float min,float max)
{
    struct object *o=recording_command(command);if(!o)return;if(!isfinite(min)||!isfinite(max)||min<0||max>1||min>max){error(EINVAL);return;}struct nn_render_state s=o->record->render;s.depth_bounds_enable=!!enable;s.depth_bounds_min=min;s.depth_bounds_max=max;if(!command_capture_render(o,NN_COMMAND_STATE_DEPTH_BOUNDS,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetPatchSize(NVNcommandBuffer *command,int size)
{
    struct object *o=recording_command(command);if(!o)return;if(size<1||size>32){error(EINVAL);return;}struct nn_render_state s=o->record->render;s.patch_size=(uint32_t)size;if(!command_capture_render(o,NN_COMMAND_STATE_PATCH_SIZE,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetPolygonOffsetClamp(NVNcommandBuffer *command,float factor,float units,float clamp)
{
    struct object *o=recording_command(command);if(!o)return;if(!isfinite(factor)||!isfinite(units)||!isfinite(clamp)){error(EINVAL);return;}struct nn_render_state s=o->record->render;s.polygon_offset_factor=factor;s.polygon_offset_units=units;s.polygon_offset_clamp=clamp;if(!command_capture_render(o,NN_COMMAND_STATE_POLYGON_OFFSET,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetStencilMask(NVNcommandBuffer *command,NVNface face,int value)
{
    struct object *o=recording_command(command);if(!o)return;if(!rs_face(face)){error(EINVAL);return;}struct nn_render_state s=o->record->render;uint64_t mask=0;for(unsigned i=0;i<2;i++)if(face&(1<<i)){s.stencil_write_mask[i]=(uint32_t)value;mask|=NN_COMMAND_STATE_STENCIL_WRITE_FRONT<<i;}if(!command_capture_render(o,mask,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetStencilValueMask(NVNcommandBuffer *command,NVNface face,int value)
{
    struct object *o=recording_command(command);if(!o)return;if(!rs_face(face)){error(EINVAL);return;}struct nn_render_state s=o->record->render;uint64_t mask=0;for(unsigned i=0;i<2;i++)if(face&(1<<i)){s.stencil_value_mask[i]=(uint32_t)value;mask|=NN_COMMAND_STATE_STENCIL_VALUE_FRONT<<i;}if(!command_capture_render(o,mask,&s))o->record->render=s;
}
static void api_nvnCommandBufferSetStencilRef(NVNcommandBuffer *command,NVNface face,int value)
{
    struct object *o=recording_command(command);if(!o)return;if(!rs_face(face)||value<0||value>255){error(EINVAL);return;}struct nn_render_state s=o->record->render;uint64_t mask=0;for(unsigned i=0;i<2;i++)if(face&(1<<i)){s.stencil_ref[i]=(uint32_t)value;mask|=NN_COMMAND_STATE_STENCIL_REF_FRONT<<i;}if(!command_capture_render(o,mask,&s))o->record->render=s;
}
