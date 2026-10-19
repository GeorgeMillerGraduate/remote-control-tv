package com.jenga.remote;

import android.content.Context;
import android.graphics.*;
import android.view.*;

public final class TouchpadView extends View {
  private final Connection connection;
  private final GestureDetector gestures;
  private float lastX, lastY, startX, startY;
  private boolean dragging, multiple, moved;
  private long started;
  private float scroll;
  public float sensitivity = 1.5f;

  public TouchpadView(Context context, Connection connection) {
    super(context);
    this.connection = connection;
    setContentDescription(
        "Remote touchpad. Tap to click; use two fingers to scroll or right-click; long press to"
            + " drag.");
    setBackgroundColor(Color.rgb(30, 48, 58));
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
                if (!multiple) click(1);
                return true;
              }

              @Override
              public boolean onDoubleTap(MotionEvent e) {
                click(1);
                return true;
              }

              @Override
              public void onLongPress(MotionEvent e) {
                if (!multiple) {
                  dragging = true;
                  connection.send("MOUSE_BUTTON", "button", 1, "down", true);
                  performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                }
              }
            });
  }

  private void click(int button) {
    connection.send("MOUSE_BUTTON", "button", button, "down", true);
    connection.send("MOUSE_BUTTON", "button", button, "down", false);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
      multiple = false;
      moved = false;
      started = System.currentTimeMillis();
      startX = lastX = event.getX();
      startY = lastY = event.getY();
      scroll = 0;
    }
    if (event.getPointerCount() > 1) multiple = true;
    // Feed all events so GestureDetector sees pointer-up and cancellation.
    gestures.onTouchEvent(event);
    if (event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN
        || event.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
      lastX = event.getX();
      lastY = event.getY();
      return true;
    }
    if (event.getActionMasked() == MotionEvent.ACTION_MOVE) {
      float x = event.getX(), y = event.getY();
      moved |= Math.abs(x - startX) + Math.abs(y - startY) > 16;
      if (event.getPointerCount() > 1) {
        scroll += (y - lastY) / 24f;
        int amount = (int) scroll;
        if (amount != 0) {
          connection.send("MOUSE_SCROLL", "amount", Math.max(-30, Math.min(30, amount)));
          scroll -= amount;
        }
      } else if (!multiple)
        connection.send(
            "MOUSE_MOVE",
            "dx",
            Math.max(-500, Math.min(500, (int) ((x - lastX) * sensitivity))),
            "dy",
            Math.max(-500, Math.min(500, (int) ((y - lastY) * sensitivity))));
      lastX = x;
      lastY = y;
    }
    if (event.getActionMasked() == MotionEvent.ACTION_UP
        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
      if (dragging) {
        connection.send("MOUSE_BUTTON", "button", 1, "down", false);
        dragging = false;
      } else if (event.getActionMasked() == MotionEvent.ACTION_UP
          && multiple
          && !moved
          && System.currentTimeMillis() - started < 350) click(3);
    }
    return true;
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    paint.setColor(Color.LTGRAY);
    paint.setTextSize(36);
    canvas.drawText("Touchpad", 32, 60, paint);
  }
}
