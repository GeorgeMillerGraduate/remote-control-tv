package com.jenga.remote;

/** One atomic, short lived pairing capability, consumed before approval begins. */
public final class PairingTicket {
  public final String id = SecurityStore.random();
  public final String secret = SecurityStore.random();
  public final long expiresAt = System.currentTimeMillis() + 120_000;
  private boolean used;
  private boolean cancelled;

  public synchronized boolean consume(String suppliedId, String suppliedSecret, long now) {
    if (used
        || now >= expiresAt
        || !SecurityStore.equal(id, suppliedId)
        || !SecurityStore.equal(secret, suppliedSecret)) return false;
    used = true;
    return true;
  }

  public synchronized void invalidate() {
    used = true;
    cancelled = true;
  }

  public synchronized boolean approvalAllowed(long now) {
    return !cancelled && now < expiresAt;
  }
}
