package com.jenga.remote;

import android.content.Context;
import android.security.keystore.*;
import android.util.Base64;
import java.security.KeyStore;
import javax.crypto.*;
import javax.crypto.spec.GCMParameterSpec;
import org.json.JSONArray;

/** Whole registry encrypted with a non-exportable AES key in Android Keystore. */
public final class Vault {
  private final Context context;
  private final SecretKey key;

  public Vault(Context context) throws Exception {
    this.context = context;
    KeyStore store = KeyStore.getInstance("AndroidKeyStore");
    store.load(null);
    if (!store.containsAlias("jenga-trust")) {
      KeyGenerator generator = KeyGenerator.getInstance("AES", "AndroidKeyStore");
      generator.init(
          new KeyGenParameterSpec.Builder(
                  "jenga-trust", KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
              .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
              .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
              .build());
      generator.generateKey();
    }
    key = (SecretKey) store.getKey("jenga-trust", null);
  }

  public JSONArray load() throws Exception {
    String encoded = context.getSharedPreferences("vault", 0).getString("registry", null);
    if (encoded == null) return new JSONArray();
    byte[] bytes = Base64.decode(encoded, Base64.NO_WRAP);
    Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
    cipher.init(Cipher.DECRYPT_MODE, key, new GCMParameterSpec(128, bytes, 0, 12));
    return new JSONArray(
        new String(
            cipher.doFinal(bytes, 12, bytes.length - 12), java.nio.charset.StandardCharsets.UTF_8));
  }

  public void save(JSONArray registry) throws Exception {
    Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
    cipher.init(Cipher.ENCRYPT_MODE, key);
    byte[] encrypted =
        cipher.doFinal(registry.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8));
    byte[] packed = new byte[12 + encrypted.length];
    System.arraycopy(cipher.getIV(), 0, packed, 0, 12);
    System.arraycopy(encrypted, 0, packed, 12, encrypted.length);
    if (!context
        .getSharedPreferences("vault", 0)
        .edit()
        .putString("registry", Base64.encodeToString(packed, Base64.NO_WRAP))
        .commit()) throw new java.io.IOException("Could not save trusted computer");
  }
}
