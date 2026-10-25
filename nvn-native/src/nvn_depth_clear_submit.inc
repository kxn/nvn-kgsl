#if NN_NVN_DEPTH_TARGETS
static int public_depth_clear_materialize(struct object*d,const struct pipeline_prepared_draw*p,nn_bo**bo,size_t*bytes,struct job**owned,nn_bo**refs,size_t*cursor,size_t capacity)
{
 *bo=NULL;*bytes=0;*owned=NULL;
 if(!p||!p->draw.depth_clear||!p->draw.depth||*cursor>capacity||p->reference_capacity>capacity-*cursor)return EINVAL;
 struct job *nodes[4]={0};for(unsigned i=0;i<4;i++)if(!(nodes[i]=calloc(1,sizeof(*nodes[i])))){for(unsigned j=0;j<i;j++)free(nodes[j]);return ENOMEM;}
 int e=0;struct object*z=p->draw.depth;struct job*owner=nodes[0];owner->depth_owner=calloc(1,sizeof(*owner->depth_owner));if(!owner->depth_owner){e=ENOMEM;goto done;}
 struct pipeline_resource_pins*pins=calloc(1,sizeof(*pins));if(!pins){e=ENOMEM;goto done;}
 if(z->pool->refs==SIZE_MAX){free(pins);e=EOVERFLOW;goto done;}
 pins->objects[0]=z->pool;pins->count=1;z->pool->refs++;owner->resource_pins=pins;
 struct nn_block_copy_range canonical={z->bo,z->offset,z->size,z->offset};
 if((e=nn_depth_shadow_create(d->gpu,d->block_copy,d->pipeline_archive->platform,z->source_layout,&p->depth_plan,&canonical,capacity-*cursor,owner->depth_owner)))goto done;
 nn_bo*resources[NN_DEPTH_SHADOW_RESOURCES];size_t count=0;
 if((e=nn_depth_shadow_resources(owner->depth_owner,resources,NN_DEPTH_SHADOW_RESOURCES,&count)))goto done;
 if(count>capacity-*cursor){e=ENOSPC;goto done;}
 for(size_t i=0;i<count;i++)queue_reference(refs,cursor,resources[i]);
 const NVNcopyRegion*r=&p->draw.clear_region;struct nn_depth_a2d_region region={(unsigned)r->xoffset,(unsigned)r->yoffset,(unsigned)r->width,(unsigned)r->height};
 float clear;memcpy(&clear,&p->draw.clear_word,4);size_t clear_bytes=0;
 e=nn_depth_transfer_encode(d->gpu,&owner->depth_owner->attachment,NULL,d->pipeline_archive->platform,NN_DEPTH_A2D_CLEAR,&region,clear,&nodes[1]->bo,&clear_bytes);if(e)goto done;
 struct nn_depth_shadow_step load[2],store[2];if((e=nn_depth_shadow_sequence(owner->depth_owner,load,store)))goto done;
 struct nn_tex_shadow steps[2]={0};const struct nn_tex_shadow*list[2]={steps,steps+1};
 for(unsigned i=0;i<2;i++){steps[i].bos[NN_TEX_SHADOW_PRE]=load[i].job;steps[i].pre_bytes=load[i].bytes;}
 size_t before_bytes=0;e=nn_tex_shadow_chain(d->gpu,list,2,nodes[1]->bo,clear_bytes,NULL,&nodes[2]->bo,&before_bytes);if(e)goto done;
 steps[0].bos[NN_TEX_SHADOW_PRE]=nodes[2]->bo;steps[0].pre_bytes=before_bytes;steps[1].bos[NN_TEX_SHADOW_PRE]=store[0].job;steps[1].pre_bytes=store[0].bytes;
 e=nn_tex_shadow_chain(d->gpu,list,2,store[1].job,store[1].bytes,NULL,&nodes[3]->bo,bytes);if(e)goto done;
 *bo=nodes[3]->bo;nodes[3]->bo=NULL;
 done:for(unsigned i=0;i<4;i++){if(nodes[i]->bo||nodes[i]->depth_owner){nodes[i]->next=*owned;*owned=nodes[i];d->refs++;}else free(nodes[i]);}
 return e;
}
#endif
