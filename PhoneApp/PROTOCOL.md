# JengaRemote protocol v1

## Transport and identity

Control is JSON UTF-8 over TLS WebSocket (`wss`). The Android client connects to the root path. The server sends binary JPEG messages only after an authenticated, permitted screen request. TLS authenticates the server through an exact SHA-256 certificate DER pin obtained from the QR. This field retains the master prompt's name serverPublicKeyFingerprint, but it hashes the certificate, not just SubjectPublicKeyInfo. Certificate regeneration therefore requires new pairing.

Client authentication uses a random 256-bit bearer credential inside that pinned TLS channel. This is mutual authentication at the application level, not mutual TLS certificates. Do not accept plaintext fallback or substitute an accept-all trust manager.

Each client JSON message contains `version: 1`, `type`, and a positive strictly increasing integer `seq`. Sequence resets for each WebSocket connection. Server JSON includes version/type but no seq. Version mismatch fails; no downgrade negotiation is implemented.

## Pairing QR

Fields: protocol=`JengaRemote`, version=1, deviceName, host, port, pairingId, pairingSecret, expiresAt (epoch milliseconds), serverPublicKeyFingerprint (64 lowercase hexadecimal characters). Version 1 in this implementation uses an epoch integer, not the conceptual prompt's ISO timestamp.

## Messages

| Direction | Type | Additional fields |
|---|---|---|
| PC → phone | HELLO | version |
| Phone → PC | PAIR_REQUEST | pairingId, pairingSecret, deviceName |
| PC → phone | AUTH_SUCCESS | monitors |
| PC → phone | PAIR_APPROVAL | deviceId, credential |
| PC → phone | PAIR_REJECTED | message |
| Phone → PC | AUTH_REQUEST | deviceId, credential |
| Phone → PC | MOUSE_MOVE | dx, dy: integers −500…500 |
| Phone → PC | MOUSE_BUTTON | button: 1 left / 2 middle / 3 right; down: boolean |
| Phone → PC | MOUSE_SCROLL | amount: integer −30…30 |
| Phone → PC | KEY_DOWN / KEY_UP | key: Java AWT virtual-key code 8…524; platform may reject unsupported codes |
| Phone → PC | TEXT_INPUT | text: ≤4000 Java characters |
| Phone → PC | MEDIA_CONTROL | action: PLAY_PAUSE / PREVIOUS / NEXT / VOLUME_UP / VOLUME_DOWN / MUTE |
| Phone → PC | SCREEN_REQUEST | monitor: zero-based; quality: 20…85 |
| PC → phone | binary | one JPEG frame, resized to max width 1280 |
| Phone → PC | DESKTOP_POINT | monitor; x, y: finite normalized values 0…1 |
| Phone → PC | PING | no fields |
| PC → phone | PONG | time: server epoch milliseconds |
| Either | DISCONNECT / WebSocket close | client DISCONNECT is optional |
| PC → phone | ERROR | message: error without input contents |

A click is ordered MOUSE_BUTTON down/up, rather than a separate MOUSE_CLICK message. Authentication failures are ERROR plus close 1008, rather than AUTH_FAILURE. AUTH_SUCCESS precedes PAIR_APPROVAL in first pairing; only PAIR_APPROVAL contains credentials. The phone must save those credentials before announcing pairing as saved.

## Limits and scheduling

Inbound server frame/assembled-message limit: 16 KiB. Client JSON additionally limited to 16,384 Java characters. At most 240 commands per second per session, eight connected sockets, 32 trusted devices and one authenticated controlling connection. The global command queue has 512 entries; saturation closes the sender. Only one frame capture task may run at once. A new screen request is dropped when capture or outgoing buffers are busy. Frames can share the WebSocket TCP queue with input: no claim of hard realtime prioritization.

WebSocket transport pings run separately. App PING refreshes the 45-second authenticated idle deadline; unauthenticated deadline is 30 seconds. Approval does not extend the QR's 120-second lifetime. The Android foreground app polls screen at at most four requests per second and pauses requests on permission error. Re-enter Desktop to retry after permission changes.

On network failure the phone retries up to five times with bounded backoff, while foregrounded and holding a saved credential. On returning from the background it attempts to reconnect to its current saved PC. Intentional disconnect or trust revocation does not trigger continuous reconnects.
