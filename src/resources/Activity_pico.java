package org.lovr.app;

import android.os.Bundle;

import com.picovr.vractivity.Eye;
import com.picovr.vractivity.HmdState;
import com.picovr.vractivity.RenderInterface;
import com.picovr.vractivity.VRActivity;

public class Activity extends VRActivity implements RenderInterface {

  // Activity

  public void onCreate(Bundle bundle) {
    super.onCreate(bundle);
  }

  public void onPause() {
    super.onPause();
  }

  public void onResume() {
    super.onResume();
  }

  // RenderInterface

  public void initGL(int width, int height) {
    //
  }

  public void onFrameBegin(HmdState state) {
    //
  }

  public void onDrawEye(Eye eye) {
    //
  }

  public void onFrameEnd() {
    //
  }

  public void onRenderPause() {
    //
  }

  public void onRenderResume() {
    //
  }

  public void onRendererShutdown() {
    //
  }

  public void surfaceChangedCallBack(int width, int height) {
    //
  }

  public void renderEventCallBack(int i) {
    //
  }

  public void onTouchEvent() {
    //
  }

  static {
    System.loadLibrary("lovr");
  }
}
