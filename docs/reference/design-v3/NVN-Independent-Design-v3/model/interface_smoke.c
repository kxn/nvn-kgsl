#include "../interface/nvn_kgsl_contract.h"
#include <assert.h>
int main(void) {
 NvnnByteRange a={4,8}, b={12,0}, bad={UINT64_MAX-3,8};
 assert(nvnn_v3_range_contains(a,b));
 assert(!nvnn_v3_range_contains(bad,bad));
 NvnnCompletionPoint x={1,2,3,4,5,6,0,0}, y=x;
 assert(nvnn_v3_same_timeline(x,y));
 y.queue_generation++;assert(!nvnn_v3_same_timeline(x,y));
 y=x;y.device_generation++;assert(!nvnn_v3_same_timeline(x,y));
 NvnnChunkReceipt chunks[2]={{0}};
 NvnnSubmitReceipt r={0};r.chunks=chunks;r.chunk_capacity=2;r.chunk_count=2;
 r.first_unproven_chunk=NVNN_NO_CHUNK;
 assert(r.chunk_capacity>=r.chunk_count);
 return 0;
}
