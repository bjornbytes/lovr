package org.lovr.app;

import android.app.NativeActivity;

public class Activity extends NativeActivity {
  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("lovr");
  }
}
