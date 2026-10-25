/* Synthetic forwarding/diagnostic tests only: no guest or GPU execution. */
#include "host_bridge.h"
#include "nvn_native/nvn.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int diagnostic, missing, fail_after;
static NVNboolean initialize_result = 0xa5;
static const char *const apis[] = {
    "nvnDeviceSetWindowOriginMode", "nvnDeviceSetDepthMode",
    "nvnMemoryPoolBuilderSetFlags", "nvnMemoryPoolBuilderSetStorage",
    "nvnMemoryPoolInitialize"
};
/* Synthetic driver has no native pool-policy diagnostic. */
int nn_nvn_get_pool_access_observation(struct nn_nvn_pool_access_observation *out)
{ (void)out; return ENODATA; }
int nn_nvn_error(void) { return diagnostic; }
void nn_nvn_clear_error(void) { diagnostic = 0; }
static void raw_called(void)
{
    fputs("[test raw] called\n", stderr);
    if (fail_after) diagnostic = ENOTSUP;
}
static void window_origin(NVNdevice *device, NVNwindowOriginMode mode)
{ assert(device == (void *)0x1234 && (int)mode == -177); raw_called(); }
static void depth_mode(NVNdevice *device, NVNdepthMode mode)
{ assert(device == (void *)0x1234 && (int)mode == -178); raw_called(); }
static void pool_flags(NVNmemoryPoolBuilder *builder, int flags)
{ assert(builder == (void *)0x1234 && (uint32_t)flags == UINT32_C(0xfedcba98)); raw_called(); }
static void pool_storage(NVNmemoryPoolBuilder *builder, void *memory, size_t size)
{
    assert(builder == (void *)0x1234 && (uintptr_t)memory == UINT64_C(0x123456789abc));
    assert(size == UINT64_C(0x123456789)); raw_called();
}
static NVNboolean pool_initialize(NVNmemoryPool *pool, const NVNmemoryPoolBuilder *builder)
{ assert(pool == (void *)0x5678 && builder == (void *)0x1234); raw_called(); return initialize_result; }
PFNNVNGENERICFUNCPTRPROC nvnBootstrapLoader(const char *name)
{
    if (missing) return NULL;
#define PROVIDE(n, f, type) if (!strcmp(name, n)) { type check = f; return (PFNNVNGENERICFUNCPTRPROC)check; }
    PROVIDE(apis[0], window_origin, PFNNVNDEVICESETWINDOWORIGINMODEPROC)
    PROVIDE(apis[1], depth_mode, PFNNVNDEVICESETDEPTHMODEPROC)
    PROVIDE(apis[2], pool_flags, PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC)
    PROVIDE(apis[3], pool_storage, PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC)
    PROVIDE(apis[4], pool_initialize, PFNNVNMEMORYPOOLINITIALIZEPROC)
#undef PROVIDE
    return NULL;
}
static void call(unsigned api, PFNNVNGENERICFUNCPTRPROC function)
{
    switch (api) {
    case 0: ((PFNNVNDEVICESETWINDOWORIGINMODEPROC)function)((void *)0x1234, (NVNwindowOriginMode)-177); break;
    case 1: ((PFNNVNDEVICESETDEPTHMODEPROC)function)((void *)0x1234, (NVNdepthMode)-178); break;
    case 2: ((PFNNVNMEMORYPOOLBUILDERSETFLAGSPROC)function)((void *)0x1234, (int)UINT32_C(0xfedcba98)); break;
    case 3: ((PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC)function)((void *)0x1234, (void *)(uintptr_t)UINT64_C(0x123456789abc), UINT64_C(0x123456789)); break;
    case 4: assert(((PFNNVNMEMORYPOOLINITIALIZEPROC)function)((void *)0x5678, (void *)0x1234) == initialize_result); break;
    default: assert(0);
    }
}
static void check_fatal(unsigned api, unsigned failure)
{
    int descriptors[2]; assert(!pipe(descriptors)); pid_t pid = fork(); assert(pid >= 0);
    if (!pid) {
        close(descriptors[0]); assert(dup2(descriptors[1], STDERR_FILENO) == STDERR_FILENO); close(descriptors[1]);
        PFNNVNGENERICFUNCPTRPROC function = nn_d2r_nvn_get_proc_address(NULL, apis[api]);
        if (failure == 0) missing = 1;
        if (failure == 1) diagnostic = EBUSY;
        if (failure == 2) fail_after = 1;
        call(api, function); _exit(91);
    }
    close(descriptors[1]); char text[4096]; size_t size = 0; ssize_t got;
    while (size < sizeof(text) - 1 && (got = read(descriptors[0], text + size, sizeof(text) - 1 - size)) > 0) size += (size_t)got;
    close(descriptors[0]); text[size] = 0; int status;
    assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 78);
    const char *observation = strstr(text, apis[api]), *fatal = strstr(text, "terminating execution");
    assert(observation && fatal && observation < fatal && strstr(text, "guest_pc=unavailable"));
    char expected[80]; snprintf(expected, sizeof(expected), "errno=%d; phase=%s", failure == 0 ? ENOSYS : failure == 1 ? EBUSY : ENOTSUP, failure == 0 ? "call" : failure == 1 ? "before" : "after");
    assert(strstr(text, expected));
    assert((strstr(text, "[test raw] called") != NULL) == (failure == 2));
    if (api == 2) assert(strstr(text, "flags=0xfedcba98"));
    if (api == 3) assert(strstr(text, "storage=0x123456789abc size=4886718345"));
    if (api == 4) assert(!strstr(text, "result="));
    fputs(text, stdout);
}
int main(void)
{
    for (unsigned api = 0; api < sizeof(apis) / sizeof(apis[0]); api++) {
        PFNNVNGENERICFUNCPTRPROC function = nn_d2r_nvn_bootstrap(apis[api]);
        assert(function != nvnBootstrapLoader(apis[api]));
        assert(function == nn_d2r_nvn_get_proc_address(NULL, apis[api]));
        call(api, function); assert(!diagnostic);
        for (unsigned failure = 0; failure < 3; failure++) check_fatal(api, failure);
    }
    initialize_result = 0;
    call(4, nn_d2r_nvn_bootstrap(apis[4])); assert(!diagnostic);
    puts("PASS 5 observed typed APIs: exact enum/flags/pointer/64-bit size/bool carriers; false result legal; pre-lookup observation; missing/old-sticky/post-error 15 fatal children; no fabricated guest PC. HOST ONLY");
}
