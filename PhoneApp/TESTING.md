# Validation record and device acceptance checks

## Executed in the build environment

- All desktop Java source compiled successfully with Eclipse ECJ using Java 17 bytecode. The project itself targets Java 21 by default; the source uses APIs compatible with both. This is not a completed JDK 21 Maven packaging run.
- Seven JUnit tests passed: successful/single-use QR, wrong token rejection without consuming a valid token, expiry boundary, cancellation of pending approval, concurrent redemption, persistent server certificate/private permissions, and distinct 256-bit credentials.
- All Android Java source compiled with javac against Android 15/API 35 compatible framework classes and the referenced AndroidX/CameraX/ML Kit/OkHttp API classes. A temporary generated-R stub was used only for this compile check and is not included in the Android project.
- XML parsed for well-formedness. Gradle wrapper files fetched from Gradle v8.9.0; distribution checksum obtained from Gradle's distribution service.
- Maven could not complete dependency/plugin resolution in this environment due Java networking/DNS restrictions. Android SDK packaging tools and an attached device were unavailable. No signed APK, full Gradle resource/dex build, visual UI test, or live phone-to-PC integration test was completed.

These checks establish source compilation and selected security properties, not end-to-end operation or production readiness.

## Run locally

Desktop: `mvn clean verify` with JDK 21. Android: `gradlew.bat assembleDebug` or `./gradlew assembleDebug`, then install on a phone.

## Required manual acceptance checks

1. Scan valid QR; verify no pointer movement before PC approval. Approve and confirm connected on both sides.
2. Try reject, closing the QR window, a 120-second-old code, two simultaneous scans and wrong pin/token. Each must fail without issuing a saved credential.
3. Reopen both apps and reconnect without QR. Revoke one phone on PC, then verify reconnect fails. Pair a second PC and switch between them.
4. Move, click/double click/right click, scroll and drag. Hold a key or button while disconnecting Wi-Fi; ensure the PC releases it.
5. Type ASCII and Unicode into an ordinary editor. Verify Enter, Tab, arrows, F keys, Ctrl+C/V/Z, Alt+Tab and presentation keys. Check clipboard side effects explicitly.
6. Enable/disable each PC permission. No unauthorized commands may take effect. Exercise emergency stop with input held. Verify a second phone cannot take control concurrently.
7. Enable viewing, test each display including monitors with negative desktop origins. Test tap, pinch, pan, drag and JPEG quality. Confirm no frames before permission/authentication.
8. Play/pause, tracks and volume on Windows; repeat with playerctl/pactl on Linux X11. Missing helpers should return useful errors.
9. Change Wi-Fi/mobile-data connections through an existing private VPN. Confirm saved pin remains enforced. Test PC sleep, dropped connection, app background/foreground and address discovery.
10. Leave running for an hour, inspect memory/CPU, review logs for credential/input leakage, and exercise a slow connection. Send malformed/oversized/rapid protocol traffic and verify fail-closed behavior.

Wayland must display an unsupported-session error. Windows secure desktop and Ctrl+Alt+Delete must not be represented as supported.
