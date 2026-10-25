/* Entire existing module/core/ordinary/quarantine regression plus explicit
 * builder-key reuse through the host image import constructor. */
#define main previous_external_fixture_main
#include "external_image_pool.c"
#undef main
static int recycled_import(void)
{
 NVNdeviceBuilder db={0};NVNdevice device={0};
 union {NVNtextureBuilder builder;NVNmemoryPool pool;} key;
 api_nvnDeviceBuilderSetDefaults(&db);CHECK(api_nvnDeviceInitialize(&device,&db));
 api_nvnTextureBuilderSetDefaults(&key.builder);CHECK(!nn_nvn_error());
 struct owner owner={0};struct nn_external_image_desc desc=descriptor(&owner);
 CHECK(!nn_nvn_import_image_pool(&device,&key.pool,&desc));CHECK(!nn_nvn_error());
 CHECK(find(&key.pool,POOL)->external&&find(&device,DEVICE)->refs==1);
 unsigned before=import_calls;last_error=E2BIG;
 CHECK(nn_nvn_import_image_pool(&device,&key.pool,&desc)==EEXIST&&import_calls==before&&nn_nvn_error()==E2BIG);
 nn_nvn_clear_error();api_nvnMemoryPoolFinalize(&key.pool);CHECK(!nn_nvn_error()&&owner.calls==1);
 api_nvnDeviceFinalize(&device);CHECK(!nn_nvn_error());
 while(objects){CHECK(builder_kind(objects->kind));remove_object(objects);}return 0;
}
int main(void)
{
 if(module_cases()||core_cases(2)||core_cases(3)||ordinary_case()||isolated_quarantine()||recycled_import())return 1;
 printf("{\"passed\":true,\"checks\":%u,\"owner_releases\":%u,\"GPU_execution\":false}\n",checks,owner_releases);return 0;
}
