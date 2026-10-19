package com.jenga.remote;

import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.BitmapFactory;
import android.os.*;
import android.view.*;
import android.widget.*;
import androidx.appcompat.app.AppCompatActivity;
import org.json.*;

public final class MainActivity extends AppCompatActivity implements Connection.Listener {
  private Connection connection;
  private Vault vault;
  private JSONArray computers;
  private JSONObject current;
  private FrameLayout content;
  private TextView status;
  private ImageView desktop;
  private String page = "Computers";
  private int monitor, monitorCount = 1, quality = 60;
  private final Handler timer = new Handler(Looper.getMainLooper());
  private boolean framePending;
  private boolean screenPaused;
  private long frameSent;
  private int reconnectAttempts;
  private boolean foreground;
  private final Runnable reconnect =
      () -> {
        if (foreground && current != null && current.has("credential") && !connection.authenticated)
          try {
            connection.connect(current, false);
          } catch (Exception e) {
            status("Reconnect failed; use Connect");
          }
      };
  private final Runnable pulse =
      new Runnable() {
        public void run() {
          if (connection.authenticated) {
            connection.send("PING");
            if (page.equals("Desktop")
                && !screenPaused
                && (!framePending || System.currentTimeMillis() - frameSent > 3000)) {
              framePending = true;
              frameSent = System.currentTimeMillis();
              connection.send("SCREEN_REQUEST", "monitor", monitor, "quality", quality);
            }
          }
          timer.postDelayed(this, page.equals("Desktop") ? 250 : 10_000);
        }
      };

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    setContentView(R.layout.activity_main);
    status = findViewById(R.id.status);
    content = findViewById(R.id.content);
    connection = new Connection(this);
    try {
      vault = new Vault(this);
      computers = vault.load();
    } catch (Exception e) {
      status.setText("Credential storage error. Clear app data to reset.");
      return;
    }
    LinearLayout tabs = findViewById(R.id.tabs);
    for (String tab : new String[] {"Computers", "Touchpad", "Keyboard", "Desktop", "Settings"})
      button(tabs, tab, () -> show(tab));
    show("Computers");
    timer.post(pulse);
    if (computers.length() > 0) connect(computers.optJSONObject(0), false);
  }

  private void button(LinearLayout parent, String name, Runnable action) {
    Button button = new Button(this);
    button.setText(name);
    button.setOnClickListener(v -> action.run());
    parent.addView(button);
  }

  private LinearLayout column() {
    content.removeAllViews();
    ScrollView scroll = new ScrollView(this);
    LinearLayout layout = new LinearLayout(this);
    layout.setOrientation(LinearLayout.VERTICAL);
    scroll.addView(layout);
    content.addView(scroll);
    return layout;
  }

  private void text(LinearLayout parent, String text) {
    TextView view = new TextView(this);
    view.setText(text);
    view.setPadding(8, 16, 8, 16);
    parent.addView(view);
  }

  private void show(String selected) {
    page = selected;
    desktop = null;
    framePending = false;
    screenPaused = false;
    LinearLayout layout = column();
    if (selected.equals("Computers")) {
      button(
          layout,
          "Scan QR Code to Connect",
          () -> startActivityForResult(new Intent(this, ScannerActivity.class), 20));
      button(layout, "Find saved computers on Wi-Fi", this::discover);
      text(
          layout,
          "Use the PC's LAN address on Wi-Fi, or its WireGuard / Tailscale address for remote"
              + " access.");
      button(
          layout,
          "Paste pairing JSON manually",
          () -> {
            EditText input = new EditText(this);
            new AlertDialog.Builder(this)
                .setTitle("Pairing JSON from your PC")
                .setView(input)
                .setPositiveButton("Connect", (d, w) -> pair(input.getText().toString()))
                .setNegativeButton("Cancel", null)
                .show();
          });
      for (int i = 0; i < computers.length(); i++) {
        final int index = i;
        JSONObject computer = computers.optJSONObject(i);
        text(
            layout,
            computer.optString("deviceName")
                + " • "
                + computer.optString("host")
                + "\nLast connected: "
                + computer.optString("lastConnected", "Never"));
        button(layout, "Connect", () -> connect(computer, false));
        button(
            layout,
            "Change LAN / VPN address",
            () -> {
              EditText input = new EditText(this);
              input.setText(computer.optString("host"));
              new AlertDialog.Builder(this)
                  .setTitle("Reachable address; certificate remains pinned")
                  .setView(input)
                  .setPositiveButton(
                      "Save",
                      (d, w) -> {
                        try {
                          computer.put("host", input.getText().toString().trim());
                          vault.save(computers);
                          show("Computers");
                        } catch (Exception e) {
                          status("Could not save address");
                        }
                      })
                  .setNegativeButton("Cancel", null)
                  .show();
            });
        button(
            layout,
            "Forget computer",
            () -> {
              disconnect();
              computers.remove(index);
              try {
                vault.save(computers);
              } catch (Exception e) {
                status("Save failed");
              }
              show("Computers");
            });
      }
      button(layout, "Disconnect", this::disconnect);
    } else if (selected.equals("Touchpad")) {
      content.removeAllViews();
      LinearLayout pad = new LinearLayout(this);
      pad.setOrientation(LinearLayout.VERTICAL);
      content.addView(pad);
      TouchpadView touch = new TouchpadView(this, connection);
      touch.sensitivity = getPreferences(0).getFloat("sensitivity", 1.5f);
      pad.addView(touch, new LinearLayout.LayoutParams(-1, 0, 1));
      LinearLayout buttons = new LinearLayout(this);
      button(buttons, "Left click", () -> click(1));
      button(buttons, "Right click", () -> click(3));
      pad.addView(buttons);
    } else if (selected.equals("Keyboard")) {
      text(layout, "Text uses clipboard paste on the PC and replaces its clipboard contents.");
      EditText input = new EditText(this);
      input.setHint("Type text to send");
      input.setMinLines(2);
      layout.addView(input);
      button(
          layout,
          "Send text",
          () -> {
            connection.send("TEXT_INPUT", "text", input.getText().toString());
            input.setText("");
          });
      String[] names = {
        "Enter",
        "Escape",
        "Tab",
        "Backspace",
        "Delete",
        "Home",
        "End",
        "←",
        "↑",
        "→",
        "↓",
        "Page Up",
        "Page Down"
      };
      int[] codes = {10, 27, 9, 8, 127, 36, 35, 37, 38, 39, 40, 33, 34};
      for (int i = 0; i < names.length; i++) {
        final int code = codes[i];
        button(layout, names[i], () -> key(code));
      }
      for (int i = 1; i <= 12; i++) {
        final int code = 111 + i;
        button(layout, "F" + i, () -> key(code));
      }
      button(layout, "Ctrl+C", () -> shortcut(17, 67));
      button(layout, "Ctrl+V", () -> shortcut(17, 86));
      button(layout, "Ctrl+Z", () -> shortcut(17, 90));
      button(layout, "Alt+Tab", () -> shortcut(18, 9));
      String[] mediaNames = {
        "Play / Pause", "Previous track", "Next track", "Volume +", "Volume −", "Mute"
      };
      String[] mediaActions = {
        "PLAY_PAUSE", "PREVIOUS", "NEXT", "VOLUME_UP", "VOLUME_DOWN", "MUTE"
      };
      for (int i = 0; i < mediaNames.length; i++) {
        final String action = mediaActions[i];
        button(layout, mediaNames[i], () -> connection.send("MEDIA_CONTROL", "action", action));
      }
      button(layout, "Presentation: Previous", () -> key(33));
      button(layout, "Presentation: Next", () -> key(34));
      button(layout, "Presentation: Start", () -> key(116));
      button(layout, "Fullscreen F11", () -> key(122));
    } else if (selected.equals("Desktop")) {
      text(
          layout,
          "Enable screen viewing on the PC first. Tap the image to click. Pinch to zoom, two"
              + " fingers to pan, long press to drag. Up to 4 JPEG requests/sec.");
      button(
          layout,
          "Next monitor (" + (monitor + 1) + " / " + monitorCount + ")",
          () -> {
            monitor = (monitor + 1) % monitorCount;
            show("Desktop");
          });
      desktop = new DesktopImageView(this, connection, monitor);
      layout.addView(desktop, new LinearLayout.LayoutParams(-1, 800));
      timer.removeCallbacks(pulse);
      timer.post(pulse);
    } else {
      text(layout, "Pointer sensitivity");
      SeekBar sensitivity = new SeekBar(this);
      sensitivity.setMax(35);
      sensitivity.setProgress((int) (getPreferences(0).getFloat("sensitivity", 1.5f) * 10) - 5);
      layout.addView(sensitivity);
      sensitivity.setOnSeekBarChangeListener(
          new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar b, int value, boolean user) {
              getPreferences(0).edit().putFloat("sensitivity", (value + 5) / 10f).apply();
            }

            public void onStartTrackingTouch(SeekBar b) {}

            public void onStopTrackingTouch(SeekBar b) {}
          });
      text(layout, "JPEG quality");
      SeekBar compression = new SeekBar(this);
      compression.setMax(65);
      compression.setProgress(quality - 20);
      layout.addView(compression);
      compression.setOnSeekBarChangeListener(
          new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar b, int v, boolean u) {
              quality = v + 20;
            }

            public void onStartTrackingTouch(SeekBar b) {}

            public void onStopTrackingTouch(SeekBar b) {}
          });
      text(
          layout,
          "Windows and Linux X11. Wayland, lock-screen control, Ctrl+Alt+Delete are not supported"
              + " in this release. Linux media requires playerctl / pactl. Internet access requires"
              + " an existing private VPN. Keep the app visible while controlling the PC.");
    }
  }

  private void discover() {
    status("Searching local Wi-Fi…");
    new Thread(
            () -> {
              int found = 0;
              try (java.net.DatagramSocket socket = new java.net.DatagramSocket()) {
                socket.setBroadcast(true);
                socket.setSoTimeout(3000);
                byte[] query = "JENGA_DISCOVER_1".getBytes(java.nio.charset.StandardCharsets.UTF_8);
                socket.send(
                    new java.net.DatagramPacket(
                        query,
                        query.length,
                        java.net.InetAddress.getByName("255.255.255.255"),
                        8765));
                long end = System.currentTimeMillis() + 3000;
                while (System.currentTimeMillis() < end) {
                  byte[] buffer = new byte[2048];
                  java.net.DatagramPacket packet =
                      new java.net.DatagramPacket(buffer, buffer.length);
                  try {
                    socket.receive(packet);
                  } catch (java.net.SocketTimeoutException timeout) {
                    break;
                  }
                  JSONObject reply =
                      new JSONObject(
                          new String(
                              buffer,
                              0,
                              packet.getLength(),
                              java.nio.charset.StandardCharsets.UTF_8));
                  if (!reply.optString("protocol").equals("JengaRemote")) continue;
                  String pin = reply.optString("serverPublicKeyFingerprint"),
                      host = packet.getAddress().getHostAddress();
                  // Do not trust discovery to change identity or credentials. TLS re-verifies this
                  // pin.
                  for (int i = 0; i < computers.length(); i++)
                    if (computers
                        .getJSONObject(i)
                        .optString("serverPublicKeyFingerprint")
                        .equals(pin)) {
                      JSONObject saved = computers.getJSONObject(i);
                      runOnUiThread(
                          () -> {
                            try {
                              saved.put("host", host);
                              vault.save(computers);
                              show("Computers");
                            } catch (Exception e) {
                              status("Could not save discovered address");
                            }
                          });
                      found++;
                    }
                }
              } catch (Exception e) {
                status("Discovery unavailable. Use QR or edit the saved address.");
                return;
              }
              status(
                  "Discovery finished: "
                      + found
                      + " saved computers found. New PCs require QR pairing.");
            },
            "lan-discovery")
        .start();
  }

  private void key(int code) {
    connection.send("KEY_DOWN", "key", code);
    connection.send("KEY_UP", "key", code);
  }

  private void shortcut(int modifier, int code) {
    connection.send("KEY_DOWN", "key", modifier);
    key(code);
    connection.send("KEY_UP", "key", modifier);
  }

  private void click(int button) {
    connection.send("MOUSE_BUTTON", "button", button, "down", true);
    connection.send("MOUSE_BUTTON", "button", button, "down", false);
  }

  private void pair(String raw) {
    try {
      JSONObject value = new JSONObject(raw);
      if (!value.getString("protocol").equals("JengaRemote")
          || value.getInt("version") != 1
          || value.getLong("expiresAt") <= System.currentTimeMillis())
        throw new IllegalArgumentException();
      if (raw.length() > 8192) throw new IllegalArgumentException();
      connect(value, true);
    } catch (Exception e) {
      status("Invalid, expired or unsupported pairing QR code");
    }
  }

  private void connect(JSONObject computer, boolean pairing) {
    timer.removeCallbacks(reconnect);
    try {
      current = computer;
      reconnectAttempts = 0;
      connection.connect(computer, pairing);
    } catch (Exception e) {
      status("Could not connect: " + e.getMessage());
    }
  }

  private void disconnect() {
    timer.removeCallbacks(reconnect);
    current = null;
    connection.disconnect();
    status("Disconnected");
  }

  @Override
  public void message(JSONObject value) {
    runOnUiThread(
        () -> {
          try {
            String type = value.getString("type");
            if (type.equals("PAIR_APPROVAL")) {
              current
                  .put("deviceId", value.getString("deviceId"))
                  .put("credential", value.getString("credential"));
              current.remove("pairingId");
              current.remove("pairingSecret");
              current.remove("expiresAt");
              current.put("lastConnected", new java.util.Date().toString());
              for (int i = computers.length() - 1; i >= 0; i--)
                if (computers
                    .getJSONObject(i)
                    .optString("serverPublicKeyFingerprint")
                    .equals(current.optString("serverPublicKeyFingerprint"))) computers.remove(i);
              computers.put(current);
              vault.save(computers);
              status("Paired and saved securely");
              show("Touchpad");
            } else if (type.equals("AUTH_SUCCESS")) {
              reconnectAttempts = 0;
              monitorCount = value.optInt("monitors", 1);
              status("Connected • authenticated");
              if (current != null && current.has("credential")) {
                current.put("lastConnected", new java.util.Date().toString());
                vault.save(computers);
                show("Touchpad");
              }
            } else if (type.equals("ERROR") || type.equals("PAIR_REJECTED")) {
              framePending = false;
              if (value.optString("message").contains("Permission disabled")) screenPaused = true;
              status(value.optString("message"));
            }
          } catch (Exception e) {
            connection.disconnect();
            status("Could not save pairing. Pair again.");
          }
        });
  }

  @Override
  public void frame(byte[] bytes) {
    var bitmap = BitmapFactory.decodeByteArray(bytes, 0, bytes.length);
    runOnUiThread(
        () -> {
          framePending = false;
          if (desktop != null && bitmap != null) desktop.setImageBitmap(bitmap);
        });
  }

  @Override
  public void status(String message) {
    runOnUiThread(
        () -> {
          status.setText(message);
          if (message.startsWith("Connection failed")
              && foreground
              && current != null
              && current.has("credential")
              && reconnectAttempts < 5) {
            timer.removeCallbacks(reconnect);
            timer.postDelayed(reconnect, Math.min(30000, 1000L << reconnectAttempts++));
          }
        });
  }

  @Override
  protected void onActivityResult(int request, int result, Intent data) {
    super.onActivityResult(request, result, data);
    if (request == 20 && result == RESULT_OK && data != null) pair(data.getStringExtra("qr"));
  }

  @Override
  protected void onStart() {
    super.onStart();
    foreground = true;
    timer.removeCallbacks(pulse);
    timer.post(pulse);
    if (current != null && current.has("credential") && !connection.authenticated)
      timer.postDelayed(reconnect, 500);
  }

  @Override
  protected void onStop() {
    super.onStop();
    foreground = false;
    timer.removeCallbacks(pulse);
    timer.removeCallbacks(reconnect);
    connection.disconnect();
  }

  @Override
  protected void onDestroy() {
    super.onDestroy();
    timer.removeCallbacks(pulse);
    connection.disconnect();
  }
}
