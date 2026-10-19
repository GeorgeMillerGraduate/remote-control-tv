# Third-party components

- Qt 6 Core/Gui/Widgets/Network/WebSockets/Concurrent/Test: Qt Project and contributors. Applicable LGPLv3/GPL/commercial terms vary by component and distribution. This project links Qt dynamically; retain the applicable licence notices and meet redistribution/relinking requirements when distributing binaries. See https://doc.qt.io/qt-6/licensing.html .
- OpenSSL 3: OpenSSL Project and contributors, Apache License 2.0. The Windows package script copies its vcpkg copyright file when available.
- QR-Code-generator 1.8.0 C++: Project Nayuki, MIT licence. The unmodified third_party/qrcodegen.cpp and .hpp include the complete licence notice.
- vcpkg: Microsoft and contributors, MIT. The manifest/toolchain baseline is pinned to 664933b8d385dfc01f4e0cdff4c7c74e71f6a740. Its source and build dependencies are downloaded only when building, not bundled into the source archive.
- The optional GitHub workflow uses official checkout/upload-artifact actions and aqtinstall 3.3.0 to obtain Qt. It has not been executed in this task.

The custom icon and connection illustration are drawn from the included branding.h C++ code. They do not depend on external stock images or fonts being embedded. The interface requests Segoe UI on Windows and uses a system fallback elsewhere.

No distribution licence is imposed here for newly written application source; choose an appropriate project licence before publishing. Preserve all applicable third-party licence and notice files when redistributing compiled packages.

The precompiled build launcher uses Zig/MinGW-w64 runtime support. Relevant distributed notices are in launcher/ZIG-LICENSE.txt and launcher/MINGW-LICENSE.txt. The Windows Universal CRT API-set imports resolve through the operating system on the target Windows 10/11 installation.
