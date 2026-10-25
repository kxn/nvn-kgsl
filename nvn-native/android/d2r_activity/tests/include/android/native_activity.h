#pragma once
#include <stddef.h>
#include "native_window.h"
typedef struct ANativeActivity ANativeActivity;
typedef struct ANativeActivityCallbacks {
 void (*onResume)(ANativeActivity *);void (*onPause)(ANativeActivity *);
 void (*onNativeWindowCreated)(ANativeActivity *,ANativeWindow *);
 void (*onNativeWindowDestroyed)(ANativeActivity *,ANativeWindow *);
 void (*onDestroy)(ANativeActivity *);
} ANativeActivityCallbacks;
struct ANativeActivity {void *instance;ANativeActivityCallbacks *callbacks;const char *internalDataPath;};
