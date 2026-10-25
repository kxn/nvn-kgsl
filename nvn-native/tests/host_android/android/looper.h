/* HOST callback/type declarations only; never replace NDK APIs. */
#ifdef __ANDROID__
#error "HOST Android callback declarations must never replace NDK headers"
#endif
#pragma once
typedef struct ALooper ALooper;
enum {ALOOPER_PREPARE_ALLOW_NON_CALLBACKS=1,ALOOPER_POLL_WAKE=-1,ALOOPER_POLL_CALLBACK=-2,ALOOPER_POLL_TIMEOUT=-3,ALOOPER_POLL_ERROR=-4};
ALooper *ALooper_prepare(int);
void ALooper_acquire(ALooper *);
void ALooper_release(ALooper *);
void ALooper_wake(ALooper *);
int ALooper_pollOnce(int,int *,int *,void **);
