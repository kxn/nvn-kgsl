/* HOST ONLY metadata and caller BO lifetime; no image conversion/GPU. */
#define _POSIX_C_SOURCE 200809L
#include NN_RESOURCE_MEMORY_BACKEND
#include NN_RESOURCE_CORE
static unsigned checks;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"packaged line%d %s sticky%d\n",__LINE__,#x,nn_nvn_error());return 1;}}while(0)
#define ERR(x) do{CK(nn_nvn_error()==(x));nn_nvn_clear_error();}while(0)
int main(void){
 NVNdevice d={0};NVNdeviceBuilder db={0};NVNmemoryPool p={0};NVNmemoryPoolBuilder pb={0};NVNtextureBuilder b={0};NVNtexture t={0};
 api_nvnDeviceBuilderSetDefaults(&db);CK(api_nvnDeviceInitialize(&d,&db));void *storage=NULL;CK(!posix_memalign(&storage,4096,4u*1024*1024));memset(storage,0xa5,4u*1024*1024);
 api_nvnMemoryPoolBuilderSetDefaults(&pb);api_nvnMemoryPoolBuilderSetDevice(&pb,&d);api_nvnMemoryPoolBuilderSetStorage(&pb,storage,4u*1024*1024);api_nvnMemoryPoolBuilderSetFlags(&pb,0xa4);CK(api_nvnMemoryPoolInitialize(&p,&pb));struct object *pool=find(&p,POOL);size_t refs=pool->refs,bos=pool->device->gpu->bos;
 for(unsigned target=1;target<=4;target+=3)for(unsigned format=0;format<2;format++)for(unsigned bh=0;bh<6;bh++){
  api_nvnTextureBuilderSetDefaults(&b);api_nvnTextureBuilderSetDevice(&b,&d);api_nvnTextureBuilderSetTarget(&b,(NVNtextureTarget)target);api_nvnTextureBuilderSetFormat(&b,(NVNformat)(format?0x44:0x25));api_nvnTextureBuilderSetSize3D(&b,73,format?651:163,target==4?3:1);api_nvnTextureBuilderSetLevels(&b,4);
  NVNpackagedTextureLayout bytes={{(unsigned char)bh,0,0,0,7,0,1,0}};api_nvnTextureBuilderSetPackagedTextureLayout(&b,&bytes);memset(&bytes,0xcc,sizeof(bytes));CK(find(&b,TB)->packaged_layout[0]==bh&&find(&b,TB)->packaged_layout[4]==7&&find(&b,TB)->packaged_layout[6]==1);
#if NN_NVN_BNTX_PACKAGED_EXPERIMENT
  struct nn_tex_source_desc desc={.target=target,.format=format?0x44:0x25,.width=73,.height=format?651:163,.layers=target==4?3u:1u,.levels=4,.block_height_log2=(int)bh};struct nn_tex_source_layout want;CK(!nn_tex_source_layout(&desc,&want));CK(api_nvnTextureBuilderGetStorageSize(&b)==(size_t)want.payload_bytes);CK(api_nvnTextureBuilderGetStorageAlignment(&b)==512);CK(api_nvnTextureBuilderGetStorageClass(&b)==0x1fe);CK(!nn_nvn_error());
  api_nvnTextureBuilderSetStorage(&b,&p,512);CK(api_nvnTextureInitialize(&t,&b));struct object *texture=find(&t,TEXTURE);CK(texture->pool==pool&&texture->bo==pool->bo&&texture->size==want.payload_bytes&&texture->offset==512);CK(!memcmp(texture->source_layout,&want,sizeof(want)));CK(texture->texture_packaged&&texture->packaged_layout[0]==bh);CK(pool->refs==refs+1&&pool->device->gpu->bos==bos);
  memset(&bytes,0xff,sizeof(bytes));api_nvnTextureBuilderSetPackagedTextureLayout(&b,&bytes);CK(texture->packaged_layout[0]==bh&&!memcmp(texture->source_layout,&want,sizeof(want)));api_nvnMemoryPoolFinalize(&p);ERR(EBUSY);api_nvnTextureFinalize(&t);CK(!nn_nvn_error()&&pool->refs==refs);
#else
  CK(!api_nvnTextureBuilderGetStorageSize(&b));ERR(ENOTSUP);CK(!api_nvnTextureInitialize(&t,&b));ERR(ENOTSUP);CK(pool->refs==refs&&pool->device->gpu->bos==bos);
#endif
 }
 const unsigned bad[][2]={{6,0x10007},{7,0x10007},{0x102,0x10007},{2,0x10006},{2,0x20007},{2,0}};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  NVNpackagedTextureLayout x={{0}};for(unsigned j=0;j<4;j++){x.layout[j]=(unsigned char)(bad[i][0]>>(8*j));x.layout[4+j]=(unsigned char)(bad[i][1]>>(8*j));}api_nvnTextureBuilderSetPackagedTextureLayout(&b,&x);CK(!api_nvnTextureBuilderGetStorageSize(&b));ERR(ENOTSUP);CK(!api_nvnTextureInitialize(&t,&b));ERR(ENOTSUP);CK(pool->refs==refs&&pool->device->gpu->bos==bos);
 }
 api_nvnMemoryPoolFinalize(&p);CK(!nn_nvn_error());api_nvnDeviceFinalize(&d);CK(!nn_nvn_error());free(storage);printf("{\"passed\":true,\"HOST_checks\":%u,\"experiment\":%d,\"GPU\":false,\"NVN_encoding_normative_proof\":false}\n",checks,NN_NVN_BNTX_PACKAGED_EXPERIMENT);return 0;
}
