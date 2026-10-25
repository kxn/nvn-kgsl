/* SPDX-License-Identifier: MIT. Typed clear to canonical GPU DWORD fill.
 * No private render state becomes NVN Program/UBO/SSBO state. */
static int clear_color_target_layout(const struct object *t,struct nn_tex_source_layout *out)
{
    if(!t||!out||t->kind!=TEXTURE||!t->device||!t->pool||t->pool->external||
       t->texture.target!=NVN_TEXTURE_TARGET_2D||t->texture.levels!=1||t->texture.depth!=1||t->texture.samples||
       !texture_sampling_identity(&t->texture))return ENOTSUP;
    if(t->source_layout){*out=*t->source_layout;return nn_tex_source_validate(out);}
    const struct texture_desc *d=&t->texture;
    struct nn_tex_source_desc source={.flags=(uint32_t)d->flags,.target=1,.format=(uint32_t)d->format,
        .width=(uint32_t)d->width,.height=(uint32_t)d->height,.layers=1,.levels=1,.stride=(uint64_t)d->stride,.block_height_log2=-1};
    int e=nn_tex_source_layout(&source,out);if(e)return e;
    /* Native LINEAR public storage can include final aligned rows/pages.
     * The kernel only spans source pitch*activeHeight; owner bounds retain
     * the full public allocation and padding remains untouched. */
    return out->linear&&out->payload_bytes<=t->size?0:ENOTSUP;
}
static int clear_color_target_admit(const struct object *t)
{
    struct nn_tex_source_layout source;int e=clear_color_target_layout(t,&source);if(e)return e;
    const float zero[4]={0};struct nn_clear_color_rect full={0,0,t->texture.width,t->texture.height};
    struct nn_clear_color_plan p;return nn_clear_color_plan(&source,&full,zero,15,&p);
}
static int record_clear_color_native(struct object *command,struct object *target,const float color[4],unsigned mask)
{
    struct recording *r=command->record;struct object *device=command->device;
    if(!r->active||target->device!=device||!device->clear_color)return ENOTSUP;
    if(target->refs==SIZE_MAX)return EOVERFLOW;
    target->refs++;int e=command_reserve(command,24);target->refs--;if(e)return e;
    struct nn_tex_source_layout source;if((e=clear_color_target_layout(target,&source)))return e;
    struct nn_clear_color_rect rect={r->scissor[0],r->scissor[1],r->scissor[2],r->scissor[3]};
    struct nn_clear_color_plan plan;if((e=nn_clear_color_plan(&source,&rect,color,mask,&plan)))return e;
    if((e=gpu_pool_access(r->pool,r->offset,r->capacity,"constant-clear command arena")))return e;
    void *commands=nn_bo_map(r->pool->bo);if(!commands)return ENOTSUP;
    if(plan.empty){
        /* Genuine empty clipped rectangle/absent component. Fixed16B typed
         * operation remains replayable even if a later caller changes target. */
        uint32_t *w=(uint32_t*)((char*)commands+r->offset+r->used);
        w[0]=nvn_pm4_packet7(0x10,3);w[1]=w[2]=w[3]=0;r->used+=16;return 0;
    }
    struct resource_span destination={target->pool,target,target->offset,(size_t)plan.destination_bytes,(size_t)plan.destination_bytes,1};
    if((e=gpu_pool_access(target->pool,target->offset,(size_t)plan.destination_bytes,"constant-clear canonical destination"))||
       (e=canonical_copy_arena(&destination)))return e;
    if(target->refs==SIZE_MAX||target->pool->refs==SIZE_MAX||device->refs>SIZE_MAX-3)return EOVERFLOW;
    struct job *code=calloc(1,sizeof(*code)),*uniform=calloc(1,sizeof(*uniform)),*dispatch=calloc(1,sizeof(*dispatch));
    if(!code||!uniform||!dispatch){free(code);free(uniform);free(dispatch);return ENOMEM;}
    size_t old=r->current->count,bytes=0;const struct nn_cs_image *image=device->clear_color;
    if((e=nn_bo_alloc(device->gpu,image->code_bytes,&code->bo))||(e=nn_bo_alloc(device->gpu,64,&uniform->bo)))goto fail;
    void *cm=nn_bo_map(code->bo),*um=nn_bo_map(uniform->bo);if(!cm||!um){e=ENOTSUP;goto fail;}
    struct nn_clear_color_range dst={target->pool->bo,0,target->pool->size,target->offset};
    struct nn_clear_color_range ubo={uniform->bo,0,nn_bo_size(uniform->bo),0};struct nn_clear_color_binding binding;
    if((e=nn_clear_color_bind(device->gpu,image,&source,&plan,&dst,&ubo,&binding)))goto fail;
    memcpy(cm,image->code,image->code_bytes);memcpy(um,&binding.args,64);
    if((e=nn_cs_encode(device->gpu,code->bo,image,binding.ubos,binding.ssbos,binding.groups,&dispatch->bo,&bytes)))goto fail;
    if((e=nn_bo_cache(code->bo,0,image->code_bytes,0))||(e=nn_bo_cache(uniform->bo,0,64,0))||
       (e=nn_bo_cache(dispatch->bo,0,nn_bo_size(dispatch->bo),0)))goto fail;
    if((e=add_resource(r->current,target))||(e=add_resource(r->current,target->pool)))goto fail;
    uint64_t ib=nn_bo_address(dispatch->bo);uint32_t *w=(uint32_t*)((char*)commands+r->offset+r->used);
    w[0]=nvn_pm4_packet7(NVN_CP_INDIRECT_BUFFER,3);w[1]=(uint32_t)ib;w[2]=(uint32_t)(ib>>32);w[3]=(uint32_t)(bytes/4);r->used+=16;
    dispatch->copy_destination=target;dispatch->copy_destination_pool=target->pool;
    dispatch->copy_destination_offset=target->offset;dispatch->copy_destination_bytes=(size_t)plan.destination_bytes;
    code->compute=uniform->compute=dispatch->compute=1;
    code->next=uniform;uniform->next=dispatch;dispatch->next=r->current->jobs;r->current->jobs=code;device->refs+=3;return 0;
fail:
    while(r->current->count>old)r->current->resources[--r->current->count]->refs--;
    struct job *retired=NULL,*jobs[]={code,uniform,dispatch};
    for(unsigned i=0;i<3;i++)if(jobs[i]->bo){jobs[i]->next=retired;retired=jobs[i];device->refs++;}else free(jobs[i]);
    retire_jobs(device,retired);return e;
}
