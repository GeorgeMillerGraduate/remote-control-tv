# Security model

This is an initial C++ source release, not an independently audited remote-access product.

- The phone verifies the exact server certificate SHA-256 fingerprint from the QR before sending any credentials. The field is named `serverPublicKeyFingerprint` for compatibility, but hashes the complete DER certificate, matching the Java implementation.
- Every control connection uses TLS 1.2 or later. Client authentication is a random 256-bit bearer credential inside that pinned TLS channel; it is not mutual-TLS certificate authentication. The server's VerifyNone mode means it does not demand a phone TLS certificate; it still rejects unauthenticated application commands.
- QR tokens are 256-bit random, expire after 120 seconds and are single-use. Desktop approval is mandatory. Closing/refreshing the QR window cancels pending pairing. Unauthenticated connections time out after 30 seconds; approve promptly.
- State is stored under the current user's Qt AppLocalDataLocation. On Windows it is normally `%LOCALAPPDATA%\JengaCode\JengaRemoteCpp`. Directories/files receive user-only ACLs, and the private key is DPAPI-encrypted for that Windows account. On Linux, the directory is mode 700 and files mode 600; the private key is not additionally encrypted there.
- Long-term phone credentials are never stored in plaintext by the server: only SHA-256 hashes of high-entropy random values are saved. Atomic private file writes fail rather than deliberately falling back to non-atomic direct writes.
- Permission changes disconnect existing sessions. Viewing is disabled on startup. Emergency disconnect releases held input and closes current sessions; it retains saved trust. Use Revoke trust to invalidate a phone's credential.
- One phone controls at a time, with 32 saved phones and eight connected WebSocket sessions. Input JSON is capped at 16 KiB and rate-limited to 240 messages/second/connection. Positive sequence numbers must strictly increase. An authenticated client must send its regular app PING before the 45-second idle timeout.
- Input values, keys, media commands and monitor coordinates are validated. No command executes client-supplied shell programs. Native APIs respect normal OS boundaries.
- Capture occurs on the GUI thread as required by Qt. JPEG resize/compression runs on a worker with one outstanding frame, and the permission/session are rechecked before sending it. This remains JPEG-over-TCP, without realtime guarantees.
- Logs contain connection and error events, not typed text, credentials or QR secrets. Device labels are self-reported. Discovery gives only public identity/address hints and cannot change the phone's pin or grant trust.

## Limitations

No formal fuzzing, Windows clean-machine deployment check or real phone/Windows integration test was completed here. TLS-handshake resource exhaustion and all local-network denial-of-service cases have not been exhaustively addressed. No per-IP cooldown or per-device permission policy is implemented. A compromised same-user Windows account can decrypt its DPAPI data. A stolen unlocked phone with an approved credential can control the PC until revoked.

The new C++ identity deliberately differs from the old Java identity. Pair it once as a new PC and forget the old entry when replacing the Java server. Never disable pin checks to migrate credentials.

On Linux text injection replaces the clipboard and uses Ctrl+V; clipboard history may retain the text. Windows sends UTF-16 units using SendInput instead, so it does not replace the clipboard. OS/browser password fields and elevated apps may behave differently; no credential-entry compatibility is claimed.

The UI has development-only `--capture-ui <path>` and `--export-brand-assets` switches. The first renders only this application's window, uses Qt test-state paths and skips auto-starting the server; the second exports the programmatically drawn icons and exits. Neither enables unattended access or authentication bypass.
