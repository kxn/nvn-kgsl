/* SPDX-License-Identifier: MIT. Depends on the resource-copy family above.
 * Uses explicit clear API component masks, never the Draw channel mask.
 * This clear backend requires the recorded full64 scissor; broad typed
 * scissor metadata is never silently treated as a full clear.
 * No depth/stencil layout or aspect storage is invented. */
static int signed_clear_pattern(int format,const int *color,uint32_t words[4],unsigned *count)
{
    _Static_assert(sizeof(int)==4,"NVN signed color ABI");
    if (!color) return EINVAL;
    if (format==NVN_FORMAT_RGBA8I) {
        words[0]=0;
        for (unsigned i=0;i<4;i++) {
            int value=color[i]; if(value<-128)value=-128; if(value>127)value=127;
            words[0]|=((uint32_t)value&255u)<<(i*8);
        }
        *count=1;return 0;
    }
    unsigned n=format==NVN_FORMAT_R32I?1:format==NVN_FORMAT_RG32I?2:format==NVN_FORMAT_RGBA32I?4:0;
    if(!n)return ENOTSUP;
    memcpy(words,color,n*4);*count=n;return 0;
}
#if NN_NVN_CLEAR_COLOR
#include "clear_color_record.inc"
#endif
static void clear_bound_color_native(struct object *o,int index,const void *color,int mask,int type)
{
    if(index<0 || index>=8 || mask<0 || (mask&~15) || !color){error(EINVAL);return;}
    if(!mask)return; /* Explicitly selects no color components. */
    struct object *t=o->record->target;
#if NN_NVN_CLEAR_COLOR
    if(type==0&&o->device->clear_color){
        if(index||!t||!o->record->scissor_set){error(ENOTSUP);return;}
        int e=record_clear_color_native(o,t,color,(unsigned)mask);if(e)error(e);return;
    }
#endif
    if(index || !t || !o->record->scissor_set || mask!=15 ||
       o->record->scissor[0]||o->record->scissor[1]||o->record->scissor[2]!=64||o->record->scissor[3]!=64){error(ENOTSUP);return;}
    if(t->device!=o->device || t->texture.width!=64 || t->texture.height!=64){error(ENOTSUP);return;}
    NVNcopyRegion region={.width=64,.height=64,.depth=1};struct resource_span d;
    uint32_t words[4]={0};unsigned count=0;
    if(t->source_layout){
        int e=type==2?signed_clear_pattern(t->texture.format,color,words,&count):clear_pattern(t->texture.format,color,type,words,&count);
        if(e || count!=1){error(e?e:ENOTSUP);return;}bridge_record_clear(o,t,&region,words[0]);return;
    }
    int e=texture_region(t,NULL,&region,&d);
    if(!e && type==1 && t->texture.format==NVN_FORMAT_RGBA8UI) {
        const uint32_t *u=color; uint32_t clamped[4];
        for(unsigned i=0;i<4;i++)clamped[i]=u[i]>255?255:u[i];
        e=clear_pattern(t->texture.format,clamped,1,words,&count);
    } else if(!e)e=type==2?signed_clear_pattern(t->texture.format,color,words,&count):clear_pattern(t->texture.format,color,type,words,&count);
    if(e){error(e);return;}
    record_resource_job(o,NULL,&d,words,count,0);
}
#include "nvn_command_copy_bound_clear.inc"
static void api_nvnCommandBufferClearColor(NVNcommandBuffer *key,int index,const float *color,int mask)
{ clear_bound_color(key,index,color,mask,0); }
static void api_nvnCommandBufferClearColorui(NVNcommandBuffer *key,int index,const uint32_t *color,int mask)
{ clear_bound_color(key,index,color,mask,1); }
static void api_nvnCommandBufferClearColori(NVNcommandBuffer *key,int index,const int *color,int mask)
{ clear_bound_color(key,index,color,mask,2); }
#include "nvn_depth_clear_record.inc"
static void api_nvnCommandBufferClearDepthStencil(NVNcommandBuffer *key,float depth,NVNboolean clear_depth,int stencil,int stencil_mask)
{
    struct object *command=recording_command(key);if(!command)return;
    if((clear_depth!=0 && clear_depth!=1) || stencil_mask<0 || stencil_mask>255 ||
        (clear_depth && !(depth>=0 && depth<=1)) || (stencil_mask && (stencil<0 || stencil>255))) {error(EINVAL);return;}
#if NN_NVN_DEPTH_TARGETS
    if(stencil_mask){error(ENOTSUP);return;}
    if(clear_depth&&command->device->pipeline_depth){int e=record_public_depth_clear(command,depth);if(e)error(e);return;}
#else
    (void)command;
#endif
    if(clear_depth || stencil_mask){error(ENOTSUP);return;}
    /* No aspect requested: a legal empty operation. Requests to an aspect
     * require an actual supported depth/stencil attachment and are rejected. */
}
