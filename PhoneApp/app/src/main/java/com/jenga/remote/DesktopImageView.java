package com.jenga.remote;

import android.content.Context;
import android.graphics.Matrix;
import android.view.*;
import androidx.appcompat.widget.AppCompatImageView;

/** Fit, pinch zoom, two-finger pan, tap-to-click and long-press drag. */
public final class DesktopImageView extends AppCompatImageView {
  private final Connection connection;
  private final int monitor;
  private final Matrix transform = new Matrix();
  private final ScaleGestureDetector scaling;
  private final GestureDetector gestures;
  private boolean fitted, dragging;
  private float lastX, lastY;

  public DesktopImageView(Context context, Connection connection, int monitor) {
    super(context);
    this.connection = connection;
    this.monitor = monitor;
    setScaleType(ScaleType.MATRIX);
    setContentDescription(
        "Remote desktop: pinch to zoom; two fingers to pan; tap to click; long press to drag");
    scaling =
        new ScaleGestureDetector(
            context,
            new ScaleGestureDetector.SimpleOnScaleGestureListener() {
              @Override
              public boolean onScale(ScaleGestureDetector detector) {
                float[] values = new float[9];
                transform.getValues(values);
                float factor = detector.getScaleFactor();
                if (values[0] * factor < 0.1f || values[0] * factor > 8f) return false;
                transform.postScale(factor, factor, detector.getFocusX(), detector.getFocusY());
                setImageMatrix(transform);
                return true;
              }
            });
    gestures =
        new GestureDetector(
            context,
            new GestureDetector.SimpleOnGestureListener() {
              @Override
              public boolean onDown(MotionEvent e) {
                return true;
              }

              @Override
              public boolean onSingleTapUp(MotionEvent e) {
                if (point(e.getX(), e.getY())) {
                  button(true);
                  button(false);
                }
                return true;
              }

              @Override
              public void onLongPress(MotionEvent e) {
                if (point(e.getX(), e.getY())) {
                  dragging = true;
                  button(true);
                }
              }
            });
  }

  @Override
  public void setImageBitmap(android.graphics.Bitmap bitmap) {
    super.setImageBitmap(bitmap);
    fit();
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh) {
    super.onSizeChanged(w, h, oldw, oldh);
    fitted = false;
    fit();
  }

  private void fit() {
    if (fitted || getDrawable() == null || getWidth() == 0 || getHeight() == 0) return;
    float width = getDrawable().getIntrinsicWidth(), height = getDrawable().getIntrinsicHeight();
    float scale = Math.min(getWidth() / width, getHeight() / height);
    transform.reset();
    transform.postScale(scale, scale);
    transform.postTranslate((getWidth() - width * scale) / 2, (getHeight() - height * scale) / 2);
    setImageMatrix(transform);
    fitted = true;
  }

  private boolean point(float x, float y) {
    if (getDrawable() == null) return false;
    Matrix inverse = new Matrix();
    if (!transform.invert(inverse)) return false;
    float[] values = {x, y};
    inverse.mapPoints(values);
    double nx = values[0] / getDrawable().getIntrinsicWidth(),
        ny = values[1] / getDrawable().getIntrinsicHeight();
    if (nx < 0 || nx > 1 || ny < 0 || ny > 1) return false;
    connection.send("DESKTOP_POINT", "monitor", monitor, "x", nx, "y", ny);
    return true;
  }

  private void button(boolean down) {
    connection.send("MOUSE_BUTTON", "button", 1, "down", down);
  }

  @Override
  public boolean onTouchEvent(MotionEvent e) {
    scaling.onTouchEvent(e);
    gestures.onTouchEvent(e);
    if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
      lastX = e.getX();
      lastY = e.getY();
    }
    if (e.getActionMasked() == MotionEvent.ACTION_MOVE) {
      if (e.getPointerCount() == 2 && !scaling.isInProgress()) {
        transform.postTranslate(e.getX() - lastX, e.getY() - lastY);
        setImageMatrix(transform);
      } else if (dragging && e.getPointerCount() == 1) point(e.getX(), e.getY());
      lastX = e.getX();
      lastY = e.getY();
    }
    if (e.getActionMasked() == MotionEvent.ACTION_UP
        || e.getActionMasked() == MotionEvent.ACTION_CANCEL) {
      if (dragging) {
        button(false);
        dragging = false;
      }
    }
    return true;
  }
}
