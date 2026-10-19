package com.jenga.remote;

import com.formdev.flatlaf.FlatDarkLaf;
import com.google.gson.JsonObject;
import com.google.zxing.*;
import com.google.zxing.common.BitMatrix;
import java.awt.*;
import java.awt.image.BufferedImage;
import java.net.*;
import java.nio.file.Path;
import javax.swing.*;

public final class DesktopApp {
  private final JFrame frame = new JFrame("Jenga Remote • PC controller");
  private final JTextArea logs = new JTextArea(12, 60);
  private final JTextField host = new JTextField(22);
  private final JSpinner port = new JSpinner(new SpinnerNumberModel(8765, 1024, 65535, 1));
  private RemoteServer server;
  private SecurityStore identity;
  private final JCheckBox mouse = new JCheckBox("Mouse", true),
      keyboard = new JCheckBox("Keyboard", true),
      screen = new JCheckBox("Allow screen viewing", false),
      media = new JCheckBox("Media", true);

  private void log(String text) {
    SwingUtilities.invokeLater(
        () -> logs.append(java.time.LocalTime.now().withNano(0) + " " + text + "\n"));
  }

  private void build() throws Exception {
    if (identity == null) throw new IllegalStateException("Identity not initialised");
    host.setText(defaultAddress());
    frame.setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
    frame.addWindowListener(
        new java.awt.event.WindowAdapter() {
          @Override
          public void windowClosing(java.awt.event.WindowEvent e) {
            new Thread(
                    () -> {
                      try {
                        if (server != null) server.closeServer();
                      } catch (Exception ignored) {
                      }
                      System.exit(0);
                    })
                .start();
          }
        });
    var panel = new JPanel(new BorderLayout(12, 12));
    panel.setBorder(BorderFactory.createEmptyBorder(20, 20, 20, 20));
    var controls = new JPanel(new GridLayout(0, 1, 6, 6));
    controls.add(new JLabel("JENGA REMOTE — visible, authorised PC control"));
    var network = new JPanel();
    network.add(new JLabel("Advertised LAN / VPN IP"));
    network.add(host);
    network.add(port);
    controls.add(network);
    JButton start = new JButton("Start server"),
        stop = new JButton("EMERGENCY STOP"),
        qr = new JButton("Connect Phone — Show QR Code"),
        revoke = new JButton("Revoke all trusted phones");
    controls.add(start);
    controls.add(qr);
    var permissions = new JPanel();
    permissions.add(mouse);
    permissions.add(keyboard);
    permissions.add(screen);
    permissions.add(media);
    controls.add(permissions);
    controls.add(stop);
    controls.add(revoke);
    JButton manage = new JButton("Manage trusted phones");
    controls.add(manage);
    manage.addActionListener(
        e -> {
          if (server == null) return;
          server.devices(
              devices -> {
                if (devices.isEmpty()) {
                  JOptionPane.showMessageDialog(frame, "No trusted phones");
                  return;
                }
                String[] ids = devices.keySet().toArray(String[]::new);
                String[] labels =
                    devices.entrySet().stream()
                        .map(entry -> entry.getValue() + " • " + entry.getKey())
                        .toArray(String[]::new);
                String selected =
                    (String)
                        JOptionPane.showInputDialog(
                            frame,
                            "Choose a phone",
                            "Trusted phones",
                            JOptionPane.PLAIN_MESSAGE,
                            null,
                            labels,
                            labels[0]);
                if (selected == null) return;
                int index = java.util.Arrays.asList(labels).indexOf(selected);
                String id = ids[index];
                int action =
                    JOptionPane.showOptionDialog(
                        frame,
                        selected,
                        "Device actions",
                        JOptionPane.DEFAULT_OPTION,
                        JOptionPane.PLAIN_MESSAGE,
                        null,
                        new String[] {"Disconnect", "Revoke trust", "Rename", "Cancel"},
                        "Cancel");
                if (action == 0) server.disconnectDevice(id);
                if (action == 1) server.revoke(id);
                if (action == 2) {
                  String name = JOptionPane.showInputDialog(frame, "Phone name", devices.get(id));
                  if (name != null) server.rename(id, name);
                }
              });
        });
    JButton shutdown = new JButton("Stop server");
    controls.add(shutdown);
    shutdown.addActionListener(
        e -> {
          if (server != null)
            new Thread(
                    () -> {
                      try {
                        server.closeServer();
                        server = null;
                        SwingUtilities.invokeLater(() -> start.setEnabled(true));
                        log("Server stopped");
                      } catch (Exception ex) {
                        log("Stop failed");
                      }
                    },
                    "server-stop")
                .start();
        });
    panel.add(controls, BorderLayout.NORTH);
    logs.setEditable(false);
    panel.add(new JScrollPane(logs), BorderLayout.CENTER);
    for (var box : new JCheckBox[] {mouse, keyboard, screen, media})
      box.addActionListener(
          e -> {
            if (server != null) {
              server.mouse = mouse.isSelected();
              server.keyboard = keyboard.isSelected();
              server.screen = screen.isSelected();
              server.media = media.isSelected();
              server.emergency();
            }
          });
    start.addActionListener(
        e -> {
          start.setEnabled(false);
          new Thread(
                  () -> {
                    try {
                      server = new RemoteServer((int) port.getValue(), identity, this::log);
                      server.mouse = mouse.isSelected();
                      server.keyboard = keyboard.isSelected();
                      server.screen = screen.isSelected();
                      server.media = media.isSelected();
                      server.start();
                    } catch (Exception ex) {
                      log("Start failed: " + ex.getMessage());
                      SwingUtilities.invokeLater(() -> start.setEnabled(true));
                    }
                  },
                  "server-start")
              .start();
        });
    stop.addActionListener(
        e -> {
          if (server != null) server.emergency();
        });
    revoke.addActionListener(
        e -> {
          if (server != null
              && JOptionPane.showConfirmDialog(
                      frame,
                      "Forget every trusted phone?",
                      "Revoke trust",
                      JOptionPane.YES_NO_OPTION)
                  == 0) server.revokeAll();
        });
    qr.addActionListener(e -> showQr());
    frame.setContentPane(panel);
    frame.pack();
    frame.setLocationRelativeTo(null);
    frame.setVisible(true);
    log("Screen viewing is disabled until you enable it. One active controller is allowed.");
  }

  private void showQr() {
    if (server == null) {
      log("Start the server first");
      return;
    }
    PairingTicket ticket = new PairingTicket();
    if (server.ticket != null) server.ticket.invalidate();
    server.ticket = ticket;
    try {
      JsonObject payload = new JsonObject();
      payload.addProperty("protocol", "JengaRemote");
      payload.addProperty("version", 1);
      payload.addProperty("deviceName", InetAddress.getLocalHost().getHostName());
      payload.addProperty("host", host.getText().trim());
      payload.addProperty("port", server.getPort());
      payload.addProperty("pairingId", ticket.id);
      payload.addProperty("pairingSecret", ticket.secret);
      payload.addProperty("expiresAt", ticket.expiresAt);
      payload.addProperty("serverPublicKeyFingerprint", identity.fingerprint);
      BitMatrix bits =
          new MultiFormatWriter().encode(payload.toString(), BarcodeFormat.QR_CODE, 480, 480);
      BufferedImage image = new BufferedImage(480, 480, BufferedImage.TYPE_INT_RGB);
      for (int y = 0; y < 480; y++)
        for (int x = 0; x < 480; x++) image.setRGB(x, y, bits.get(x, y) ? 0xff000000 : 0xffffffff);
      JDialog dialog = new JDialog(frame, "Scan with Jenga Remote", false);
      JLabel countdown =
          new JLabel("Scan this QR, then approve the phone on this PC", SwingConstants.CENTER);
      dialog.add(new JLabel(new ImageIcon(image)), BorderLayout.CENTER);
      dialog.add(countdown, BorderLayout.SOUTH);
      Timer timer =
          new Timer(
              1000,
              e -> {
                long remaining =
                    Math.max(0, (ticket.expiresAt - System.currentTimeMillis()) / 1000);
                countdown.setText("Pairing expires in " + remaining + " seconds • Close to cancel");
                if (remaining == 0) dialog.dispose();
              });
      dialog.addWindowListener(
          new java.awt.event.WindowAdapter() {
            @Override
            public void windowClosed(java.awt.event.WindowEvent e) {
              ticket.invalidate();
              timer.stop();
            }
          });
      dialog.setDefaultCloseOperation(WindowConstants.DISPOSE_ON_CLOSE);
      dialog.pack();
      dialog.setLocationRelativeTo(frame);
      timer.start();
      dialog.setVisible(true);
    } catch (Exception e) {
      ticket.invalidate();
      log("QR generation failed");
    }
  }

  private static String defaultAddress() throws Exception {
    var interfaces = NetworkInterface.getNetworkInterfaces();
    while (interfaces.hasMoreElements()) {
      var network = interfaces.nextElement();
      if (!network.isUp() || network.isLoopback()) continue;
      var addresses = network.getInetAddresses();
      while (addresses.hasMoreElements()) {
        var address = addresses.nextElement();
        if (address instanceof Inet4Address) return address.getHostAddress();
      }
    }
    return "127.0.0.1";
  }

  public static void main(String[] args) {
    FlatDarkLaf.setup();
    // Identity creation can involve keytool; do not run it on Swing's EDT.
    try {
      DesktopApp app = new DesktopApp();
      app.identity = new SecurityStore(Path.of(System.getProperty("user.home"), ".jenga-remote"));
      SwingUtilities.invokeLater(
          () -> {
            try {
              app.build();
            } catch (Exception e) {
              JOptionPane.showMessageDialog(null, e.getMessage());
            }
          });
    } catch (Exception e) {
      System.err.println("Startup failed: " + e.getMessage());
    }
  }
}
