/* SPDX-License-Identifier: MIT
 * HOST ONLY: real NVN value-state API and RAM backend; no GPU/game execution. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <math.h>
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "../../src/nvn.c"
#endif
#include NN_TEST_NVN_SOURCE
static unsigned checks;
#define T(x) do{checks++;if(!(x)){fprintf(stderr,"line%d: %s errno%d\n",__LINE__,#x,nn_nvn_error());abort();}}while(0)
#define CALL(name,...) ((__typeof__(&api_##name))nvnBootstrapLoader(#name))(__VA_ARGS__)
#define OK(x) do{x;T(!nn_nvn_error());}while(0)
#define ERR(x,e) do{x;T(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static uint32_t value(const uint32_t *words,size_t n,unsigned reg)
{ for(size_t i=0;i<n;i+=2)if(words[i]==nvn_pm4_packet4(reg,1))return words[i+1];abort(); }
/* Independent SDK getter/branch-table facts, not a hardware execution model. */
static void check_sdk_multisample_values(void)
{
    NVNmultisampleState m,before;unsigned char expected[24];
    OK(CALL(nvnMultisampleStateSetDefaults,&m));
    memset(expected,0,sizeof(expected));expected[0]=5;memset(expected+4,0x88,16);
    T(!memcmp(&m,expected,sizeof(expected)));
    static const int samples[]={0,1,2,4,8,16};
    static const int grid[][2]={{4,4},{4,4},{2,4},{2,2},{1,2},{1,1}};
    for(unsigned a=0;a<6;a++)for(unsigned b=0;b<6;b++){
        OK(CALL(nvnMultisampleStateSetSamples,&m,samples[a]));
        OK(CALL(nvnMultisampleStateSetRasterSamples,&m,samples[b]));
        int w=71,h=73;OK(CALL(nvnMultisampleStateGetSampleLocationsGrid,&m,&w,&h));
        unsigned greater=a>b?a:b;T(w==grid[greater][0]&&h==grid[greater][1]);
    }
    before=m;memcpy(expected,&m,sizeof(expected));
    const float positions[]={0,1,-1,.5f,.96875f,.03125f};
    OK(CALL(nvnMultisampleStateSetSampleLocations,&m,5,3,positions));
    expected[9]=0xf0;expected[10]=0x80;expected[11]=0x1f;
    T(!memcmp(&m,expected,sizeof(expected)));
    before=m;const float invalid[]={0,0,NAN,1};
    ERR(CALL(nvnMultisampleStateSetSampleLocations,&m,0,2,invalid),EINVAL);T(!memcmp(&m,&before,sizeof(m)));
    ERR(CALL(nvnMultisampleStateSetSampleLocations,&m,15,2,positions),EINVAL);T(!memcmp(&m,&before,sizeof(m)));
    OK(CALL(nvnMultisampleStateSetSampleLocations,&m,16,0,NULL));T(!memcmp(&m,&before,sizeof(m)));
    const float infinities[]={-INFINITY,INFINITY};
    OK(CALL(nvnMultisampleStateSetSampleLocations,&m,15,1,infinities));expected[19]=0xf0;
    T(!memcmp(&m,expected,sizeof(expected)));
}
int main(void)
{
#undef ENTRY
#define ENTRY(name,type) T(nvnBootstrapLoader(#name)!=NULL);
#include "render_state_entries.inc"
#undef ENTRY
    check_sdk_multisample_values();
    NVNblendState b,copy;NVNcolorState c;NVNchannelMaskState m;NVNmultisampleState ms;NVNpolygonState p;NVNdepthStencilState d;
    OK(CALL(nvnBlendStateSetDefaults,&b));OK(CALL(nvnColorStateSetDefaults,&c));OK(CALL(nvnChannelMaskStateSetDefaults,&m));OK(CALL(nvnMultisampleStateSetDefaults,&ms));OK(CALL(nvnPolygonStateSetDefaults,&p));OK(CALL(nvnDepthStencilStateSetDefaults,&d));
    /* Original SDK474330 writes5; getter474400 extracts bit2. These are scalar
     * API facts, not assumptions about our private opaque byte encoding. */
    T(CALL(nvnMultisampleStateGetMultisampleEnable,&ms)==1&&CALL(nvnMultisampleStateGetSamples,&ms)==0&&CALL(nvnMultisampleStateGetAlphaToCoverageEnable,&ms)==0&&CALL(nvnMultisampleStateGetAlphaToCoverageDither,&ms)==1&&CALL(nvnMultisampleStateGetRasterSamples,&ms)==0&&CALL(nvnMultisampleStateGetCoverageModulationMode,&ms)==0&&CALL(nvnMultisampleStateGetCoverageToColorEnable,&ms)==0&&CALL(nvnMultisampleStateGetCoverageToColorOutput,&ms)==0&&CALL(nvnMultisampleStateGetSampleLocationsEnable,&ms)==0&&CALL(nvnMultisampleStateGetSampleLocationsGridEnable,&ms)==0);
    T(CALL(nvnBlendStateGetBlendTarget,&b)==0);NVNblendFunc a1,a2,a3,a4;NVNblendEquation e1,e2;
    OK(CALL(nvnBlendStateGetBlendFunc,&b,&a1,&a2,&a3,&a4));T(a1==2&&a2==1&&a3==2&&a4==1);
    OK(CALL(nvnBlendStateGetBlendEquation,&b,&e1,&e2));T(e1==1&&e2==1);
    T(CALL(nvnColorStateGetLogicOp,&c)==3&&CALL(nvnColorStateGetAlphaTest,&c)==8);
    T(CALL(nvnDepthStencilStateGetDepthTestEnable,&d)==0&&CALL(nvnDepthStencilStateGetDepthWriteEnable,&d)==1&&CALL(nvnDepthStencilStateGetDepthFunc,&d)==2&&CALL(nvnDepthStencilStateGetStencilTestEnable,&d)==0);
    NVNstencilOp f,zf,zp;for(unsigned face=1;face<=2;face++){T(CALL(nvnDepthStencilStateGetStencilFunc,&d,face)==8);OK(CALL(nvnDepthStencilStateGetStencilOp,&d,face,&f,&zf,&zp));T(f==1&&zf==1&&zp==1);}
    T(CALL(nvnPolygonStateGetCullFace,&p)==0&&CALL(nvnPolygonStateGetFrontFace,&p)==1&&CALL(nvnPolygonStateGetPolygonMode,&p)==2&&CALL(nvnPolygonStateGetPolygonOffsetEnables,&p)==0);
    for(int target=0;target<8;target++){
        NVNboolean rr,gg,bb,aa;OK(CALL(nvnChannelMaskStateGetChannelMask,&m,target,&rr,&gg,&bb,&aa));T(rr&&gg&&bb&&aa);T(!CALL(nvnColorStateGetBlendEnable,&c,target));
        OK(CALL(nvnChannelMaskStateSetChannelMask,&m,target,1,0,0,1));OK(CALL(nvnChannelMaskStateGetChannelMask,&m,target,&rr,&gg,&bb,&aa));T(rr&&!gg&&!bb&&aa);
        OK(CALL(nvnColorStateSetBlendEnable,&c,target,7));T(CALL(nvnColorStateGetBlendEnable,&c,target)==1);
    }
    copy=b;ERR(CALL(nvnBlendStateSetBlendTarget,&b,8),EINVAL);T(!memcmp(&b,&copy,sizeof(b)));
    ERR(CALL(nvnBlendStateSetBlendFunc,&b,(NVNblendFunc)0,2,2,2),EINVAL);T(!memcmp(&b,&copy,sizeof(b)));
    ERR(CALL(nvnBlendStateSetBlendEquation,&b,0,1),EINVAL);T(!memcmp(&b,&copy,sizeof(b)));
    ERR(CALL(nvnBlendStateGetBlendFunc,&b,&a1,NULL,&a3,&a4),EINVAL);
    OK(CALL(nvnBlendStateSetBlendTarget,&b,3));OK(CALL(nvnBlendStateSetBlendFunc,&b,5,6,2,1));OK(CALL(nvnBlendStateSetBlendEquation,&b,2,3));
    copy=b;memset(&b,0xa5,sizeof(b));T(CALL(nvnBlendStateGetBlendTarget,&copy)==3);OK(CALL(nvnBlendStateGetBlendFunc,&copy,&a1,&a2,&a3,&a4));T(a1==5&&a2==6&&a3==2&&a4==1);
    OK(CALL(nvnMultisampleStateSetMultisampleEnable,&ms,(NVNboolean)0));T(CALL(nvnMultisampleStateGetMultisampleEnable,&ms)==0);
    OK(CALL(nvnMultisampleStateSetSamples,&ms,(int)4));T(CALL(nvnMultisampleStateGetSamples,&ms)==4);
    OK(CALL(nvnMultisampleStateSetAlphaToCoverageEnable,&ms,(NVNboolean)1));T(CALL(nvnMultisampleStateGetAlphaToCoverageEnable,&ms)==1);
    OK(CALL(nvnMultisampleStateSetAlphaToCoverageDither,&ms,(NVNboolean)0));T(CALL(nvnMultisampleStateGetAlphaToCoverageDither,&ms)==0);
    OK(CALL(nvnMultisampleStateSetRasterSamples,&ms,(int)4));T(CALL(nvnMultisampleStateGetRasterSamples,&ms)==4);
    OK(CALL(nvnMultisampleStateSetCoverageModulationMode,&ms,(NVNcoverageModulationMode)3));T(CALL(nvnMultisampleStateGetCoverageModulationMode,&ms)==3);
    OK(CALL(nvnMultisampleStateSetCoverageToColorEnable,&ms,(NVNboolean)1));T(CALL(nvnMultisampleStateGetCoverageToColorEnable,&ms)==1);
    OK(CALL(nvnMultisampleStateSetCoverageToColorOutput,&ms,(int)7));T(CALL(nvnMultisampleStateGetCoverageToColorOutput,&ms)==7);
    OK(CALL(nvnMultisampleStateSetSampleLocationsEnable,&ms,(NVNboolean)1));T(CALL(nvnMultisampleStateGetSampleLocationsEnable,&ms)==1);
    OK(CALL(nvnMultisampleStateSetSampleLocationsGridEnable,&ms,(NVNboolean)1));T(CALL(nvnMultisampleStateGetSampleLocationsGridEnable,&ms)==1);
    NVNmultisampleState old_ms=ms;float location[2]={.5f,.5f};OK(CALL(nvnMultisampleStateSetSampleLocations,&ms,0,1,location));T(!memcmp(&old_ms,&ms,sizeof(ms)));int gw=71,gh=73;OK(CALL(nvnMultisampleStateGetSampleLocationsGrid,&ms,&gw,&gh));T(gw==2&&gh==2);ERR(CALL(nvnMultisampleStateSetSamples,&ms,3),EINVAL);OK(CALL(nvnMultisampleStateSetDefaults,&ms));
    OK(CALL(nvnDepthStencilStateSetDepthTestEnable,&d,1));OK(CALL(nvnDepthStencilStateSetDepthWriteEnable,&d,0));OK(CALL(nvnDepthStencilStateSetDepthFunc,&d,8));OK(CALL(nvnDepthStencilStateSetStencilTestEnable,&d,1));OK(CALL(nvnDepthStencilStateSetStencilFunc,&d,NVN_FACE_FRONT_AND_BACK,4));OK(CALL(nvnDepthStencilStateSetStencilOp,&d,NVN_FACE_BACK,2,3,4));T(CALL(nvnDepthStencilStateGetDepthTestEnable,&d)==1&&CALL(nvnDepthStencilStateGetDepthWriteEnable,&d)==0&&CALL(nvnDepthStencilStateGetDepthFunc,&d)==8&&CALL(nvnDepthStencilStateGetStencilTestEnable,&d)==1);T(CALL(nvnDepthStencilStateGetStencilFunc,&d,NVN_FACE_BACK)==4);OK(CALL(nvnDepthStencilStateGetStencilOp,&d,NVN_FACE_BACK,&f,&zf,&zp));T(f==2&&zf==3&&zp==4);OK(CALL(nvnDepthStencilStateSetDefaults,&d));
    OK(CALL(nvnPolygonStateSetCullFace,&p,NVN_FACE_BACK));OK(CALL(nvnPolygonStateSetFrontFace,&p,NVN_FRONT_FACE_CW));OK(CALL(nvnPolygonStateSetPolygonMode,&p,NVN_POLYGON_MODE_LINE));OK(CALL(nvnPolygonStateSetPolygonOffsetEnables,&p,3));T(CALL(nvnPolygonStateGetCullFace,&p)==2&&CALL(nvnPolygonStateGetFrontFace,&p)==0&&CALL(nvnPolygonStateGetPolygonMode,&p)==1&&CALL(nvnPolygonStateGetPolygonOffsetEnables,&p)==3);OK(CALL(nvnPolygonStateSetDefaults,&p));OK(CALL(nvnColorStateSetLogicOp,&c,NVN_LOGIC_OP_XOR));OK(CALL(nvnColorStateSetAlphaTest,&c,NVN_ALPHA_FUNC_ALWAYS));
    NVNdeviceBuilder db={0};NVNdevice dev={0};NVNcommandBuffer cmd={0};OK(CALL(nvnDeviceBuilderSetDefaults,&db));T(CALL(nvnDeviceInitialize,&dev,&db));T(CALL(nvnCommandBufferInitialize,&cmd,&dev));
    struct object *o=find(&cmd,COMMAND);T(o);struct nn_render_state def;nn_render_state_defaults(&def);T(!memcmp(&o->record->render,&def,sizeof(def)));T(def.multisample.alpha_to_coverage_dither==1);
    ERR(CALL(nvnCommandBufferBindBlendState,&cmd,&copy),EINVAL);o->record->active=1;
    OK(CALL(nvnCommandBufferBindBlendState,&cmd,&copy));T(o->record->render.blend[3].src_rgb==5&&o->record->render.blend[3].equation_alpha==3);
    memset(&copy,0,sizeof(copy));T(o->record->render.blend[3].src_rgb==5);
    OK(CALL(nvnCommandBufferBindColorState,&cmd,&c));T(o->record->render.blend_enable==255);
    OK(CALL(nvnCommandBufferBindChannelMaskState,&cmd,&m));T(o->record->render.color_mask==0x99999999);
    OK(CALL(nvnCommandBufferBindPolygonState,&cmd,&p));OK(CALL(nvnCommandBufferBindDepthStencilState,&cmd,&d));OK(CALL(nvnCommandBufferBindMultisampleState,&cmd,&ms));
    float bound_locations[2]={0,1};OK(CALL(nvnMultisampleStateSetSampleLocations,&ms,7,1,bound_locations));
    OK(CALL(nvnCommandBufferBindMultisampleState,&cmd,&ms));T(o->record->render.multisample.sample_locations[7]==0xf0);
    OK(CALL(nvnMultisampleStateSetDefaults,&ms));T(o->record->render.multisample.sample_locations[7]==0xf0);
    struct nn_render_state prior=o->record->render;ERR(CALL(nvnCommandBufferBindBlendState,&cmd,NULL),EINVAL);T(!memcmp(&prior,&o->record->render,sizeof(prior)));
    float color[4]={.125f,.25f,.5f,1};OK(CALL(nvnCommandBufferSetBlendColor,&cmd,color));color[0]=9;T(o->record->render.blend_color[0]==.125f);
    struct nn_render_state snap=o->record->render;OK(CALL(nvnCommandBufferSetStencilRef,&cmd,NVN_FACE_FRONT_AND_BACK,17));T(snap.stencil_ref[0]==0&&o->record->render.stencil_ref[0]==17&&o->record->render.stencil_ref[1]==17);
    OK(CALL(nvnCommandBufferSetStencilMask,&cmd,NVN_FACE_BACK,-1));OK(CALL(nvnCommandBufferSetStencilValueMask,&cmd,NVN_FACE_FRONT,0x55));T(o->record->render.stencil_write_mask[1]==UINT32_MAX&&o->record->render.stencil_value_mask[0]==0x55);
    OK(CALL(nvnCommandBufferSetPatchSize,&cmd,32));OK(CALL(nvnCommandBufferSetDepthBounds,&cmd,0,.25f,.75f));OK(CALL(nvnCommandBufferSetPolygonOffsetClamp,&cmd,1.5f,-2,.5f));
    prior=o->record->render;ERR(CALL(nvnCommandBufferSetPatchSize,&cmd,0),EINVAL);ERR(CALL(nvnCommandBufferSetDepthBounds,&cmd,0,1,0),EINVAL);ERR(CALL(nvnCommandBufferSetPolygonOffsetClamp,&cmd,NAN,0,0),EINVAL);ERR(CALL(nvnCommandBufferSetBlendColor,&cmd,NULL),EINVAL);ERR(CALL(nvnCommandBufferSetStencilMask,&cmd,NVN_FACE_NONE,1),EINVAL);T(!memcmp(&prior,&o->record->render,sizeof(prior)));
    ERR(CALL(nvnBlendStateSetDefaults,NULL),EINVAL);CALL(nvnColorStateSetDefaults,&c);T(nn_nvn_error()==0);
    /* Sticky errors survive legal writes/getters, and state bytes are still real. */
    ERR(CALL(nvnDepthStencilStateSetStencilFunc,&d,NVN_FACE_NONE,1),EINVAL);api_nvnBlendStateSetBlendTarget(NULL,0);api_nvnColorStateSetDefaults(&c);T(nn_nvn_error()==EINVAL);T(api_nvnColorStateGetLogicOp(&c)==3&&nn_nvn_error()==EINVAL);nn_nvn_clear_error();
    uint32_t words[64],saved[64];size_t n=0;struct nn_render_target_info ti={0x06050002,0,1,0,1};nn_render_state_defaults(&snap);T(!nn_render_state_encode(&snap,&ti,NULL,0,&n));T(n==48);memset(words,0x6a,sizeof(words));memcpy(saved,words,sizeof(words));T(nn_render_state_encode(&snap,&ti,words,n-1,&n)==ENOSPC&&n==0&&!memcmp(saved,words,sizeof(words)));
    T(!nn_render_state_encode(&snap,&ti,words,64,&n));T(value(words,n,0x8820)==((12u<<3)|(15u<<7)));T(value(words,n,0x8821)==(1u|(1u<<16)));T(value(words,n,0x8871)==0&&value(words,n,0x8880)==0&&value(words,n,0x8114)==0&&value(words,n,0x8115)==0);
    snap.blend_enable=1;snap.blend[0]=(struct nn_render_blend){5,6,97,98,2,5};snap.color_mask=10;snap.blend_color[0]=.5f;snap.cull_face=2;snap.front_face=0;snap.polygon_offset_enables=4;
    T(!nn_render_state_encode(&snap,&ti,words,64,&n));T(value(words,n,0x8820)==(3u|(12u<<3)|(10u<<7)));T(value(words,n,0x8821)==(6u|(1u<<5)|(7u<<8)|(12u<<16)|(4u<<21)|(13u<<24)));T(value(words,n,0x8090)==(2u|4u|16u|2048u|8192u));T(value(words,n,0x8860)==0x3f000000);T(value(words,n,0xa989)==1&&value(words,n,0x8865)==0xffff0001);
    /* Disabled state parameters are stored but cannot block a draw. */
    nn_render_state_defaults(&snap);snap.multisample.locations_grid_enable=1;snap.multisample.alpha_to_coverage_dither=1;
    snap.depth.depth_func=8;snap.depth.stencil_func[0]=1;snap.patch_size=32;
    T(!nn_render_state_validate(&snap,&ti));
    /* Logic replaces blending; dual factors in an inactive blend path are inert. */
    snap.logic_op=NVN_LOGIC_OP_XOR;snap.blend_enable=1;snap.blend[0].src_rgb=16;
    T(!nn_render_state_encode(&snap,&ti,words,64,&n));
    T(value(words,n,0x8820)==(4u|(6u<<3)|(15u<<7)));
    T(value(words,n,0x8865)==0xffff0001);
    /* Unsupported enabled features must leave staged caller output untouched. */
    for(unsigned what=0;what<10;what++){
        nn_render_state_defaults(&snap);switch(what){case 0:snap.depth.depth_test=1;break;case 1:snap.depth.stencil_test=1;break;case 2:snap.depth_bounds_enable=1;break;case 3:snap.multisample.samples=4;break;case 4:snap.multisample.alpha_to_coverage=1;break;case 5:snap.multisample.locations_enable=1;break;case 6:snap.alpha_func=1;break;case 7:snap.blend_enable=1;snap.blend[0].src_rgb=16;break;case 8:snap.cull_face=2;ti.window_origin=1;break;case 9:ti.depth_attachment=1;break;}
        memcpy(words,saved,sizeof(words));T(nn_render_state_encode(&snap,&ti,words,64,&n)==ENOTSUP&&n==0&&!memcmp(words,saved,sizeof(words)));ti.window_origin=0;ti.depth_attachment=0;
    }
    o->record->active=0;OK(CALL(nvnCommandBufferFinalize,&cmd));OK(CALL(nvnDeviceFinalize,&dev));while(objects){T(objects->kind==DB);remove_object(objects);}T(!handles);
    printf("{\"scope\":\"host_only_render_state\",\"checks\":%u,\"gpu_submits\":0,\"typed_entries\":75}\n",checks);return 0;
}
