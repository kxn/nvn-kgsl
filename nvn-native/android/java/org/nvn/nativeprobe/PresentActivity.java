package org.nvn.nativeprobe;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

/** Supply a SurfaceView-backed window while retaining NativeActivity's native
 * SurfaceHolder callbacks, input queue and lifecycle bridge. Android 13/14's
 * root NativeActivity window may lack the handle needed by createFromWindow. */
public final class PresentActivity extends NativeActivity {
    public volatile int probeSurfaceX, probeSurfaceY;
    private SurfaceView probeView;
    private void locateSurface() {
        int[] location = new int[2];
        probeView.getLocationOnScreen(location);
        probeSurfaceX = location[0];
        probeSurfaceY = location[1];
    }
    @Override public void surfaceCreated(SurfaceHolder holder) {
        locateSurface();
        super.surfaceCreated(holder);
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        locateSurface();
        super.surfaceChanged(holder, format, width, height);
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().takeSurface(null);
        probeView = new SurfaceView(this);
        probeView.getHolder().addCallback(this);
        setContentView(probeView);
    }
}
