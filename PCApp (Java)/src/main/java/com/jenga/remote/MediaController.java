package com.jenga.remote;

import com.sun.jna.*;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Explicit allowlist: no client-supplied shell commands or executable names. */
public final class MediaController {
  public interface User32 extends Library {
    void keybd_event(byte key, byte scan, int flags, Pointer extra);
  }

  private static final Map<String, Integer> WINDOWS =
      Map.of(
          "PLAY_PAUSE",
          0xB3,
          "PREVIOUS",
          0xB1,
          "NEXT",
          0xB0,
          "VOLUME_UP",
          0xAF,
          "VOLUME_DOWN",
          0xAE,
          "MUTE",
          0xAD);

  public void execute(String action) throws Exception {
    if (!WINDOWS.containsKey(action))
      throw new IllegalArgumentException("Unsupported media action");
    if (System.getProperty("os.name").startsWith("Windows")) {
      User32 user = Native.load("user32", User32.class);
      byte key = WINDOWS.get(action).byteValue();
      user.keybd_event(key, (byte) 0, 0, null);
      user.keybd_event(key, (byte) 0, 2, null);
    } else {
      String[] command =
          switch (action) {
            case "PLAY_PAUSE" -> new String[] {"playerctl", "play-pause"};
            case "PREVIOUS" -> new String[] {"playerctl", "previous"};
            case "NEXT" -> new String[] {"playerctl", "next"};
            case "VOLUME_UP" -> new String[] {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%"};
            case "VOLUME_DOWN" ->
                new String[] {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%"};
            default -> new String[] {"pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle"};
          };
      Process process =
          new ProcessBuilder(command)
              .redirectOutput(ProcessBuilder.Redirect.DISCARD)
              .redirectError(ProcessBuilder.Redirect.DISCARD)
              .start();
      if (!process.waitFor(2, TimeUnit.SECONDS)) {
        process.destroyForcibly();
        throw new IllegalArgumentException("Media helper timed out");
      }
      if (process.exitValue() != 0)
        throw new IllegalArgumentException(
            "Media action unavailable; install playerctl / pactl and start a player");
    }
  }
}
