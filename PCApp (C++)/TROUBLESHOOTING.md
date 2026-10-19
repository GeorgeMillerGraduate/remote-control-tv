# Troubleshooting

**Which file do I open?** First: `JengaRemote-Start.exe`. To rebuild: `BUILD-WINDOWS.cmd`. After a successful build: `dist\JengaRemote\JengaRemote.exe`. Do not move only the EXE out of its folder.

**No Qt installation / wrong kit:** install Qt 6 with its MSVC 2022 64-bit kit and Qt WebSockets module. The updated launcher detects a kit beside the project, under your user Qt folder, or under C:\Qt. An explicit QtDir must be the kit directory, not the Qt installation root or Qt Creator folder. MinGW cannot link the MSVC OpenSSL libraries used by the supplied script.

**No C++ compiler:** install Visual Studio 2022 C++ Desktop development or equivalent Build Tools, including Windows SDK and CMake. NetBeans' Java compiler does not compile C++.

**The first build takes time:** vcpkg builds OpenSSL and downloads its required tools. This happens on the development machine; other PCs use the already deployed runtime folder.

**TLS or DLL error:** use the deployed `dist` folder, not a copied raw build EXE. Confirm `tls` plugins and matching OpenSSL DLLs are included. Rerun the deployment script and inspect its first failure. The server has no plaintext fallback.

**Port already in use:** stop the Java server or another instance. Both versions default to TCP 8765. A different port can be set under Connection settings before starting.

**QR scans but phone cannot connect:** verify the advertised LAN IP, matching Wi-Fi/VPN, firewall and PC server status. 127.0.0.1 is local to each device and never the address to advertise to a phone. Guest Wi-Fi may block peer access. Select or type the correct address, then refresh the QR.

**Old saved PC refuses connection:** the C++ version has a new certificate and credential store. Forget the old Java entry and pair again. Keep pin verification enabled. An expired server certificate or incorrect phone date/time can also fail TLS validation.

**Screen stopped after enabling its permission:** changing PC permissions disconnects the current session. Reconnect, then open Desktop. Linux Wayland and Windows secure desktops are unsupported.

**Windows blocks injected input:** use ordinary apps at the same privilege level as the server. Do not run as administrator to bypass secure-desktop boundaries. Ctrl+Alt+Delete cannot be synthesized here.

**Windows SmartScreen / unsigned app:** this project does not include a code-signing certificate. A locally built unsigned application is not equivalent to a signed commercial installer. Review the build/source and any displayed publisher information before deciding how to run or distribute it.

**Internet use:** configure your own private WireGuard/Tailscale network first. QR pairing is not a relay and does not open routers or firewall ports. Discovery is optional UDP broadcast; the Android app searches only its existing saved PCs on default port 8765.
