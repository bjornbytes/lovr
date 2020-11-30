package org.lovr.app;
import android.app.NativeActivity;
import android.Manifest;
import android.app.NativeActivity;
import android.support.v4.app.ActivityCompat;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

public class Activity extends NativeActivity implements ActivityCompat.OnRequestPermissionsResultCallback {
  static {
    System.loadLibrary("lovr");
    System.loadLibrary("vrapi");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    Log.i("LOVR", "MainActivity.onCreate()");
    RequestPermissionsIfNeeded();
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
  {
    if(grantResults[0] == PackageManager.PERMISSION_GRANTED)
    {
      Log.i("LOVR", "RECORD_AUDIO permissions have now been granted.");
    }
    else
    {
      Log.i("LOVR", "RECORD_AUDIO permissions were denied.");
    }
  }

  private void RequestPermissionsIfNeeded()
  {
    int existingPermission = ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO);
    if(existingPermission != PackageManager.PERMISSION_GRANTED)
    {
      Log.i("LOVR", "Asking for RECORD_AUDIO permissions.");
      ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, 1);
    }
    else
    {
      Log.i("LOVR", "RECORD_AUDIO permissions already granted.");
    }
  }
}
