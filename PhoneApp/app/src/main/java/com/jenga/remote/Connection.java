package com.jenga.remote;

import java.security.*;
import java.security.cert.*;
import java.util.concurrent.TimeUnit;
import javax.net.ssl.*;
import okhttp3.*;
import okio.ByteString;
import org.json.JSONObject;

/** TLS uses exact out-of-band certificate identity, including for private IP addresses. */
public final class Connection extends WebSocketListener {
  public interface Listener {
    void message(JSONObject value);

    void frame(byte[] bytes);

    void status(String text);
  }

  private final Listener listener;
  private volatile WebSocket socket;
  private OkHttpClient client;
  private long sequence;
  private JSONObject computer;
  private boolean pairing;
  public volatile boolean authenticated;

  public Connection(Listener listener) {
    this.listener = listener;
  }

  public void connect(JSONObject computer, boolean pairing) throws Exception {
    disconnect();
    this.computer = computer;
    this.pairing = pairing;
    sequence = 0;
    String pin = computer.getString("serverPublicKeyFingerprint");
    if (!pin.matches("[a-fA-F0-9]{64}"))
      throw new IllegalArgumentException("Invalid server identity");
    X509TrustManager trust =
        new X509TrustManager() {
          @Override
          public void checkClientTrusted(X509Certificate[] c, String a)
              throws CertificateException {
            throw new CertificateException("Client mode only");
          }

          @Override
          public X509Certificate[] getAcceptedIssuers() {
            return new X509Certificate[0];
          }

          @Override
          public void checkServerTrusted(X509Certificate[] chain, String auth)
              throws CertificateException {
            try {
              chain[0].checkValidity();
              byte[] digest = MessageDigest.getInstance("SHA-256").digest(chain[0].getEncoded());
              StringBuilder hex = new StringBuilder();
              for (byte b : digest)
                hex.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
              if (!MessageDigest.isEqual(
                  pin.toLowerCase(java.util.Locale.ROOT)
                      .getBytes(java.nio.charset.StandardCharsets.US_ASCII),
                  hex.toString().getBytes(java.nio.charset.StandardCharsets.US_ASCII)))
                throw new CertificateException("Server identity changed");
            } catch (GeneralSecurityException e) {
              throw new CertificateException(e);
            }
          }
        };
    SSLContext tls = SSLContext.getInstance("TLS");
    tls.init(null, new TrustManager[] {trust}, new SecureRandom());
    client =
        new OkHttpClient.Builder()
            .sslSocketFactory(tls.getSocketFactory(), trust)
            // Name validation is replaced ONLY by the exact certificate check above.
            .hostnameVerifier((hostname, session) -> hostname.equals(computer.optString("host")))
            .pingInterval(10, TimeUnit.SECONDS)
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(0, TimeUnit.SECONDS)
            .build();
    String host = computer.getString("host");
    int port = computer.getInt("port");
    if (port < 1024 || port > 65535 || !host.matches("[a-zA-Z0-9.:-]{1,253}"))
      throw new IllegalArgumentException("Invalid address");
    String address = host.contains(":") ? "[" + host + "]" : host;
    socket =
        client.newWebSocket(
            new Request.Builder().url("https://" + address + ":" + port + "/").build(), this);
    listener.status("Connecting…");
  }

  @Override
  public void onOpen(WebSocket socket, Response response) {
    if (pairing)
      send(
          "PAIR_REQUEST",
          "pairingId",
          computer.optString("pairingId"),
          "pairingSecret",
          computer.optString("pairingSecret"),
          "deviceName",
          android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL);
    else
      send(
          "AUTH_REQUEST",
          "deviceId",
          computer.optString("deviceId"),
          "credential",
          computer.optString("credential"));
  }

  public synchronized void send(String type, Object... fields) {
    if (socket == null) return;
    try {
      JSONObject value =
          new JSONObject().put("version", 1).put("seq", ++sequence).put("type", type);
      for (int i = 0; i < fields.length; i += 2) value.put(fields[i].toString(), fields[i + 1]);
      if (socket.queueSize() > 512_000) {
        disconnect();
        listener.status("Connection congested; reconnect");
        return;
      }
      socket.send(value.toString());
    } catch (Exception e) {
      listener.status("Could not send command");
    }
  }

  @Override
  public void onMessage(WebSocket socket, String text) {
    if (socket != this.socket) return;
    if (text.length() > 32768) {
      socket.close(1009, "Message too large");
      return;
    }
    try {
      JSONObject value = new JSONObject(text);
      if (value.optString("type").equals("AUTH_SUCCESS")) authenticated = true;
      listener.message(value);
    } catch (Exception e) {
      listener.status("Invalid server response");
    }
  }

  @Override
  public void onMessage(WebSocket socket, ByteString bytes) {
    if (socket == this.socket && authenticated && bytes.size() < 2_000_000)
      listener.frame(bytes.toByteArray());
  }

  @Override
  public void onFailure(WebSocket socket, Throwable error, Response response) {
    if (socket != this.socket) return;
    authenticated = false;
    listener.status(
        "Connection failed: "
            + error.getClass().getSimpleName()
            + ". Check IP, VPN and certificate.");
  }

  @Override
  public void onClosed(WebSocket socket, int code, String reason) {
    if (socket != this.socket) return;
    authenticated = false;
    listener.status("Disconnected: " + reason);
  }

  @Override
  public void onClosing(WebSocket socket, int code, String reason) {
    socket.close(code, reason);
  }

  public synchronized void disconnect() {
    authenticated = false;
    if (socket != null) {
      socket.close(1000, "Phone disconnected");
      socket = null;
    }
    if (client != null) {
      client.dispatcher().executorService().shutdown();
      client.connectionPool().evictAll();
      client = null;
    }
  }
}
