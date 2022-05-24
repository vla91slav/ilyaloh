// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import android.annotation.TargetApi;
import android.hardware.display.DisplayManager;
import android.view.Choreographer;
import android.view.Display;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import io.flutter.embedding.engine.FlutterJNI;

// TODO(mattcarroll): add javadoc.
public class VsyncWaiter {
  @TargetApi(17)
  class DisplayListener implements DisplayManager.DisplayListener {
    DisplayListener(DisplayManager displayManager) {
      this.displayManager = displayManager;
    }

    private DisplayManager displayManager;

    void register() {
      displayManager.registerDisplayListener(this, null);
    }

    @Override
    public void onDisplayAdded(int displayId) {}

    @Override
    public void onDisplayRemoved(int displayId) {}

    @Override
    public void onDisplayChanged(int displayId) {
      if (displayId == Display.DEFAULT_DISPLAY) {
        final Display primaryDisplay = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
        float fps = primaryDisplay.getRefreshRate();
        VsyncWaiter.this.refreshPeriodNanos = (long) (1000000000.0 / fps);
        VsyncWaiter.this.flutterJNI.setRefreshRateFPS(fps);
      }
    }
  }

  private static VsyncWaiter instance;
  private static DisplayListener listener;
  private long refreshPeriodNanos = -1;
  private FlutterJNI flutterJNI;

  @NonNull
  public static VsyncWaiter getInstance(float fps, FlutterJNI flutterJNI) {
    if (instance == null) {
      instance = new VsyncWaiter(flutterJNI);
    }
    flutterJNI.setRefreshRateFPS(fps);
    instance.refreshPeriodNanos = (long) (1000000000.0 / fps);
    return instance;
  }

  @TargetApi(17)
  @NonNull
  public static VsyncWaiter getInstance(DisplayManager displayManager, FlutterJNI flutterJNI) {
    if (instance == null) {
      instance = new VsyncWaiter(flutterJNI);
    }
    if (listener == null) {
      listener = instance.new DisplayListener(displayManager);
      listener.register();
    }
    if (instance.refreshPeriodNanos == -1) {
      final Display primaryDisplay = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
      float fps = primaryDisplay.getRefreshRate();
      instance.refreshPeriodNanos = (long) (1000000000.0 / fps);
      flutterJNI.setRefreshRateFPS(fps);
    }
    return instance;
  }

  // For tests, to reset the singleton between tests.
  @VisibleForTesting
  public static void reset() {
    instance = null;
    listener = null;
  }

  private final FlutterJNI.AsyncWaitForVsyncDelegate asyncWaitForVsyncDelegate =
      new FlutterJNI.AsyncWaitForVsyncDelegate() {
        @Override
        public void asyncWaitForVsync(long cookie) {
          Choreographer.getInstance()
              .postFrameCallback(
                  new Choreographer.FrameCallback() {
                    @Override
                    public void doFrame(long frameTimeNanos) {
                      long delay = System.nanoTime() - frameTimeNanos;
                      if (delay < 0) {
                        delay = 0;
                      }
                      flutterJNI.onVsync(delay, refreshPeriodNanos, cookie);
                    }
                  });
        }
      };

  private VsyncWaiter(FlutterJNI flutterJNI) {
    this.flutterJNI = flutterJNI;
  }

  public void init() {
    flutterJNI.setAsyncWaitForVsyncDelegate(asyncWaitForVsyncDelegate);
  }
}
