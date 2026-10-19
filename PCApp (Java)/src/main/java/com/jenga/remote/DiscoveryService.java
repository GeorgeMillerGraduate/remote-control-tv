package com.jenga.remote;

import com.google.gson.JsonObject;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicBoolean;

/** Discovery is an unauthenticated hint; it never grants trust or changes a pin. */
public final class DiscoveryService implements AutoCloseable {
  private final DatagramSocket socket;
  private final AtomicBoolean running = new AtomicBoolean(true);

  public DiscoveryService(int port, String fingerprint) throws Exception {
    socket = new DatagramSocket(port);
    socket.setSoTimeout(1000);
    Thread worker =
        new Thread(
            () -> {
              byte[] buffer = new byte[128];
              long next = 0;
              while (running.get())
                try {
                  DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                  socket.receive(packet);
                  if (!packet.getAddress().isSiteLocalAddress()
                      && !packet.getAddress().isLoopbackAddress()) continue;
                  if (System.currentTimeMillis() < next) continue;
                  if (!new String(packet.getData(), 0, packet.getLength(), StandardCharsets.UTF_8)
                      .equals("JENGA_DISCOVER_1")) continue;
                  next = System.currentTimeMillis() + 100;
                  JsonObject value = new JsonObject();
                  value.addProperty("protocol", "JengaRemote");
                  value.addProperty("version", 1);
                  value.addProperty("port", port);
                  value.addProperty("serverPublicKeyFingerprint", fingerprint);
                  byte[] reply = value.toString().getBytes(StandardCharsets.UTF_8);
                  socket.send(
                      new DatagramPacket(
                          reply, reply.length, packet.getAddress(), packet.getPort()));
                } catch (SocketTimeoutException ignored) {
                } catch (Exception e) {
                  if (running.get()) break;
                }
            },
            "lan-discovery");
    worker.setDaemon(true);
    worker.start();
  }

  @Override
  public void close() {
    running.set(false);
    socket.close();
  }
}
