#pragma once
/* HOST TEST ONLY. Never used by Android shared target. */
typedef struct ANativeWindow {int width,height;} ANativeWindow;
int32_t ANativeWindow_getWidth(ANativeWindow *);
int32_t ANativeWindow_getHeight(ANativeWindow *);
