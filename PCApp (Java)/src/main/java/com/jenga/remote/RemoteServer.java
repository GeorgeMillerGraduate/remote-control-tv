package com.jenga.remote;

import com.google.gson.*;
import java.net.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.*;
import javax.swing.*;
import org.java_websocket.*;
import org.java_websocket.handshake.ClientHandshake;
import org.java_websocket.server.*;

public final class RemoteServer extends WebSocketServer {
  private static final Gson JSON = new GsonBuilder().setPrettyPrinting().create();
  private final SecurityStore store;
  private final InputController input;
  private final MediaController mediaController = new MediaController();
  private final Consumer<String> log;
  private final ConcurrentMap<WebSocket, Session> sessions = new ConcurrentHashMap<>();
  private final Properties trusted = new Properties();
  private final Properties names = new Properties();
  private final ThreadPoolExecutor commands =
      new ThreadPoolExecutor(
          1,
          1,
          0,
          TimeUnit.SECONDS,
          new ArrayBlockingQueue<>(512),
          new ThreadPoolExecutor.AbortPolicy());
  private final ExecutorService frames = Executors.newSingleThreadExecutor();
  private final java.util.concurrent.atomic.AtomicBoolean frameBusy =
      new java.util.concurrent.atomic.AtomicBoolean();
  private final ExecutorService approvals = Executors.newSingleThreadExecutor();
  private final ScheduledExecutorService clock = Executors.newSingleThreadScheduledExecutor();
  public volatile PairingTicket ticket;
  public volatile boolean mouse = true, keyboard = true, screen = false, media = true;
  private WebSocket controller;
  private DiscoveryService discovery;

  private static final class Session {
    volatile String id;
    boolean pending;
    long sequence, last = System.currentTimeMillis(), window = last;
    int count;
  }

  public RemoteServer(int port, SecurityStore store, Consumer<String> log) throws Exception {
    super(
        new InetSocketAddress(port),
        2,
        java.util.List.of(new org.java_websocket.drafts.Draft_6455(java.util.List.of(), 16384)));
    this.store = store;
    this.log = log;
    input = new InputController();
    setWebSocketFactory(new DefaultSSLWebSocketServerFactory(store.context));
    setConnectionLostTimeout(20);
    Path registry = store.directory.resolve("trusted.properties");
    if (Files.exists(registry))
      try (var stream = Files.newInputStream(registry)) {
        trusted.load(stream);
      }
    Path namesFile = store.directory.resolve("device-names.properties");
    if (Files.exists(namesFile))
      try (var stream = Files.newInputStream(namesFile)) {
        names.load(stream);
      }
    clock.scheduleAtFixedRate(
        () -> {
          long now = System.currentTimeMillis();
          sessions.forEach(
              (socket, session) -> {
                if (now - session.last > (session.id == null ? 30_000 : 45_000))
                  socket.close(1008, "Session timeout");
              });
        },
        5,
        5,
        TimeUnit.SECONDS);
  }

  @Override
  public void onOpen(WebSocket socket, ClientHandshake request) {
    if (sessions.size() >= 8) {
      socket.close(1013, "Server busy");
      return;
    }
    sessions.put(socket, new Session());
    send(socket, "HELLO", "version", 1);
  }

  @Override
  public void onMessage(WebSocket socket, String text) {
    if (text.length() > 16_384) {
      socket.close(1009, "Message too large");
      return;
    }
    try {
      commands.execute(() -> handle(socket, text));
    } catch (RejectedExecutionException e) {
      socket.close(1013, "Command queue full");
    }
  }

  @Override
  public void onMessage(WebSocket socket, java.nio.ByteBuffer bytes) {
    socket.close(1003, "JSON required");
  }

  private void handle(WebSocket socket, String text) {
    Session session = sessions.get(socket);
    if (session == null || !socket.isOpen()) return;
    try {
      long now = System.currentTimeMillis();
      if (now - session.window >= 1000) {
        session.window = now;
        session.count = 0;
      }
      if (++session.count > 240) throw new IllegalArgumentException("Rate exceeded");
      JsonObject message = JsonParser.parseString(text).getAsJsonObject();
      if (message.get("version").getAsInt() != 1)
        throw new IllegalArgumentException("Unsupported version");
      long sequence = message.get("seq").getAsLong();
      if (sequence <= session.sequence) throw new IllegalArgumentException("Invalid sequence");
      session.sequence = sequence;
      session.last = now;
      String type = message.get("type").getAsString();
      if (session.id == null) {
        if (session.pending) throw new IllegalArgumentException("Approval pending");
        if (type.equals("PAIR_REQUEST")) {
          PairingTicket current = ticket;
          if (current == null
              || !current.consume(
                  string(message, "pairingId", 100), string(message, "pairingSecret", 100), now))
            throw new IllegalArgumentException("Invalid or expired QR code");
          String name = string(message, "deviceName", 80);
          session.pending = true;
          approvals.execute(() -> approve(socket, session, name, current));
          return;
        }
        if (type.equals("AUTH_REQUEST")) {
          String id = string(message, "deviceId", 100);
          String token = string(message, "credential", 100);
          if (!SecurityStore.equal(trusted.getProperty(id, ""), SecurityStore.hash(token)))
            throw new IllegalArgumentException("Authentication failed");
          establish(socket, session, id);
          return;
        }
        throw new IllegalArgumentException("Authenticate first");
      }
      switch (type) {
        case "PING" -> send(socket, "PONG", "time", now);
        case "DISCONNECT" -> socket.close(1000, "Disconnected");
        case "MOUSE_MOVE" -> {
          require(mouse);
          input.move(integer(message, "dx", -500, 500), integer(message, "dy", -500, 500));
        }
        case "MOUSE_BUTTON" -> {
          require(mouse);
          input.button(integer(message, "button", 1, 3), message.get("down").getAsBoolean());
        }
        case "MOUSE_SCROLL" -> {
          require(mouse);
          input.scroll(integer(message, "amount", -30, 30));
        }
        case "KEY_DOWN", "KEY_UP" -> {
          require(keyboard);
          input.key(integer(message, "key", 8, 524), type.equals("KEY_DOWN"));
        }
        case "MEDIA_CONTROL" -> {
          require(media);
          mediaController.execute(string(message, "action", 30));
        }
        case "TEXT_INPUT" -> {
          require(keyboard);
          input.text(string(message, "text", 4000));
        }
        case "SCREEN_REQUEST" -> {
          require(screen);
          int display = integer(message, "monitor", 0, monitorCount() - 1);
          int quality = integer(message, "quality", 20, 85);
          if (!socket.hasBufferedData() && frameBusy.compareAndSet(false, true))
            frames.execute(
                () -> {
                  try {
                    byte[] frame = input.capture(display, quality);
                    if (screen
                        && socket.isOpen()
                        && sessions.get(socket) == session
                        && session.id != null) socket.send(frame);
                  } catch (Exception e) {
                    send(socket, "ERROR", "message", "Screen capture failed");
                  } finally {
                    frameBusy.set(false);
                  }
                });
        }
        case "DESKTOP_POINT" -> {
          require(mouse && screen);
          double x = message.get("x").getAsDouble(), y = message.get("y").getAsDouble();
          if (!Double.isFinite(x) || !Double.isFinite(y) || x < 0 || x > 1 || y < 0 || y > 1)
            throw new IllegalArgumentException("Invalid point");
          input.absolute(integer(message, "monitor", 0, monitorCount() - 1), x, y);
        }
        default -> throw new IllegalArgumentException("Unsupported command");
      }
    } catch (Exception e) {
      send(
          socket,
          "ERROR",
          "message",
          e instanceof IllegalArgumentException ? e.getMessage() : "Command failed");
      // Releasing held input is essential even on malformed commands.
      if (controller == socket) input.releaseAll();
      if (session.id == null) socket.close(1008, "Authentication failed");
    }
  }

  private void approve(WebSocket socket, Session session, String name, PairingTicket pairing) {
    int[] result = {JOptionPane.NO_OPTION};
    try {
      SwingUtilities.invokeAndWait(
          () ->
              result[0] =
                  JOptionPane.showConfirmDialog(
                      null,
                      "Device (self-reported): "
                          + name
                          + "\nAddress: "
                          + socket.getRemoteSocketAddress()
                          + "\nApprove only the phone you are pairing.",
                      "New device pairing request",
                      JOptionPane.YES_NO_OPTION));
      commands.execute(
          () -> {
            if (sessions.get(socket) != session || !socket.isOpen()) return;
            if (!pairing.approvalAllowed(System.currentTimeMillis())) {
              socket.close(1008, "Pairing cancelled or expired");
              return;
            }
            if (result[0] != JOptionPane.YES_OPTION) {
              send(socket, "PAIR_REJECTED", "message", "Rejected");
              socket.close();
              return;
            }
            if (controller != null && controller.isOpen()) {
              socket.close(1008, "Another controller is active");
              return;
            }
            if (trusted.size() >= 32) {
              socket.close(1008, "Trust registry full");
              return;
            }
            String id = UUID.randomUUID().toString(), credential = SecurityStore.random();
            trusted.setProperty(id, SecurityStore.hash(credential));
            names.setProperty(id, name);
            try {
              save();
              establish(socket, session, id);
              send(socket, "PAIR_APPROVAL", "deviceId", id, "credential", credential);
            } catch (Exception e) {
              trusted.remove(id);
              try {
                save();
              } catch (Exception ignored) {
              }
              socket.close(1011, "Could not save trust");
            }
          });
    } catch (Exception e) {
      socket.close(1011, "Approval failed");
    }
  }

  private void establish(WebSocket socket, Session session, String id) {
    if (controller != null && controller != socket && controller.isOpen())
      throw new IllegalArgumentException("Another phone is controlling this PC");
    controller = socket;
    session.id = id;
    session.pending = false;
    send(socket, "AUTH_SUCCESS", "monitors", monitorCount());
    log.accept("Remote control active: " + id);
  }

  private int monitorCount() {
    return java.awt.GraphicsEnvironment.getLocalGraphicsEnvironment().getScreenDevices().length;
  }

  public void devices(Consumer<Map<String, String>> callback) {
    commands.execute(
        () -> {
          Map<String, String> result = new TreeMap<>();
          for (String id : trusted.stringPropertyNames())
            result.put(id, names.getProperty(id, "Android phone"));
          SwingUtilities.invokeLater(() -> callback.accept(result));
        });
  }

  public void rename(String id, String name) {
    commands.execute(
        () -> {
          if (trusted.containsKey(id) && name.length() <= 80) {
            names.setProperty(id, name);
            try {
              save();
            } catch (Exception e) {
              log.accept("Device name save failed");
            }
          }
        });
  }

  public void revoke(String id) {
    commands.execute(
        () -> {
          trusted.remove(id);
          names.remove(id);
          sessions.forEach(
              (socket, session) -> {
                if (id.equals(session.id)) socket.close(1000, "Trust revoked");
              });
          try {
            save();
          } catch (Exception e) {
            log.accept("Trust revocation could not be persisted");
          }
        });
  }

  public void disconnectDevice(String id) {
    sessions.forEach(
        (socket, session) -> {
          if (id.equals(session.id)) socket.close(1000, "Disconnected by PC");
        });
  }

  private void save() throws Exception {
    atomicProperties("device-names.properties", names, "Self-reported device names");
    atomicProperties("trusted.properties", trusted, "JengaRemote credential hashes");
  }

  private void atomicProperties(String name, Properties values, String comment) throws Exception {
    Path temporary = Files.createTempFile(store.directory, "registry-", ".tmp");
    try {
      SecurityStore.protect(temporary, false);
      try (var stream = Files.newOutputStream(temporary)) {
        values.store(stream, comment);
      }
      Files.move(
          temporary,
          store.directory.resolve(name),
          StandardCopyOption.ATOMIC_MOVE,
          StandardCopyOption.REPLACE_EXISTING);
    } finally {
      Files.deleteIfExists(temporary);
    }
  }

  public void revokeAll() {
    commands.execute(
        () -> {
          trusted.clear();
          names.clear();
          try {
            save();
          } catch (Exception e) {
            log.accept("Trust save failed");
          }
          emergency();
        });
  }

  public void emergency() {
    sessions.keySet().forEach(s -> s.close(1000, "Desktop stopped session"));
    commands.execute(input::releaseAll);
  }

  public void closeServer() throws Exception {
    if (ticket != null) ticket.invalidate();
    if (discovery != null) discovery.close();
    emergency();
    stop(1000);
    clock.shutdownNow();
    frames.shutdownNow();
    approvals.shutdownNow();
    commands.shutdown();
  }

  @Override
  public void onClose(WebSocket socket, int code, String reason, boolean remote) {
    sessions.remove(socket);
    try {
      commands.execute(
          () -> {
            if (controller == socket) {
              input.releaseAll();
              controller = null;
              log.accept("Remote control disconnected");
            }
          });
    } catch (RejectedExecutionException ignored) {
    }
  }

  @Override
  public void onError(WebSocket socket, Exception e) {
    log.accept("Network error: " + e.getClass().getSimpleName());
  }

  @Override
  public void onStart() {
    try {
      discovery = new DiscoveryService(getPort(), store.fingerprint);
    } catch (Exception e) {
      log.accept("LAN discovery unavailable; QR pairing still works");
    }
    log.accept("Encrypted server listening on " + getPort());
  }

  private static void require(boolean permission) {
    if (!permission) throw new IllegalArgumentException("Permission disabled on PC");
  }

  private static int integer(JsonObject object, String key, int min, int max) {
    double value = object.get(key).getAsDouble();
    if (!Double.isFinite(value) || value != Math.rint(value) || value < min || value > max)
      throw new IllegalArgumentException("Invalid " + key);
    return (int) value;
  }

  private static String string(JsonObject object, String key, int max) {
    String value = object.get(key).getAsString();
    if (value.length() > max) throw new IllegalArgumentException("Field too long");
    return value;
  }

  private static void send(WebSocket socket, String type, Object... fields) {
    if (!socket.isOpen()) return;
    JsonObject value = new JsonObject();
    value.addProperty("version", 1);
    value.addProperty("type", type);
    for (int i = 0; i < fields.length; i += 2)
      value.add(fields[i].toString(), JSON.toJsonTree(fields[i + 1]));
    socket.send(JSON.toJson(value));
  }
}
