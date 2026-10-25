#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hle.h"
#include <stdatomic.h>
#include <time.h>
#include "hle_oe.c"
#include "../activity_host.c"
uint64_t now_ns(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))abort();return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;}
pthread_key_t g_keyres[2];
int dm_tsd_set(pthread_key_t key,const void *value){return pthread_setspecific(key,value);}
uint64_t front_game_res(int docked);
uint64_t dm_game_res(int docked){return front_game_res(docked);}
static unsigned checks,register_calls,attach_calls,close_calls,main_calls,lookups;
static int register_error,attach_error,close_error,main_result,close_in_main;
static int expected_stage;
static const void *registered_token,*attached_token;
static void *attached_window;
static uint64_t next_generation=11;
#define T(x) do{++checks;if(!(x)){fprintf(stderr,"line%u %s\n",__LINE__,#x);return 1;}}while(0)
int32_t ANativeWindow_getWidth(ANativeWindow *w){return w->width;}
int32_t ANativeWindow_getHeight(ANativeWindow *w){return w->height;}
int nn_window_registry_register(const void *t){register_calls++;registered_token=t;return register_error;}
int nn_window_registry_attach(const void *t,void *w,uint64_t *g){attach_calls++;attached_token=t;attached_window=w;*g=0;if(attach_error)return attach_error;*g=next_generation++;return 0;}
int nn_window_registry_close(const void *t,uint64_t g,uint32_t ms){close_calls++;if(t!=registered_token||g!=attached_generation||ms>1500)return EINVAL;return close_error;}
int dm_android_main(int n,char **v){main_calls++;if(n!=3||strcmp(v[1],"original-modules")||strcmp(v[2],expected_stage==1?"--sdk-init":expected_stage==2?"--constructors":"--run")||v[3])return EINVAL;if(close_in_main&&nn_d2r_activity_host_close(attached_generation,0))return EIO;return main_result;}
static PFNNVNGENERICFUNCPTRPROC get(const NVNdevice *d,const char *name){if(d||strcmp(name,"nvnDeviceBuilderSetDefaults"))return NULL;lookups++;return (PFNNVNGENERICFUNCPTRPROC)get;}
PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_bootstrap(const char *name){return strcmp(name,"nvnDeviceGetProcAddress")?NULL:(PFNNVNGENERICFUNCPTRPROC)get;}
PFNNVNGENERICFUNCPTRPROC nn_d2r_nvn_get_proc_address(const NVNdevice *d,const char *name){return get(d,name);}
static void reset(void){attached_generation=0;started=closed=0;register_calls=attach_calls=close_calls=main_calls=lookups=0;register_error=attach_error=close_error=main_result=close_in_main=0;registered_token=attached_token=NULL;attached_window=NULL;next_generation=11;}
static _Atomic int producers_done,consumer_bad;
static void *focus_producer(void *arg){
 intptr_t seed=(intptr_t)arg;for(int i=0;i<64;i++)if(nn_d2r_activity_host_notify_focus(2+(int)((i+seed)&1)))atomic_store(&consumer_bad,1);
 atomic_fetch_add(&producers_done,1);return NULL;
}
static void *focus_consumer(void *unused){
 (void)unused;do{uint32_t message=999;if(h_oe_TryPopMsg(&message)&&message!=15)atomic_store(&consumer_bad,1);}while(atomic_load(&producers_done)!=2);return NULL;
}
static int focus_checks(void){
 reset();dm_focus_set(1);uint32_t message;while(h_oe_TryPopMsg(&message)){}
 T(nn_d2r_activity_host_notify_focus(0)==EINVAL);T(nn_d2r_activity_host_notify_focus(4)==EINVAL);T(nn_d2r_activity_host_notify_focus(-1)==EINVAL);
 T(h_oe_GetFocus()==1&&!h_oe_TryPopMsg(&message));
 T(nn_d2r_activity_host_notify_focus(1)==0&&!h_oe_TryPopMsg(&message));
 T(nn_d2r_activity_host_notify_focus(3)==0&&h_oe_GetFocus()==3);
 T(h_oe_TryPopMsg(&message)&&message==15);T(!h_oe_TryPopMsg(&message));
 T(nn_d2r_activity_host_notify_focus(3)==0&&!h_oe_TryPopMsg(&message));
 T(nn_d2r_activity_host_notify_focus(2)==0);T(nn_d2r_activity_host_notify_focus(1)==0);
 T(h_oe_GetFocus()==1&&h_oe_TryPopMsg(&message)&&message==15&&!h_oe_TryPopMsg(&message));
 pthread_t a,b,c;T(!pthread_create(&c,NULL,focus_consumer,NULL));T(!pthread_create(&a,NULL,focus_producer,(void *)0));T(!pthread_create(&b,NULL,focus_producer,(void *)1));
 T(!pthread_join(a,NULL));T(!pthread_join(b,NULL));T(!pthread_join(c,NULL));T(!atomic_load(&consumer_bad));
 T(nn_d2r_activity_host_notify_focus(1)==0&&h_oe_GetFocus()==1);
 T(h_oe_TryPopMsg(&message)&&message==15);T(!h_oe_TryPopMsg(&message));
 T(!register_calls&&!attach_calls&&!close_calls&&!main_calls&&!lookups);
 return 0;
}
int main(void){
 const void *token=nn_d2r_activity_host_token();T(token&&token==nn_d2r_activity_host_token());
 int found=0;for(const hle_force_ent *e=(const void *)__start_swforce;e<(const hle_force_ent *)(const void *)__stop_swforce;e++)if(!strcmp(e->name,"_ZN2nn2vi15GetNativeWindowEPPvPNS0_5LayerE")){
  Result (*f)(void**,void*)=(Result(*)(void**,void*))e->fn;void *out=NULL;T(f(&out,NULL)==RES_OK&&out==token);found++;
 }T(found==1);
 ANativeWindow w={1280,720};uint64_t gen=77;
 reset();T(nn_d2r_activity_host_attach(NULL,NULL,&gen)==EINVAL&&gen==0);T(nn_d2r_activity_host_attach(&w,NULL,NULL)==EINVAL);T(register_calls==0&&attach_calls==0);
 reset();w.width=0;T(nn_d2r_activity_host_attach(&w,NULL,&gen)==EINVAL&&gen==0&&register_calls==0);w.width=1280;
 reset();T(nn_d2r_activity_host_attach(&w,"garbage",&gen)==EINVAL&&gen==0&&register_calls==0);
 reset();register_error=ENOSPC;T(nn_d2r_activity_host_attach(&w,NULL,&gen)==ENOSPC&&gen==0&&attach_calls==0);
 reset();attach_error=ENOMEM;T(nn_d2r_activity_host_attach(&w,NULL,&gen)==ENOMEM&&gen==0&&attach_calls==1);attach_error=0;T(nn_d2r_activity_host_attach(&w,NULL,&gen)==0&&gen==11);T(registered_token==token&&attached_token==token&&attached_window==&w);
 T(nn_d2r_activity_host_attach(&w,NULL,&gen)==EBUSY&&gen==0&&attach_calls==2);
 T(nn_d2r_activity_host_close(999,0)==ESTALE&&close_calls==0&&!closed);
 T(nn_d2r_activity_host_close(11,1501)==EINVAL&&close_calls==0);
 T(nn_d2r_activity_host_close(11,0)==0&&close_calls==1&&closed);
 T(nn_d2r_activity_host_attach(&w,NULL,&gen)==0&&gen==12);T(!closed);
 T(nn_d2r_activity_host_check_bootstrap()==0&&lookups==2);
 for(int stage=1;stage<=3;stage++){
  reset();T(nn_d2r_activity_host_run("original-modules",stage)==EAGAIN&&main_calls==0);
  T(nn_d2r_activity_host_attach(&w,NULL,&gen)==0);
  expected_stage=stage;main_result=stage==3?7:0;close_in_main=1;
  T(nn_d2r_activity_host_run("original-modules",stage)==main_result&&main_calls==1&&close_calls==1);
  T(nn_d2r_activity_host_run("original-modules",stage)==EALREADY&&main_calls==1);
  T(nn_d2r_activity_host_attach(&w,NULL,&gen)==ENOTSUP&&gen==0);
 }
 reset();T(nn_d2r_activity_host_run(NULL,1)==EINVAL&&main_calls==0);T(nn_d2r_activity_host_run("",2)==EINVAL);T(nn_d2r_activity_host_run("original-modules",0)==EINVAL);T(nn_d2r_activity_host_run("original-modules",4)==EINVAL);
 reset();T(nn_d2r_activity_host_attach(&w,NULL,&gen)==0);close_error=EIO;T(nn_d2r_activity_host_close(gen,0)==EIO&&closed);T(nn_d2r_activity_host_run("original-modules",1)==EAGAIN&&main_calls==0);
 T(focus_checks()==0);
 printf("{\"checks\":%u,\"host_only\":true,\"original_vi_identity\":true,\"original_modules_executed\":false,\"gpu_executed\":false}\n",checks);return 0;
}
