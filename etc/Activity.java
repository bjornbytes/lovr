package @ANDROID_PACKAGE@;

import android.Manifest;
import android.app.NativeActivity;
import android.content.pm.PackageManager;
import android.util.Log;
import android.os.Build;

public class Activity extends NativeActivity {
  static {
    if (Build.MANUFACTURER.contains("Oculus")) {
      Log.d("LOVR", "Using Oculus OpenXR Loader");
      System.loadLibrary("openxr_loader_oculus");
    } else if (Build.MANUFACTURER.contains("Pico")) {
      Log.d("LOVR", "Using Pico OpenXR Loader");
      System.loadLibrary("openxr_loader_pico");
    } else {
      Log.d("LOVR", "Using Generic OpenXR Loader");
      System.loadLibrary("openxr_loader_generic");
    }
    System.loadLibrary("lovr");
  }

  protected native void lovrPermissionEvent(int permission, boolean granted);

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      lovrPermissionEvent(0, true);
    } else {
      lovrPermissionEvent(0, false);
    }
  }

  private void requestAudioCapturePermission() {
    if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      requestPermissions(new String[] { Manifest.permission.RECORD_AUDIO }, 1);
    } else {
      lovrPermissionEvent(0, true);
    }
  }
}
