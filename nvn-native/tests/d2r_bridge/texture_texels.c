/* SPDX-License-Identifier: MIT. HOST arguments, ownership, no image execution. */
#define _POSIX_C_SOURCE 200809L
#define nn_bo_alloc raw_alloc
#define nn_bo_import raw_import
#define nn_bo_cache raw_cache
#define nn_bo_destroy raw_destroy
#define nn_queue_submit raw_submit
#define nn_queue_finish raw_finish
#include NN_TEXELS_MEMORY_BACKEND
#undef nn_bo_alloc
#undef nn_bo_import
#undef nn_bo_cache
#undef nn_bo_destroy
#undef nn_queue_submit
#undef nn_queue_finish
#include "nvn_native/block_copy.h"
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
static unsigned checks,submissions,finishes,allocations,imports,caches;
static int fail_alloc,fail_import,fail_cache,fail_submit,fail_finish,fail_destroy;
static struct nn_block_copy_args seen;
static struct {nn_bo *bo;size_t offset,bytes;int from;} last_cache;
int nn_bo_alloc(nn_device *d,size_t n,nn_bo **b){allocations++;if(fail_alloc){fail_alloc=0;*b=NULL;return ENOMEM;}return raw_alloc(d,n,b);}
int nn_bo_import(nn_device *d,void *p,size_t n,nn_bo **b){imports++;if(fail_import){fail_import=0;*b=NULL;return ENOMEM;}return raw_import(d,p,n,b);}
int nn_bo_cache(nn_bo *b,size_t o,size_t n,int f){caches++;last_cache.bo=b;last_cache.offset=o;last_cache.bytes=n;last_cache.from=f;if(fail_cache){fail_cache=0;return EIO;}return raw_cache(b,o,n,f);}
int nn_bo_destroy(nn_bo *b){if(fail_destroy){fail_destroy=0;return EIO;}return raw_destroy(b);}
int nn_queue_submit(nn_queue *q,nn_bo *b,size_t o,size_t n,nn_bo *const *refs,size_t count,uint32_t *ts)
{
 (void)o;assert(n&&(count==3||count==4)&&count+1<=64-q->count);submissions++;memcpy(&seen,nn_bo_map(refs[count-1]),sizeof(seen));
 q->pending[q->count++]=b;b->busy++;for(size_t i=0;i<count;i++){q->pending[q->count++]=refs[i];refs[i]->busy++;}
 if(fail_submit){fail_submit=0;return EDEADLK;}*ts=submissions;return 0;
}
int nn_queue_finish(nn_queue *q,uint32_t timeout){finishes++;assert(timeout==1000);if(fail_finish){fail_finish=0;return ETIMEDOUT;}return raw_finish(q,timeout);}
#include NN_TEXELS_CORE
#include NN_TEXELS_IMAGE
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"line%d %s error%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define GOOD() CK(!nn_nvn_error())
#define BAD(e) do{CK(nn_nvn_error()==(e));nn_nvn_clear_error();}while(0)
static NVNdevice device;static NVNtexture tex;static NVNmemoryPool pools[2];static void *memory[2];static size_t sizes[2];
static unsigned char *snapshot[2];static void *caller;
static int pool(unsigned i,size_t size,int flags)
{
 CK(!posix_memalign(memory+i,4096,size));sizes[i]=size;memset(memory[i],0xa5,size);snapshot[i]=malloc(size);CK(snapshot[i]);
 NVNmemoryPoolBuilder b={0};api_nvnMemoryPoolBuilderSetDefaults(&b);api_nvnMemoryPoolBuilderSetDevice(&b,&device);api_nvnMemoryPoolBuilderSetFlags(&b,flags);api_nvnMemoryPoolBuilderSetStorage(&b,memory[i],size);CK(api_nvnMemoryPoolInitialize(pools+i,&b));GOOD();return 0;
}
static int make_texture(const struct nn_public_copy_case *c)
{
 NVNtextureBuilder b={0};api_nvnTextureBuilderSetDefaults(&b);api_nvnTextureBuilderSetDevice(&b,&device);api_nvnTextureBuilderSetFlags(&b,c->flags);api_nvnTextureBuilderSetTarget(&b,c->target);api_nvnTextureBuilderSetSize3D(&b,c->width,c->height,c->layers);api_nvnTextureBuilderSetLevels(&b,c->levels);api_nvnTextureBuilderSetFormat(&b,c->format);
 CK(api_nvnTextureBuilderGetStorageSize(&b)==c->source.payload_bytes);api_nvnTextureBuilderSetStorage(&b,pools,c->canonical_offset);CK(api_nvnTextureInitialize(&tex,&b));GOOD();return 0;
}
static int release_case(void)
{
 api_nvnTextureFinalize(&tex);GOOD();for(unsigned i=0;i<2;i++)if(memory[i]){api_nvnMemoryPoolFinalize(pools+i);GOOD();free(memory[i]);free(snapshot[i]);memory[i]=NULL;snapshot[i]=NULL;}return 0;
}
static int cases(void)
{
 unsigned before=submissions;
 for(unsigned n=0;n<234;n++){
  const struct nn_public_copy_case *c=nn_public_copy_cases+n;if(c->family==2)continue;
  CK(!pool(0,c->pool_bytes[0],0xa4));if(c->pool_bytes[1])CK(!pool(1,c->pool_bytes[1],0xa4));CK(!make_texture(c));
  NVNtextureView v={0};api_nvnTextureViewSetDefaults(&v);api_nvnTextureViewSetFormat(&v,c->format);api_nvnTextureViewSetLevels(&v,c->level,1);api_nvnTextureViewSetLayers(&v,c->base_layer,c->layer_count);GOOD();
  unsigned which=c->shared?0:1;void *ptr=(char *)memory[which]+c->linear_offset;struct object *t=find(&tex,TEXTURE);uint64_t addr=api_nvnTextureGetTextureAddress(&tex);unsigned old=submissions,im=imports,fin=finishes;size_t bos=t->device->gpu->bos;
  for(unsigned repeat=0;repeat<2;repeat++){
   for(unsigned i=0;i<2;i++)if(memory[i]){memset(memory[i],0xa5+repeat,sizes[i]);memcpy(snapshot[i],memory[i],sizes[i]);}
   if(c->family==0){if(c->variant)api_nvnTextureWriteTexelsStrided(&tex,&v,c->region,ptr,c->row_stride,c->image_stride);else api_nvnTextureWriteTexels(&tex,&v,c->region,ptr);}
   else{if(c->variant)api_nvnTextureReadTexelsStrided(&tex,&v,c->region,ptr,c->row_stride,c->image_stride);else api_nvnTextureReadTexels(&tex,&v,c->region,ptr);}
   if(nn_nvn_error()){fprintf(stderr,"case%u repeat%u\n",n,repeat);}
   GOOD();
   CK(submissions==old+repeat+1&&finishes==fin+repeat+1&&imports==im);CK(!retained_texel_transfers&&t->refs==0&&t->device->gpu->bos==bos);CK(api_nvnTextureGetTextureAddress(&tex)==addr);
   const struct nn_tex_source_level *m=c->source.level+c->level;
   CK(seen.canonical_base==m->offset+c->source.layer_stride*c->base_layer);CK(seen.canonical_layer_stride==c->source.layer_stride);CK(seen.width_gobs==m->pitch/64&&seen.block_height_log2==m->block_height_log2);
   CK(seen.region_x_bytes==c->x_bytes&&seen.region_y_rows==c->y_rows&&seen.row_bytes==c->row_bytes&&seen.rows==c->rows&&seen.layers==c->layer_count);CK(seen.direction==(c->family==0));CK(seen.linear_row_stride==(c->rows>1?c->row_stride:c->row_bytes));CK(seen.linear_span<=c->linear_bytes&&seen.linear_words==(seen.linear_span+3)/4);
   /* The HOST queue did NOT execute the shader. No CPU image operation may
    * secretly fill either source, destination or padding while tests pass. */
   for(unsigned i=0;i<2;i++)if(memory[i])CK(!memcmp(memory[i],snapshot[i],sizes[i]));
  }
  /* SDK53.314 caches the selected full layers, independently of mip/format/
   * target/region. An invalid region pointer must not be dereferenced. */
  api_nvnTextureViewSetLevels(&v,99,1);api_nvnTextureViewSetFormat(&v,NVN_FORMAT_RGBA8);api_nvnTextureViewSetTarget(&v,NVN_TEXTURE_TARGET_BUFFER);GOOD();unsigned cache=caches;
  api_nvnTextureFlushTexels(&tex,&v,(const NVNcopyRegion *)(uintptr_t)1);GOOD();CK(caches==cache+1&&last_cache.bo==t->bo&&!last_cache.from&&last_cache.offset==c->canonical_offset+c->base_layer*c->source.layer_stride&&last_cache.bytes==c->layer_count*c->source.layer_stride);
  api_nvnTextureInvalidateTexels(&tex,&v,NULL);GOOD();CK(caches==cache+2&&last_cache.from==1);
  CK(!release_case());
 }
 CK(submissions-before==312);return 0;
}
static int negatives(void)
{
 const struct nn_public_copy_case *c=nn_public_copy_cases;CK(!pool(0,65536,0xa4));CK(!make_texture(c));CK(!posix_memalign(&caller,4096,32768));memset(caller,0xb6,32768);
 unsigned s=submissions,f=finishes,a=allocations,im=imports;struct object *t=find(&tex,TEXTURE);NVNcopyRegion r={0,0,0,c->width,c->height,c->layers};
 api_nvnTextureWriteTexels(&tex,NULL,&r,(char*)caller+1);BAD(EINVAL);api_nvnTextureReadTexels(&tex,NULL,&r,(char*)memory[0]+512);BAD(EINVAL);CK(submissions==s&&finishes==f&&allocations==a&&imports==im);
 t->bo->busy++;api_nvnTextureWriteTexels(&tex,NULL,&r,caller);BAD(EBUSY);api_nvnTextureReadTexels(&tex,NULL,&r,caller);BAD(EBUSY);t->bo->busy--;CK(submissions==s&&finishes==f&&allocations==a&&imports==im);
 NVNmemoryPool duplicate;NVNmemoryPoolBuilder b={0};api_nvnMemoryPoolBuilderSetDefaults(&b);api_nvnMemoryPoolBuilderSetDevice(&b,&device);api_nvnMemoryPoolBuilderSetFlags(&b,0xa4);api_nvnMemoryPoolBuilderSetStorage(&b,memory[0],65536);CK(api_nvnMemoryPoolInitialize(&duplicate,&b));GOOD();find(&duplicate,POOL)->bo->busy++;
 api_nvnTextureWriteTexels(&tex,NULL,&r,caller);BAD(EBUSY);find(&duplicate,POOL)->bo->busy--;api_nvnMemoryPoolFinalize(&duplicate);GOOD();CK(submissions==s&&finishes==f);
 CK(!mprotect(caller,32768,PROT_READ));api_nvnTextureWriteTexels(&tex,NULL,&r,caller);BAD(ENOTSUP);CK(!mprotect(caller,32768,PROT_READ|PROT_WRITE));
 int flags=t->pool->flags;t->pool->flags=0xa1;api_nvnTextureWriteTexels(&tex,NULL,&r,caller);BAD(ENOTSUP);t->pool->flags=flags;
 enum nn_nvn_gpu_pool_policy policy=t->pool->gpu_policy;t->pool->gpu_policy=NN_NVN_GPU_UNCACHED_UNQUALIFIED;api_nvnTextureReadTexels(&tex,NULL,&r,caller);BAD(ENOTSUP);t->pool->gpu_policy=policy;
 NVNtextureView v={0};api_nvnTextureViewSetDefaults(&v);api_nvnTextureViewSetLayers(&v,c->layers,1);api_nvnTextureFlushTexels(&tex,&v,NULL);BAD(EINVAL);
 CK(submissions==s&&finishes==f);
 for(unsigned mode=0;mode<3;mode++){
  unsigned old_imports=imports;size_t bos=t->device->gpu->bos;
  if(mode==0)fail_import=1;
  if(mode==1)fail_alloc=1;
  if(mode==2)fail_cache=1;
  api_nvnTextureWriteTexels(&tex,NULL,&r,caller);BAD(mode<2?ENOMEM:EIO);CK(!retained_texel_transfers&&!t->refs&&t->device->gpu->bos==bos&&imports==old_imports+1);CK(submissions==s&&finishes==f);
 }
 /* Genuine imported caller bytes, still a nonexecuting HOST queue. */
 api_nvnTextureWriteTexels(&tex,NULL,&r,caller);GOOD();CK(submissions==s+1&&finishes==f+1);for(unsigned i=0;i<32768;i++)CK(((unsigned char *)caller)[i]==0xb6);
 /* Unknown submit/finish and cleanup failure preserve complete owners. */
 for(unsigned mode=0;mode<3;mode++){
  pid_t child=fork();CK(child>=0);if(!child){
   if(mode==0)fail_submit=1;else if(mode==1)fail_finish=1;else fail_destroy=1;
   api_nvnTextureWriteTexels(&tex,NULL,&r,caller);assert(nn_nvn_error()==(mode==0?EDEADLK:mode==1?ETIMEDOUT:EIO));
   assert(retained_texel_transfers&&retained_texel_transfers->texture==t&&t->refs==1);nn_nvn_clear_error();api_nvnTextureFinalize(&tex);assert(nn_nvn_error()==EBUSY);_exit(0);
  }int status;CK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));
 }
 free(caller);caller=NULL;CK(!release_case());return 0;
}
int main(void)
{
 NVNdeviceBuilder b={0};api_nvnDeviceBuilderSetDefaults(&b);CK(api_nvnDeviceInitialize(&device,&b));GOOD();CK(!nn_nvn_register_block_copy(&device,&NVN_COMPILED_IMAGE));
 CK(nvnBootstrapLoader("nvnTextureWriteTexels")== (PFNNVNGENERICFUNCPTRPROC)api_nvnTextureWriteTexels);CK(nvnBootstrapLoader("nvnTextureWriteTexelsStrided")== (PFNNVNGENERICFUNCPTRPROC)api_nvnTextureWriteTexelsStrided);CK(nvnBootstrapLoader("nvnTextureFlushTexels")== (PFNNVNGENERICFUNCPTRPROC)api_nvnTextureFlushTexels);
 CK(!cases());CK(!negatives());api_nvnDeviceFinalize(&device);GOOD();
 printf("{\"passed\":true,\"HOST_checks\":%u,\"public_geometry_cases\":156,\"mock_nonexecuting_submissions\":%u,\"actual_GPU_submissions\":0,\"CPU_image_conversion\":false}\n",checks,submissions);return 0;
}
