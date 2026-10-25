#pragma once
#include <stdatomic.h>
typedef struct ANativeWindow {_Atomic int refs;} ANativeWindow;
void ANativeWindow_acquire(ANativeWindow *);
void ANativeWindow_release(ANativeWindow *);
