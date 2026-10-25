#if NN_NVN_DEPTH_TARGETS
static int public_depth_clear_prepare(struct object*d,const struct late_draw*draw,struct pipeline_prepared_draw*p)
{
 if(!draw->depth||draw->program||!d->pipeline_depth||!d->block_copy)return ENOTSUP;
 int e=public_depth_plan(draw->depth,draw->depth_view,&p->depth_plan);if(e)return e;
 if(gpu_pool_access(draw->depth->pool,draw->depth->offset,draw->depth->size,"NVN depth clear"))return ENOTSUP;
 if((e=public_depth_arena_alias(draw->depth)))return e;
 if((e=pipeline_entry_pin(p,draw->depth))||(e=pipeline_entry_pin(p,draw->depth->pool)))return e;
 const NVNcopyRegion*r=&draw->clear_region;uint32_t w=p->depth_plan.aliases.tiled.width,h=p->depth_plan.aliases.tiled.height;
 if(r->xoffset<0||r->yoffset<0||r->zoffset||r->width<=0||r->height<=0||r->depth!=1||
    (unsigned)r->xoffset>=w||(unsigned)r->width>w-(unsigned)r->xoffset||
    (unsigned)r->yoffset>=h||(unsigned)r->height>h-(unsigned)r->yoffset)return EINVAL;
 float clear;memcpy(&clear,&draw->clear_word,4);if(!(clear>=0&&clear<=1))return EINVAL;
 if((e=nn_depth_transfer_platform_validate(d->gpu,d->pipeline_archive->platform)))return e;
 p->reference_capacity=NN_DEPTH_SHADOW_RESOURCES+3;return 0;
}
#endif
