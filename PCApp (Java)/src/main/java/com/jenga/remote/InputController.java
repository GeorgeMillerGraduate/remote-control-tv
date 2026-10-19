package com.jenga.remote;

import java.awt.*;
import java.awt.datatransfer.StringSelection;
import java.awt.event.*;
import java.util.HashSet;
import java.util.Set;

/** All calls are serialized by RemoteServer's command executor. */
public final class InputController {
  private final Robot robot = new Robot();
  private final Set<Integer> keys = new HashSet<>();
  private final Set<Integer> buttons = new HashSet<>();

  public InputController() throws AWTException {
    if ("wayland".equalsIgnoreCase(System.getenv("XDG_SESSION_TYPE")))
      throw new AWTException("Use an X11 session: Wayland control is not implemented");
  }

  public void move(int x, int y) {
    Point p = MouseInfo.getPointerInfo().getLocation();
    robot.mouseMove(p.x + x, p.y + y);
  }

  public void absolute(int monitor, double x, double y) {
    Rectangle bounds =
        GraphicsEnvironment.getLocalGraphicsEnvironment()
            .getScreenDevices()[monitor]
            .getDefaultConfiguration()
            .getBounds();
    robot.mouseMove(
        bounds.x + (int) (x * (bounds.width - 1)), bounds.y + (int) (y * (bounds.height - 1)));
  }

  public void button(int button, boolean down) {
    int mask = InputEvent.getMaskForButton(button);
    if (down) {
      robot.mousePress(mask);
      buttons.add(mask);
    } else {
      robot.mouseRelease(mask);
      buttons.remove(mask);
    }
  }

  public void key(int key, boolean down) {
    if (down) {
      robot.keyPress(key);
      keys.add(key);
    } else {
      robot.keyRelease(key);
      keys.remove(key);
    }
  }

  public void scroll(int amount) {
    robot.mouseWheel(amount);
  }

  public void text(String text) {
    // Explicit paste: arbitrary Unicode works, but this replaces the host clipboard.
    Toolkit.getDefaultToolkit().getSystemClipboard().setContents(new StringSelection(text), null);
    key(KeyEvent.VK_CONTROL, true);
    key(KeyEvent.VK_V, true);
    key(KeyEvent.VK_V, false);
    key(KeyEvent.VK_CONTROL, false);
  }

  public void releaseAll() {
    for (int key : keys) robot.keyRelease(key);
    for (int button : buttons) robot.mouseRelease(button);
    keys.clear();
    buttons.clear();
  }

  public byte[] capture(int monitor, int quality) throws Exception {
    var device = GraphicsEnvironment.getLocalGraphicsEnvironment().getScreenDevices()[monitor];
    var image = robot.createScreenCapture(device.getDefaultConfiguration().getBounds());
    int width = Math.min(1280, image.getWidth());
    int height = (int) ((double) image.getHeight() * width / image.getWidth());
    var scaled =
        new java.awt.image.BufferedImage(width, height, java.awt.image.BufferedImage.TYPE_INT_RGB);
    var graphics = scaled.createGraphics();
    graphics.drawImage(image, 0, 0, width, height, null);
    Point pointer = MouseInfo.getPointerInfo().getLocation();
    Rectangle bounds = device.getDefaultConfiguration().getBounds();
    if (bounds.contains(pointer)) {
      int cx = (int) ((pointer.x - bounds.x) * (double) width / bounds.width);
      int cy = (int) ((pointer.y - bounds.y) * (double) height / bounds.height);
      graphics.setColor(Color.BLACK);
      graphics.drawOval(cx - 5, cy - 5, 10, 10);
      graphics.setColor(Color.WHITE);
      graphics.drawOval(cx - 4, cy - 4, 8, 8);
    }
    graphics.dispose();
    var writer = javax.imageio.ImageIO.getImageWritersByFormatName("jpeg").next();
    var bytes = new java.io.ByteArrayOutputStream();
    try (var output = javax.imageio.ImageIO.createImageOutputStream(bytes)) {
      writer.setOutput(output);
      var parameters = writer.getDefaultWriteParam();
      parameters.setCompressionMode(javax.imageio.ImageWriteParam.MODE_EXPLICIT);
      parameters.setCompressionQuality(quality / 100f);
      writer.write(null, new javax.imageio.IIOImage(scaled, null, null), parameters);
    } finally {
      writer.dispose();
    }
    return bytes.toByteArray();
  }
}
