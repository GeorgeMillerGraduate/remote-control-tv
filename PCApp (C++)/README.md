# Jenga Remote — C++ desktop server

A native C++17 / Qt 6 replacement for the Java PC application. It produces **JengaRemote.exe** on Windows and works with the **existing Jenga Remote Android app**. Java is not required on the PC after building.

This archive includes a **precompiled Windows x64 build launcher** (`JengaRemote-Start.exe`) and the buildable remote-server source. The final remote-control application still has to be compiled on Windows. The C++ application and tests were compiled and run on Linux in the development environment. The Windows build, native Windows input and an actual phone still need testing on your PC.

## First: make the Windows executable

Install these development tools once:

1. **Visual Studio 2022** or its Build Tools, with **Desktop development with C++**, the MSVC x64 compiler, Windows SDK, and C++ CMake tools.
2. **Qt 6, MSVC 2022 64-bit kit**, including **Qt WebSockets**. Qt 6.8.3 is the reference Windows kit in the supplied workflow; the CMake minimum is Qt 6.4. Select a current compatible Qt 6 kit if you prefer and test it. Do not choose the MinGW kit for this MSVC script.
3. **Git for Windows**.

Extract all files directly into your existing project folder, alongside the `msvc2022_64` Qt folder you copied there. Allow the updated build scripts to replace their previous versions. Do not create an extra nested project folder.

Double-click **JengaRemote-Start.exe**. It finds the copied Qt kit automatically, shows a visible build console, compiles/tests/deploys the server, and opens the finished app when successful. It checks for valid Qt files rather than assuming a folder name is correct. It also searches your original user Qt installation and C:\Qt. No Qt path needs to be typed.

When a deployed app already exists at `dist\JengaRemote\JengaRemote.exe`, the launcher opens it directly. To force a rebuild after source changes, run **BUILD-WINDOWS.cmd**, which always rebuilds and then launches. Both routes leave any Qt installation intact.

The script downloads the pinned vcpkg toolchain into `.tools`, builds the OpenSSL dependency, compiles, runs the tests and creates:

```text
dist\JengaRemote\JengaRemote.exe
dist\JengaRemote-Windows-x64.zip
```

**Keep the entire `dist\JengaRemote` folder together.** The EXE uses the Qt plugins/DLLs and OpenSSL DLLs beside it. This is a portable native application folder, not a single-file executable. You can copy that folder to another suitable Windows x64 PC without installing Java or Qt there. The MSVC runtime is gathered by the deployment tool; verify the packaged folder on a clean PC before sharing it.

PowerShell alternative:

```powershell
.\build-windows.ps1 -Launch
# Optional explicit path:
.\build-windows.ps1 -QtDir 'C:\Qt\6.8.3\msvc2022_64' -Launch
```

A ready-to-use GitHub Actions workflow is included in `.github/workflows/build-windows.yml`. Put the project contents at the root of your own repository and run **Build Windows app** in Actions to get its downloadable Windows artifact. No repository was created and no remote workflow was run for you.

## Connect your existing phone app

1. **Stop the Java PC server first**: both default to TCP 8765.
2. On the phone, forget the old Java server entry if replacing it. This C++ program has its own certificate and credentials, so old saved credentials cannot be transferred automatically.
3. Open `JengaRemote.exe`. Its server starts automatically when possible.
4. Open **Connection settings** and check that the advertised address is the PC's real Wi-Fi/LAN address. `127.0.0.1` is a fallback only and cannot be reached by another device. For internet access, enter the PC's existing private VPN address.
5. Click **Connect phone / Show QR**.
6. In the Android app: **Computers → Scan QR Code to Connect**.
7. Scan and approve the new phone on the PC. Keep the QR window open until pairing completes.
8. Use Touchpad or Keyboard. To view the desktop, enable **Screen viewing** on the PC, reconnect after the permission change, then open Desktop on the phone.

No Android rebuild is needed. Later connections use the saved certificate pin and long-term credential without rescanning. The phone app's existing foreground-only behavior and reconnect policy are unchanged.

## Features

- Custom high-DPI interface, sidebar, illustrated pairing card, connection/address/device cards and visible active-control badge.
- Custom monitor-and-phone Windows icon embedded in the EXE, with 16/24/32/48/64/128/256-pixel variants. A matching icon is used in the window/taskbar.
- TLS WebSocket server, 120-second single-use QR tokens, desktop approval, certificate fingerprint pinning compatible with Android, saved devices and revocation.
- Native Windows mouse, wheel, keyboard, shortcuts, direct UTF-16 text input and media keys. Linux X11 input through XTest; Linux text uses clipboard paste.
- JPEG desktop viewing, cursor overlay, monitor selection and touch coordinate mapping. Android controls its existing zoom/pan/drag viewer.
- Mouse/keyboard/media/screen permissions, screen viewing off by default, emergency disconnect, device rename/disconnect/revoke and local discovery for saved devices.
- Windows private key encrypted with user-account DPAPI. Linux private key protected by owner-only file permissions. Credential hashes persisted using atomic writes.

## Other build routes

**Qt Creator:** open `CMakeLists.txt` and select the MSVC x64 kit. Supply an OpenSSL 3 development installation via `OPENSSL_ROOT_DIR`, or use the vcpkg toolchain/manifest. A raw Qt Creator build does not automatically deploy DLLs; use `windeployqt` and copy the matching OpenSSL runtime DLLs before moving the app to another PC.

**Linux X11:** install a C++17 compiler, CMake 3.24+, Qt 6 Widgets/Network/WebSockets/Concurrent/Test development packages, OpenSSL 3 development files and X11/XTest development files. Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/JengaRemote
```

Run the actual controller from an X11 graphical session. Wayland input/capture is not implemented. Do not run the app as root or administrator.

## Runtime boundaries

Local connections need a reachable LAN and appropriate host firewall rules. Remote internet use requires an existing WireGuard/Tailscale private network; QR does not bypass NAT. The server listens on all interfaces, so scope firewall access to the intended LAN/VPN. It never configures public port forwarding.

Windows login/UAC secure desktops and Ctrl+Alt+Delete are unsupported. Some applications reject injected Unicode/media keys. Mixed-DPI monitor mapping, multi-display capture, keyboard layouts and real drag behavior need target-machine testing. Linux media keys depend on the X11 keymap and desktop media handling. No hardware video codec, tray autostart, installer, code signing or public relay is included.

Read **SECURITY.md**, **TESTING.md** and **TROUBLESHOOTING.md** before distributing the app.
