# Jenga Remote 0.1.0

Java Android controller and Java Swing PC server. This is an initial source release, not a fully device-tested production release. No APK or installer has been verified. Read TESTING.md before relying on it.

## Start here

1. Open DesktopRemoteServer in NetBeans as a Maven project using JDK 21.
2. Build it, then run `java -jar target/remote-server-0.1.0.jar`.
3. Click Start server. Check the advertised address is the PC's reachable Wi-Fi address.
4. Click Connect Phone — Show QR Code and leave that window open.
5. Open AndroidRemoteClient in Android Studio. Build and run on an Android 8+ phone.
6. Scan the PC QR code. Approve the request on the PC within the pairing countdown.
7. Use Touchpad or Keyboard. Enable screen viewing on the PC before opening Desktop.

Both source projects are independently buildable. Documentation is duplicated into each archive so neither depends on files from the other archive. The protocol is documented in PROTOCOL.md.

## Implemented in source

- TLS WebSocket connection, exact server certificate pin, temporary 120-second single-use QR token, desktop approval and cancellation.
- Keystore-encrypted Android saved PCs, persistent PC identity, hashed client credentials, saved-PC reconnect and bounded retries after connection failures.
- Multiple saved PCs and trusted phones, one active controller, phone rename/disconnect/revocation on the PC.
- Touchpad movement, tap/double tap, two-finger right click/scroll, long-press drag, dedicated click buttons and sensitivity adjustment.
- Unicode clipboard paste, special keys, F1–F12, common shortcuts and presentation navigation.
- Windows media keys through JNA; Linux playerctl/pactl helpers.
- Explicit desktop screen permission, monitor selection, JPEG quality control, frame backpressure, zoom, pan and long-press desktop drag.
- Local discovery for already saved computers, QR/manual JSON pairing, editable saved LAN/VPN addresses.
- Emergency disconnect, mouse/keyboard/media/screen permissions, connection logs, start/stop server.

## Scope still outstanding

This release does not implement every polish item in the supplied master prompt. Remaining items include standalone modifier toggles, configurable session limits, per-device permissions, a persistent pairing/audit history, rich latency/connection-quality display, hardware accelerated video, system-tray/startup integration, pointer acceleration/natural-scroll/left-handed options, fullscreen and keyboard-overlay viewer controls, automatic discovery of new PCs, a settings editor for all server policies, a desktop Refresh QR button (close/reopen instead), and a complete device/OS integration test suite.

No public relay or NAT traversal is provided. Internet use requires an already configured WireGuard/Tailscale private network. Wayland, Ctrl+Alt+Delete, elevated/secure Windows desktops and login bypass are unsupported. The Android application disconnects when backgrounded. A saved connection resumes on return to the foreground; deliberate PC disconnects do not trigger infinite reconnects.

See BUILDING.md, PAIRING.md, SECURITY.md and TROUBLESHOOTING.md.
