package com.jenga.remote;

import static org.junit.jupiter.api.Assertions.*;

import java.nio.file.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

public final class SecurityTest {
  @TempDir Path directory;

  @Test
  void validTicketIsSingleUse() {
    PairingTicket ticket = new PairingTicket();
    assertTrue(ticket.consume(ticket.id, ticket.secret, System.currentTimeMillis()));
    assertFalse(ticket.consume(ticket.id, ticket.secret, System.currentTimeMillis()));
  }

  @Test
  void wrongSecretDoesNotConsumeTicket() {
    PairingTicket ticket = new PairingTicket();
    assertFalse(ticket.consume(ticket.id, "wrong", System.currentTimeMillis()));
    assertTrue(ticket.consume(ticket.id, ticket.secret, System.currentTimeMillis()));
  }

  @Test
  void expiryBoundaryFailsClosed() {
    PairingTicket ticket = new PairingTicket();
    assertFalse(ticket.consume(ticket.id, ticket.secret, ticket.expiresAt));
    assertFalse(ticket.approvalAllowed(ticket.expiresAt));
  }

  @Test
  void cancellationAlsoCancelsPendingApproval() {
    PairingTicket ticket = new PairingTicket();
    assertTrue(ticket.consume(ticket.id, ticket.secret, System.currentTimeMillis()));
    ticket.invalidate();
    assertFalse(ticket.approvalAllowed(System.currentTimeMillis()));
  }

  @Test
  void concurrentRequestsCannotReuseQr() throws Exception {
    PairingTicket ticket = new PairingTicket();
    AtomicInteger successes = new AtomicInteger();
    ExecutorService pool = Executors.newFixedThreadPool(8);
    for (int i = 0; i < 40; i++)
      pool.submit(
          () -> {
            if (ticket.consume(ticket.id, ticket.secret, System.currentTimeMillis()))
              successes.incrementAndGet();
          });
    pool.shutdown();
    assertTrue(pool.awaitTermination(5, TimeUnit.SECONDS));
    assertEquals(1, successes.get());
  }

  @Test
  void persistentIdentityDoesNotChangeAcrossRestarts() throws Exception {
    SecurityStore first = new SecurityStore(directory);
    SecurityStore second = new SecurityStore(directory);
    assertEquals(first.fingerprint, second.fingerprint);
    assertTrue(first.fingerprint.matches("[0-9a-f]{64}"));
    if (Files.getFileStore(directory).supportsFileAttributeView("posix")) {
      assertEquals(
          "rw-------",
          java.nio.file.attribute.PosixFilePermissions.toString(
              Files.getPosixFilePermissions(directory.resolve("identity.p12"))));
    }
  }

  @Test
  void randomCredentialsAreDistinctAndNotRawHashes() {
    String first = SecurityStore.random(), second = SecurityStore.random();
    assertNotEquals(first, second);
    assertEquals(43, first.length());
    assertFalse(SecurityStore.equal(SecurityStore.hash(first), first));
  }
}
