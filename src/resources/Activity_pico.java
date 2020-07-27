package org.lovr.app;

import android.os.Bundle;

import com.picovr.vractivity.Eye;
import com.picovr.vractivity.HmdState;
import com.picovr.vractivity.RenderInterface;
import com.picovr.vractivity.VRActivity;
import com.picovr.cvclient.ButtonNum;
import com.picovr.cvclient.CVController;
import com.picovr.cvclient.CVControllerListener;
import com.picovr.cvclient.CVControllerManager;
import com.picovr.picovrlib.cvcontrollerclient.ControllerClient;
import com.psmart.vrlib.PicovrSDK;

public class Activity extends VRActivity implements RenderInterface, CVControllerListener {
  CVControllerManager controllerManager;
  boolean controllersActive;

  // Activity

  public void onCreate(Bundle bundle) {
    super.onCreate(bundle);

    if (ControllerClient.isControllerServiceExisted(this)) {
      controllerManager = new CVControllerManager(this);
      controllerManager.setListener(this);
    }

    lovrPicoOnCreate(getPackageCodePath());
  }

  public void onPause() {
    super.onPause();
    if (controllerManager != null) {
      controllerManager.unbindService();
    }
  }

  public void onResume() {
    super.onResume();
    PicovrSDK.SetEyeBufferSize(1920, 1920);
    if (controllerManager != null) {
      controllerManager.bindService();
    }
  }

  // RenderInterface

  public void initGL(int width, int height) {
    lovrPicoSetDisplayDimensions(width, height);
  }

  public void onFrameBegin(HmdState state) {
    if (controllersActive) {
      for (int i = 0; i < 2; i++) {
        CVController controller = (i == 0) ?
          controllerManager.getMainController() :
          controllerManager.getSubController();

        if (controller == null || controller.getConnectState() == 0) {
          lovrPicoUpdateControllerPose(i, false, 0, 0, 0, 0, 0, 0, 0);
          continue;
        }

        float p[] = controller.getPosition();
        float q[] = controller.getOrientation();
        lovrPicoUpdateControllerPose(i, true, p[0], p[1], p[2], q[0], q[1], q[2], q[3]);

        int thumbstick[] = controller.getTouchPad();
        float trigger = (float) controller.getTriggerNum() / 255.f;
        float thumbstickX = ((float) thumbstick[1] - 128.f) / (thumbstick[1] > 128 ? 127.f : 128.f);
        float thumbstickY = ((float) thumbstick[0] - 128.f) / (thumbstick[0] > 128 ? 127.f : 128.f);

        int buttons = 0;
        ButtonNum gripButton = (i == 0) ? ButtonNum.buttonRG : ButtonNum.buttonLG; // Yes I know
        buttons |= trigger >= .9f ? (1 << 0) : 0;
        buttons |= controller.getButtonState(ButtonNum.click) ? (1 << 1) : 0;
        buttons |= controller.getButtonState(gripButton) ? (1 << 2) : 0;
        buttons |= controller.getButtonState(ButtonNum.app) ? (1 << 3) : 0;
        buttons |= controller.getButtonState(ButtonNum.buttonAX) ? (1 << 4) : 0;
        buttons |= controller.getButtonState(ButtonNum.buttonBY) ? (1 << 5) : 0;
        lovrPicoUpdateControllerInput(i, buttons, trigger, thumbstickX, thumbstickY);
      }
    }

    float p[] = state.getPos();
    float q[] = state.getOrientation();
    float fov = state.getFov();
    float ipd = state.getIpd();
    lovrPicoOnFrame(p[0], p[1], p[2], q[0], q[1], q[2], q[3], fov, ipd);
  }

  public void onDrawEye(Eye eye) {
    lovrPicoDrawEye(eye.getType());
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

  // CVControllerListener

  public void onBindSuccess() {
    //
  }

  public void onBindFail() {
    controllersActive = false;
  }

  public void onThreadStart() {
    controllersActive = true;
  }

  public void onConnectStateChanged(int serial, int state) {
    //
  }

  public void onMainControllerChanged(int serial) {
    //
  }

  public void onChannelChanged(int device, int channel) {
    //
  }

  // Native
  protected native void lovrPicoOnCreate(String apkPath);
  protected native void lovrPicoSetDisplayDimensions(int width, int height);
  protected native void lovrPicoUpdateControllerPose(int hand, boolean active, float x, float y, float z, float qx, float qy, float qz, float qw);
  protected native void lovrPicoUpdateControllerInput(int hand, int buttons, float trigger, float thumbstickX, float thumbstickY);
  protected native void lovrPicoOnFrame(float x, float y, float z, float qx, float qy, float qz, float qw, float fov, float ipd);
  protected native void lovrPicoDrawEye(int eye);

  public void vibrate(int hand, float strength, float duration) {
    if (controllerManager != null) {
      int ms = (int) (duration * 1000.f);
      ControllerClient.vibrateCV2ControllerStrength(strength, ms, hand);
    }
  }

  static {
    System.loadLibrary("lovr");
  }
}
