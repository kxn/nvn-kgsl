/* Pure packet/ABI inspection, not CP execution or a counter simulator. */
#include "nvn_native/counter.h"
#include "nvn_native/pm4.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++;if(!(x)){fprintf(stderr,"line%d %s\n",__LINE__,#x);return 1;} } while(0)
static uint64_t pair(const uint32_t *w){return w[0]|(uint64_t)w[1]<<32;}
int main(void)
{
    uint32_t w[128],before[128];size_t n=99;uint64_t state=UINT64_C(0x1234500000),dst=UINT64_C(0x6789a00000);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_RESET_SAMPLES,state,0,NULL,0,&n));CHECK(n==29);
    memset(w,0xa5,sizeof(w));memcpy(before,w,sizeof(w));CHECK(nn_counter_words(0x06050002,NN_COUNTER_RESET_SAMPLES,state,0,w,n-1,&n)==ENOSPC);CHECK(n==0&&!memcmp(w,before,sizeof(w)));
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_RESET_SAMPLES,state,0,w,128,&n));CHECK(n==29);
    CHECK(w[0]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,4)&&pair(w+1)==state+32&&!w[3]&&!w[4]);
    CHECK(w[5]==nvn_pm4_packet7(0x12,0));
    CHECK(w[6]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0));CHECK(w[7]==nvn_pm4_packet7(NVN_CP_MEM_WRITE,4));CHECK(pair(w+8)==state);CHECK(w[10]==UINT32_MAX&&w[11]==UINT32_MAX);
    CHECK(w[13]==nvn_pm4_packet4(0x8891,1)&&w[14]==2);CHECK(w[15]==nvn_pm4_packet4(0x8927,2)&&pair(w+16)==state);
    CHECK(w[18]==nvn_pm4_packet7(0x46,1)&&w[19]==0x15);CHECK(w[20]==nvn_pm4_packet7(0x3c,6)&&w[21]==0x14);CHECK(pair(w+22)==state&&w[24]==UINT32_MAX&&w[25]==UINT32_MAX&&w[26]==16);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_BEGIN_SAMPLES,state,0,w,128,&n));CHECK(n==23);
    CHECK(w[0]==nvn_pm4_packet7(NVN_CP_WAIT_FOR_IDLE,0)&&pair(w+2)==state&&pair(w+10)==state);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_REPORT_SAMPLES,state,dst,w,128,&n));CHECK(n==42);
    CHECK(pair(w+2)==state+16&&pair(w+10)==state+16&&pair(w+16)==state+16);
    CHECK(w[21]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,9));CHECK(w[22]==((1u<<29)|4u|(1u<<30)));CHECK(pair(w+23)==dst&&pair(w+25)==state+32&&pair(w+27)==state+16&&pair(w+29)==state);
    CHECK(w[31]==nvn_pm4_packet7(0x46,4)&&w[32]==(0x16u|(1u<<30)));CHECK(pair(w+33)==dst+8&&w[35]==0);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_END_SAMPLES,state,0,w,128,&n));CHECK(n==36);
    CHECK(w[21]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,9)&&pair(w+23)==state+32&&pair(w+25)==state+32&&pair(w+27)==state+16&&pair(w+29)==state);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_REPORT_TIMESTAMP,0,dst,w,128,&n));CHECK(n==17);CHECK(w[0]==nvn_pm4_packet7(0x46,4)&&w[1]==(0x16u|(1u<<30)));CHECK(pair(w+2)==dst);CHECK(w[6]==nvn_pm4_packet7(NVN_CP_MEM_TO_MEM,5)&&w[7]==1u<<29&&pair(w+8)==dst+8&&pair(w+10)==dst);
    CHECK(!nn_counter_words(0x06050002,NN_COUNTER_REPORT_TIMESTAMP_TOP,0,dst,w,128,&n));CHECK(n==16);CHECK(w[0]==nvn_pm4_packet7(0x3e,3));CHECK(w[1]==(0x980u|(2u<<18)|(1u<<30)));CHECK(pair(w+2)==dst);
    /* Walk complete generated packets, not only selected control words. */
    const size_t expected[]={29,42,17,16,23,36};
    for(unsigned op=0;op<6;op++){
        CHECK(!nn_counter_words(0x06050002,(enum nn_counter_operation)op,state,dst,w,128,&n)&&n==expected[op]);
        size_t at=0;while(at<n){unsigned length=nvn_pm4_packet_words(w[at]);CHECK(length&&length<=n-at);at+=length;}CHECK(at==n);
    }
    memcpy(before,w,sizeof(w));
    struct {uint32_t chip;enum nn_counter_operation op;uint64_t state,dst;int error;} bad[]={
        {0x43030b00,NN_COUNTER_REPORT_SAMPLES,state,dst,ENOTSUP},
        {0x06050002,6,state,dst,ENOTSUP},{0x06050002,(enum nn_counter_operation)-1,state,dst,ENOTSUP},
        {0x06050002,NN_COUNTER_RESET_SAMPLES,0,0,EINVAL},{0x06050002,NN_COUNTER_RESET_SAMPLES,state+4,0,EINVAL},
        {0x06050002,NN_COUNTER_REPORT_SAMPLES,state,state+16,EINVAL},{0x06050002,NN_COUNTER_REPORT_SAMPLES,state,0,EINVAL},
        {0x06050002,NN_COUNTER_REPORT_SAMPLES,state,dst+8,EINVAL},{0x06050002,NN_COUNTER_REPORT_TIMESTAMP,0,UINT64_MAX-7,EINVAL}
    };
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){n=99;CHECK(nn_counter_words(bad[i].chip,bad[i].op,bad[i].state,bad[i].dst,w,128,&n)==bad[i].error);CHECK(n==0&&!memcmp(w,before,sizeof(w)));}
    uint64_t ns=UINT64_MAX;CHECK(!nn_counter_ticks_to_ns(0,&ns)&&ns==0);CHECK(!nn_counter_ticks_to_ns(12,&ns)&&ns==625);CHECK(!nn_counter_ticks_to_ns(19200000,&ns)&&ns==1000000000);CHECK(!nn_counter_ticks_to_ns(1,&ns)&&ns==52);CHECK(nn_counter_ticks_to_ns(UINT64_MAX,&ns)==EOVERFLOW&&ns==52);CHECK(nn_counter_ticks_to_ns(0,NULL)==EINVAL);
    printf("{\"passed\":true,\"checks\":%u,\"gpu_executes\":0,\"scope\":\"pure_packet_inspection\"}\n",checks);return 0;
}
