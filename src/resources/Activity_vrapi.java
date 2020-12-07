package org.lovr.app;
import android.app.NativeActivity;
import android.Manifest;
import android.app.NativeActivity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

public class Activity extends NativeActivity {
  static {
    System.loadLibrary("lovr");
    System.loadLibrary("vrapi");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    Log.i("LOVR", "MainActivity.onCreate()");
  }

  protected native void lovrPermissionEvent(int permission, boolean granted);

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    if(grantResults[0] == PackageManager.PERMISSION_GRANTED)
    {
      Log.i("LOVR", "RECORD_AUDIO granted.");
      lovrPermissionEvent(0, true);
    }
    else
    {
      Log.i("LOVR", "RECORD_AUDIO rejected.");
      lovrPermissionEvent(0, false);
    }
  }

  private void requestAudioCapturePermission()
  {
    int existingPermission = checkSelfPermission(Manifest.permission.RECORD_AUDIO);
    if(existingPermission != PackageManager.PERMISSION_GRANTED)
    {
      Log.i("LOVR", "Asking for RECORD_AUDIO permissions.");
      requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 1);
    }
    else
    {
      Log.i("LOVR", "RECORD_AUDIO already permitted.");
      lovrPermissionEvent(0, true);
    }
  }
}
