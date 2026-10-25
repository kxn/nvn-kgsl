/* A Draw after Call may use only inherited GPU bindings. Keep its operation
 * and native Device query values; resolve effective state at enqueue through
 * the same generic pipeline consumer. No recording-time defaults or old
 * per-Draw resources substitute for the inherited state. */
static int command_record_deferred_draw(struct object *o,NVNdrawPrimitive primitive,int first,int count,const struct nn_gfx_draw *parameters)
{
#if NN_NVN_GRAPHICS_SOURCE_CACHE
 struct recording *r=o->record;struct late_draw *draw=calloc(1,sizeof(*draw));if(!draw)return ENOMEM;
 draw->generic_pipeline=1;struct pipeline_draw_snapshot *p=&draw->pipeline;
 p->native.draw=parameters?*parameters:(struct nn_gfx_draw){.primitive=primitive,.first=(unsigned)first,.count=(unsigned)count,.instances=1};
 p->queries.window_origin=o->device->graphics_state.window_origin;p->queries.depth_mode=o->device->graphics_state.depth_mode;
 p->owner=calloc(1,sizeof(*p->owner));if(!p->owner){free(draw);return ENOMEM;}p->owner->references=1;
 const struct nn_gfx_buffer *arguments[]={&p->native.draw.index,&p->native.draw.indirect,&p->native.draw.indirect_count};int e=0;
 for(unsigned i=0;i<3;i++)if(arguments[i]->bo){
  uint64_t address=nn_bo_address(arguments[i]->bo);if(arguments[i]->offset>UINT64_MAX-address){e=EOVERFLOW;goto fail;}
  struct object *pool=address_pool(o->device,address+arguments[i]->offset,arguments[i]->bytes);
  if(!pool||pool->bo!=arguments[i]->bo){e=EINVAL;goto fail;}
  if((e=pipeline_record_pin(p->owner,pool)))goto fail;
 }
 e=command_reserve(o,16);if(e)goto fail;
 draw->command_offset=r->logical_bytes+r->used-r->start;
 memset((char*)nn_bo_map(r->pool->bo)+r->offset+r->used,0,16);r->used+=16;
 draw->next=r->current->draws;r->current->draws=draw;return 0;
 fail:pipeline_record_release(p);free(draw);return e;
#else
 (void)o;(void)primitive;(void)first;(void)count;(void)parameters;return ENOTSUP;
#endif
}
