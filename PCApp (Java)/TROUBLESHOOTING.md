# Troubleshooting

**NetBeans shows the wrong Java version:** select JDK 21 for the Maven project and its Java platform. Run `java -version` in the same terminal you use to launch it. Use a full JDK for keytool.

**Phone cannot connect:** verify both addresses, server running, advertised port, host firewall and VPN ACL. A 192.168.x.x address normally works only on that LAN. Guest Wi-Fi/AP isolation can block device-to-device connections. Use a reachable VPN address when on mobile data.

**Pin / certificate error:** verify PC identity and phone date/time. A reset server identity requires deleting that saved PC and pairing again. Do not work around it with disabled certificate checks.

**Pairing expired or approval has no effect:** keep the QR window open, scan a fresh code and approve promptly. Connection authentication times out after 30 seconds. The PC accepts one pending approval worker and one active controller; disconnect the other controller first.

**QR too dense:** increase desktop UI scaling/window size or bring the phone closer. The QR is 480×480 with a white border. Use a second display to scan remotely; gallery import is not provided.

**Discovery finds nothing:** discovery is optional and checks only saved PCs at default UDP 8765. Use QR pairing or edit a saved address manually. Broadcast filtering, custom ports and VPNs can prevent discovery without preventing direct connections.

**Mouse/keyboard do nothing:** enable the corresponding PC permission, ensure the phone is authenticated, and use a normal desktop application. Wayland and Windows secure/elevated desktops may reject Robot input. Use Linux X11; do not disable OS security.

**Unicode sends no text:** the editor must accept Ctrl+V and have focus. This mode replaces the PC clipboard. Some terminals use Ctrl+Shift+V instead, which this release does not automatically choose.

**No desktop image:** enable Allow screen viewing on PC, reconnect after the permission change and reopen Desktop. Select a valid monitor. The viewer uses JPEG, up to four requested frames per second, not full-motion video.

**Media unavailable:** Windows uses JNA user32 media keys; the focused player/OS decides how to handle them. Linux requires playerctl for tracks/playback and pactl for volume. Test those tools as the same ordinary desktop user.

**Connection stops when switching apps:** intentional foreground-only behavior releases PC input when the controller leaves the screen. Return to the controller to reconnect to a saved PC. Retry budget after network failure is five attempts; use Connect to start another attempt.

**Camera denied:** allow Camera in Android app settings and scan again. The scanner uses the bundled ML Kit model and does not upload camera frames.

**Gradle fails:** use compatible JDK 17/Studio JDK, install SDK 35, check SDK path, internet access to Google/Maven/Gradle repositories and wrapper distribution checksum. This archive contains source, not a preverified APK.
