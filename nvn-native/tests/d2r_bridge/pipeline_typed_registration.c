/* HOST ONLY public source-cache registration; no GPU execution. */
#define main untyped_source_cache_main
#include "pipeline_nvn_source.c"
#undef main
int main(void)
{
 NVNdeviceBuilder db={0};NVNdevice dk={0};OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));struct object *dev=find(&dk,DEVICE);T(dev);
 const struct nn_pipeline_typed_archive *a=&NN_GRAPHICS_PIPELINE_TYPED_ARCHIVE;
 if(!NN_NVN_GRAPHICS_SOURCE_CACHE){T(nn_nvn_register_typed_pipeline_archive(&dk,a,reader,ranges)==ENOTSUP&&!dev->pipeline_types);}
 else{
  struct nn_pipeline_typed_archive bad=*a;bad.pair_count--;
  T(nn_nvn_register_typed_pipeline_archive(&dk,&bad,reader,ranges)==EINVAL&&!dev->pipeline_archive&&!dev->pipeline_types);
  T(nn_nvn_register_typed_pipeline_archive(&dk,a,NULL,ranges)==EINVAL&&!dev->pipeline_archive&&!dev->pipeline_types);
  T(!nn_nvn_register_typed_pipeline_archive(&dk,a,reader,ranges)&&dev->pipeline_archive==a->archive&&dev->pipeline_types==a);
  T(nn_nvn_register_typed_pipeline_archive(&dk,a,reader,ranges)==EEXIST&&dev->pipeline_types==a);
 }
 OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB);remove_object(objects);}
 if(NN_NVN_GRAPHICS_SOURCE_CACHE){
  memset(&db,0,sizeof(db));memset(&dk,0,sizeof(dk));OK(api_nvnDeviceBuilderSetDefaults(&db));T(api_nvnDeviceInitialize(&dk,&db));dev=find(&dk,DEVICE);
  T(!nn_nvn_register_pipeline_archive(&dk,a->archive,reader,ranges));
  T(nn_nvn_register_typed_pipeline_archive(&dk,a,reader,ranges)==EEXIST&&!dev->pipeline_types);
  OK(api_nvnDeviceFinalize(&dk));while(objects){T(objects->kind==DB);remove_object(objects);}
 }
 printf("HOST ONLY typed public archive registration checks=%u; GPU submits=0\n",checks);return 0;
}
