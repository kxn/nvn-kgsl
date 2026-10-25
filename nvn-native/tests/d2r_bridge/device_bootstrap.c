/* Host-only scalar/ownership regression. This includes the real core solely to
 * inspect builder/device snapshots and inject a bad builder scalar. Link the
 * existing memory_backend: it cannot execute GPU submissions. No runtime getter
 * or copied driver implementation is introduced. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef NN_TEST_NVN_SOURCE
#define NN_TEST_NVN_SOURCE "nvn-native/src/nvn.c"
#endif
#include NN_TEST_NVN_SOURCE

_Static_assert(sizeof(int) == 4, "NVN integer ABI width");
static unsigned checks, rejected_values, version_queries, ownership_busy_checks;
static unsigned backend_opens, backend_closes;
static int fail_next_open;
/* Linker interposition observes calls to the existing host memory backend and
 * provides one deterministic ENOMEM failure. Normal cases invoke it unchanged. */
int __real_nn_device_open(nn_device **out);
int __real_nn_device_close(nn_device *device);
int __wrap_nn_device_open(nn_device **out)
{
    ++backend_opens;
    if(fail_next_open) { int e=fail_next_open; fail_next_open=0; *out=NULL; return e; }
    return __real_nn_device_open(out);
}
int __wrap_nn_device_close(nn_device *device)
{ ++backend_closes; return __real_nn_device_close(device); }
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr,"FAIL line %d: %s (diagnostic=%d)\n",__LINE__,#expr,nn_nvn_error()); return 1; \
} } while (0)
#define LOAD(var,type,name) type var=(type)nvnBootstrapLoader(name); CHECK(var != NULL)
#define CLEAN() do { CHECK(nn_nvn_error()==0); } while(0)
#define EXPECT_ERROR(value) do { CHECK(nn_nvn_error()==(value)); nn_nvn_clear_error(); } while(0)

/* Lookup without changing diagnostics; negative tests must observe the API's
 * own error and absence/preservation of its object, not find() side effects. */
static struct object *peek(const void *key)
{
    for (struct object *o=objects;o;o=o->next) if(o->key==key) return o;
    return NULL;
}
struct guarded { uint32_t before[3]; int value; uint32_t after[3]; };
static struct guarded guard(void)
{
    struct guarded g={{0x13579bdf,0x2468ace0,0x5aa55aa5},0x12345678,
                      {0x55aa55aa,0x0badc0de,0xfedcba98}};
    return g;
}
static int query(PFNNVNDEVICEGETINTEGERPROC get,const NVNdevice *d,int property,int expected)
{
    struct guarded g=guard(), want=g;
    want.value=expected;
    get(d,(NVNdeviceInfo)property,&g.value);
    CHECK(nn_nvn_error()==0);
    CHECK(memcmp(&g,&want,sizeof(g))==0);
    ++version_queries;
    return 0;
}
static int query_error(PFNNVNDEVICEGETINTEGERPROC get,const NVNdevice *d,int property,int error_code)
{
    struct guarded g=guard(),want=g;
    get(d,(NVNdeviceInfo)property,&g.value);
    CHECK(nn_nvn_error()==error_code);
    CHECK(memcmp(&g,&want,sizeof(g))==0);
    nn_nvn_clear_error();
    return 0;
}
int main(void)
{
    LOAD(defaults,PFNNVNDEVICEBUILDERSETDEFAULTSPROC,"nvnDeviceBuilderSetDefaults");
    LOAD(setflags,PFNNVNDEVICEBUILDERSETFLAGSPROC,"nvnDeviceBuilderSetFlags");
    LOAD(initialize,PFNNVNDEVICEINITIALIZEPROC,"nvnDeviceInitialize");
    LOAD(finalize,PFNNVNDEVICEFINALIZEPROC,"nvnDeviceFinalize");
    LOAD(get,PFNNVNDEVICEGETINTEGERPROC,"nvnDeviceGetInteger");
    LOAD(qdefaults,PFNNVNQUEUEBUILDERSETDEFAULTSPROC,"nvnQueueBuilderSetDefaults");
    LOAD(qdevice,PFNNVNQUEUEBUILDERSETDEVICEPROC,"nvnQueueBuilderSetDevice");
    LOAD(qinitialize,PFNNVNQUEUEINITIALIZEPROC,"nvnQueueInitialize");
    LOAD(qfinalize,PFNNVNQUEUEFINALIZEPROC,"nvnQueueFinalize");
    NVNdeviceBuilder b={0},never_builder={0};
    NVNdevice zero_device={0},separate_device={0},failed_device={0},never_device={0};
    NVNqueueBuilder qb={0}; NVNqueue queue={0};
    CLEAN();
    CHECK(nvnBootstrapLoader("nvnDeviceBuilderGetFlags")==NULL);
    CLEAN(); /* Missing runtime getter must stay absent. */

    defaults(&b); CLEAN(); CHECK(peek(&b)&&peek(&b)->kind==DB&&peek(&b)->flags==0);
    setflags(&b,0x100); CLEAN(); CHECK(peek(&b)->flags==0x100);
    setflags(&b,0); CLEAN(); CHECK(peek(&b)->flags==0);
    setflags(&b,0x100); CLEAN(); CHECK(peek(&b)->flags==0x100);
    defaults(&b); CLEAN(); CHECK(peek(&b)->flags==0);
    defaults(&b); CLEAN(); CHECK(peek(&b)->flags==0);
    CHECK(initialize(&zero_device,&b)); CLEAN();
    CHECK(peek(&zero_device)&&peek(&zero_device)->flags==0);
    setflags(&b,0x100); CLEAN(); CHECK(peek(&zero_device)->flags==0);
    CHECK(initialize(&separate_device,&b)); CLEAN();
    CHECK(peek(&separate_device)&&peek(&separate_device)->flags==0x100);
    setflags(&b,0); CLEAN(); CHECK(peek(&separate_device)->flags==0x100);
    defaults(&b); CLEAN(); CHECK(peek(&separate_device)->flags==0x100);
    setflags(&b,0x100); CLEAN();

    const int rejected[]={1,2,4,8,0x10,0x20,0x40,0x80,0x115,
        0x101,0x102,0x104,0x108,0x110,0x120,0x140,0x180,0x1ff,0x200,-1,INT_MIN};
    for(size_t i=0;i<sizeof(rejected)/sizeof(rejected[0]);++i) {
        setflags(&b,rejected[i]); EXPECT_ERROR(ENOTSUP);
        CHECK(peek(&b)->flags==0x100); ++rejected_values;
    }
    setflags(NULL,0); EXPECT_ERROR(EINVAL);
    setflags(&never_builder,0x100); EXPECT_ERROR(EINVAL); CHECK(!peek(&never_builder));
    qdefaults(&qb); CLEAN();
    setflags((NVNdeviceBuilder *)&qb,0x100); EXPECT_ERROR(EINVAL);
    CHECK(peek(&qb)->kind==QB && peek(&qb)->flags==0);
    setflags((NVNdeviceBuilder *)&zero_device,0x100); EXPECT_ERROR(EINVAL);
    CHECK(peek(&zero_device)->kind==DEVICE && peek(&zero_device)->flags==0);
    defaults(NULL); EXPECT_ERROR(EINVAL);
    CHECK(!initialize(&failed_device,NULL)); EXPECT_ERROR(EINVAL); CHECK(!peek(&failed_device));
    CHECK(!initialize(&failed_device,&never_builder)); EXPECT_ERROR(EINVAL); CHECK(!peek(&failed_device));
    CHECK(!initialize(&failed_device,(NVNdeviceBuilder *)&qb)); EXPECT_ERROR(EINVAL); CHECK(!peek(&failed_device));
    CHECK(!initialize(NULL,&b)); EXPECT_ERROR(EINVAL);

    /* White-box adversarial persisted builder state: bypass setter only here.
     * Initialize itself must reject it before creating a device/backend. */
    unsigned opens_before=backend_opens;
    peek(&b)->flags=0x115;
    CHECK(!initialize(&failed_device,&b)); EXPECT_ERROR(ENOTSUP);
    CHECK(!peek(&failed_device)); CHECK(peek(&b)->flags==0x115);
    CHECK(backend_opens==opens_before);
    CHECK(!query_error(get,&failed_device,0,EINVAL));
    peek(&b)->flags=0x200;
    CHECK(!initialize(&failed_device,&b)); EXPECT_ERROR(ENOTSUP); CHECK(!peek(&failed_device));
    CHECK(backend_opens==opens_before);
    defaults(&b); CLEAN(); CHECK(peek(&b)->flags==0);
    fail_next_open=ENOMEM;
    CHECK(!initialize(&failed_device,&b)); EXPECT_ERROR(ENOMEM);
    CHECK(!peek(&failed_device)); CHECK(fail_next_open==0);
    CHECK(backend_opens==opens_before+1);
    CHECK(!query_error(get,&failed_device,1,EINVAL));

    CHECK(!query(get,&zero_device,0,53)); CHECK(!query(get,&zero_device,1,105));
    CHECK(!query(get,&separate_device,1,105)); CHECK(!query(get,&separate_device,0,53));
    CHECK(!query(get,&separate_device,0,53)); CHECK(!query(get,&separate_device,1,105));
    CHECK(!query_error(get,NULL,0,EINVAL));
    CHECK(!query_error(get,&never_device,0,EINVAL));
    CHECK(!query_error(get,(NVNdevice *)&qb,1,EINVAL));
    get(&zero_device,NVN_DEVICE_INFO_API_MAJOR_VERSION,NULL); EXPECT_ERROR(EINVAL);
    CHECK(!query_error(get,&zero_device,0x7ffffffe,ENOTSUP));
    CHECK(!query_error(get,&zero_device,-1,ENOTSUP));
    CHECK(!query_error(get,&zero_device,NVN_DEVICE_INFO_UNIFORM_BUFFER_BINDINGS_PER_STAGE,ENOTSUP));

    /* A real core queue owns a device reference. The backend is host memory
     * only: this covers live-child EBUSY, not GPU inflight/unknown completion. */
    qdevice(&qb,&separate_device); CLEAN();
    CHECK(qinitialize(&queue,&qb)); CLEAN(); CHECK(peek(&separate_device)->refs>0);
    finalize(&separate_device); EXPECT_ERROR(EBUSY); ++ownership_busy_checks;
    CHECK(peek(&separate_device)!=NULL);
    CHECK(!query(get,&separate_device,0,53)); CHECK(!query(get,&separate_device,1,105));
    qfinalize(&queue); CLEAN(); CHECK(!peek(&queue));
    finalize(&separate_device); CLEAN(); CHECK(!peek(&separate_device));
    CHECK(!query_error(get,&separate_device,0,EINVAL));
    finalize(&separate_device); EXPECT_ERROR(EINVAL);

    /* Reuse the same public object storage; identity is fixed, flags are a new
     * snapshot rather than stale state inherited from its previous lifetime. */
    CHECK(initialize(&separate_device,&b)); CLEAN();
    CHECK(peek(&separate_device)->flags==0);
    CHECK(!query(get,&separate_device,1,105)); CHECK(!query(get,&separate_device,0,53));
    setflags(&b,0x100); CLEAN(); CHECK(peek(&separate_device)->flags==0);
    CHECK(!query(get,&separate_device,0,53));

    /* Distinguishable diagnostics prove a successful call and later failure do
     * not erase/replace an earlier error; only this harness resets between cases. */
    setflags(&b,0x115); CHECK(nn_nvn_error()==ENOTSUP);
    get(&zero_device,NVN_DEVICE_INFO_API_MAJOR_VERSION,NULL);
    CHECK(nn_nvn_error()==ENOTSUP);
    struct guarded g=guard(),want=g; want.value=105;
    get(&zero_device,NVN_DEVICE_INFO_API_MINOR_VERSION,&g.value);
    CHECK(nn_nvn_error()==ENOTSUP); CHECK(memcmp(&g,&want,sizeof(g))==0);
    setflags(&b,0); CHECK(nn_nvn_error()==ENOTSUP); CHECK(peek(&b)->flags==0);
    nn_nvn_clear_error(); CLEAN();

    finalize(&zero_device); CLEAN(); CHECK(!peek(&zero_device));
    CHECK(!query_error(get,&zero_device,1,EINVAL));
    finalize(&separate_device); CLEAN(); CHECK(!peek(&separate_device));
    CHECK(!handles); CHECK(backend_opens==4); CHECK(backend_closes==3);
    /* Builders have no NVN finalize entry. Reclaim only their side-table entries
     * for leak-checker hygiene, after all API assertions have completed. */
    while(objects) { CHECK(objects->kind==DB || objects->kind==QB); remove_object(objects); }
    CLEAN();
    printf("{\"operation\":\"device_bootstrap_host_semantics\",\"passed\":true,"
           "\"checks\":%u,\"rejected_flag_values\":%u,\"version_queries\":%u,"
           "\"ownership_busy_checks\":%u,\"gpu_submissions\":0,"
           "\"gpu_execution\":false,\"original_game_executed\":false,"
           "\"backend\":\"existing_host_memory_backend\","
           "\"profile\":\"nn-nvn-partial-abi53.105-v1\"}\n",
           checks,rejected_values,version_queries,ownership_busy_checks);
    return 0;
}
