package org.lovr.app;
import android.Manifest;
import android.app.NativeActivity;
import android.content.pm.PackageManager;

public class Activity extends NativeActivity {
  static {
    System.loadLibrary("lovr");
    System.loadLibrary("vrapi");
  }

  protected native void lovrPermissionEvent(int permission, boolean granted);

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    if(grantResults[0] == PackageManager.PERMISSION_GRANTED)
    {
      lovrPermissionEvent(0, true);
    }
    else
    {
      lovrPermissionEvent(0, false);
    }
  }

  private void requestAudioCapturePermission()
  {
    int existingPermission = checkSelfPermission(Manifest.permission.RECORD_AUDIO);
    if(existingPermission != PackageManager.PERMISSION_GRANTED)
    {
      requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 1);
    }
    else
    {
      lovrPermissionEvent(0, true);
    }
  }
}
