# Executed checks

## In this environment

- Built the application and test executable with GCC 13.3, C++17, CMake/Ninja, Qt 6.4.2 and OpenSSL 3.0.13 on Linux.
- Ran the Qt test suite over real loopback TLS WebSockets using a fake input backend. Cases cover single-use/wrong QR secrets, expiry/cancellation, credential randomness and SHA-256, persistent identity/private permissions, approval before trust, authenticated mouse/key command dispatch, sequence replay rejection, saved-credential reconnection, viewing denied by default, revocation/disconnect and rejected revoked credentials.
- Rendered and inspected the actual Qt dashboard in the offscreen Qt platform. Verified the icon PNG and ICO sizes.

The fake input backend records commands without controlling this environment's desktop. This is not a Windows build test or proof of real mouse/keyboard injection. The Windows workflow is supplied but has not been run; the final remote-control EXE is not in this source archive. The new build bootstrapper is separately cross-compiled as a native Windows x64 GUI EXE. Its PE structure, imports and embedded icon/manifest are checked, and the PowerShell script is syntax-parsed. It has not been executed on Windows here.

## Run on Windows

`BUILD-WINDOWS.cmd` calls the PowerShell build script, which runs CTest before deployment. Test programs and Qt DLLs are on PATH for the test run. Qt's offscreen platform is used for protocol tests.

## Manual acceptance

1. Stop the Java server, build the native app, launch its EXE, verify the embedded shortcut/taskbar icon and clear connection badge.
2. Scan a fresh QR with the existing Android app, refuse a request, try an expired/cancelled QR and verify no trust or control before approval. Pair successfully; close both apps and reconnect with saved credentials.
3. Verify click/double click/right click, wheel, long-press drag, special keys, shortcut combinations, Unicode text and native media controls in ordinary applications.
4. Disconnect while holding a key/button. Verify it releases on the PC. Exercise emergency disconnect and each permission toggle.
5. Enable viewing, reconnect, check every monitor, portrait layouts, negative monitor origins and mixed-DPI displays. Test the phone viewer's tap/zoom/pan/drag.
6. Rename/revoke a trusted phone; verify revoked credentials fail. Try a second phone while one controls the PC.
7. Repeat through a configured private VPN from mobile data. Test Wi-Fi loss, PC sleep, slow connections and Android background/foreground transitions.
8. Copy the entire deployed Windows folder onto a clean Windows x64 PC with no Java/Qt development tools. Confirm Qt platform, JPEG, TLS plugins, OpenSSL and MSVC runtime files were gathered successfully.
9. Run a longer session and review CPU/memory, input ordering, display lag and logs. Do not call the release production-ready until these checks pass.
