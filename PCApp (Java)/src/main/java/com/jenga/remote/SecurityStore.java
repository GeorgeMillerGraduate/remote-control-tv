package com.jenga.remote;

import java.nio.file.*;
import java.nio.file.attribute.*;
import java.security.*;
import java.security.cert.Certificate;
import java.util.*;
import javax.net.ssl.*;

/** Persistent server identity. Only credential hashes are retained by the server. */
public final class SecurityStore {
  public final Path directory;
  public final SSLContext context;
  public final String fingerprint;
  private static final SecureRandom RANDOM = new SecureRandom();

  public SecurityStore(Path directory) throws Exception {
    this.directory = directory;
    Files.createDirectories(directory);
    protect(directory, true);
    Path passwordFile = directory.resolve("identity.password");
    Path keyFile = directory.resolve("identity.p12");
    if (!Files.exists(passwordFile)) {
      Files.writeString(passwordFile, random());
      protect(passwordFile, false);
    }
    String password = Files.readString(passwordFile).trim();
    if (!Files.exists(keyFile)) {
      String tool = Path.of(System.getProperty("java.home"), "bin", "keytool").toString();
      Process process =
          new ProcessBuilder(
                  tool,
                  "-genkeypair",
                  "-alias",
                  "server",
                  "-keyalg",
                  "RSA",
                  "-keysize",
                  "3072",
                  "-validity",
                  "3650",
                  "-dname",
                  "CN=JengaRemote",
                  "-storetype",
                  "PKCS12",
                  "-keystore",
                  keyFile.toString(),
                  "-storepass:file",
                  passwordFile.toString())
              .redirectErrorStream(true)
              .start();
      process.getInputStream().transferTo(java.io.OutputStream.nullOutputStream());
      if (process.waitFor() != 0) throw new IllegalStateException("keytool failed");
    }
    protect(keyFile, false);
    KeyStore keys = KeyStore.getInstance("PKCS12");
    try (var stream = Files.newInputStream(keyFile)) {
      keys.load(stream, password.toCharArray());
    }
    Certificate certificate = keys.getCertificate("server");
    fingerprint = hash(certificate.getEncoded());
    KeyManagerFactory manager =
        KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
    manager.init(keys, password.toCharArray());
    context = SSLContext.getInstance("TLS");
    context.init(manager.getKeyManagers(), null, RANDOM);
  }

  public static String random() {
    byte[] bytes = new byte[32];
    RANDOM.nextBytes(bytes);
    return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
  }

  public static String hash(byte[] value) {
    try {
      return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(value));
    } catch (GeneralSecurityException e) {
      throw new IllegalStateException(e);
    }
  }

  public static String hash(String value) {
    return hash(value.getBytes(java.nio.charset.StandardCharsets.UTF_8));
  }

  public static boolean equal(String a, String b) {
    return MessageDigest.isEqual(
        a.getBytes(java.nio.charset.StandardCharsets.UTF_8),
        b.getBytes(java.nio.charset.StandardCharsets.UTF_8));
  }

  public static void protect(Path file, boolean directory) throws Exception {
    if (Files.getFileAttributeView(file, PosixFileAttributeView.class) != null) {
      Files.setPosixFilePermissions(
          file, PosixFilePermissions.fromString(directory ? "rwx------" : "rw-------"));
    } else {
      AclFileAttributeView acl = Files.getFileAttributeView(file, AclFileAttributeView.class);
      if (acl == null) throw new IllegalStateException("Private file permissions unavailable");
      acl.setAcl(
          List.of(
              AclEntry.newBuilder()
                  .setType(AclEntryType.ALLOW)
                  .setPrincipal(Files.getOwner(file))
                  .setPermissions(EnumSet.allOf(AclEntryPermission.class))
                  .build()));
    }
  }
}
