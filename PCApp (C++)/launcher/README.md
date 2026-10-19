# Native Windows build bootstrapper

JengaRemote-Start.exe is an unsigned x64 Windows GUI executable compiled from launcher.c. It uses only Windows system libraries and requires no Java, Python or Qt to start. The build it starts still requires the project development prerequisites.

It resolves its own directory, opens an existing deployed remote app if available, otherwise invokes the colocated build-windows.ps1 in a visible Windows PowerShell console. ExecutionPolicy Bypass is scoped only to that PowerShell process; no machine/user policy is changed. It never requests administrator elevation or installs the C++ compiler silently. The script auto-detects the valid Qt kit and invokes only the documented project build.

## Reproduce the included bootstrapper

With Zig 0.17.0 (the cross-compiler used here), from this launcher directory:

```sh
zig rc /fo launcher.res launcher.rc
zig cc -target x86_64-windows-gnu -Os -s -municode -Wl,--subsystem,windows launcher.c launcher.res -lshell32 -luser32 -o ../JengaRemote-Start.exe
```

The resource embeds the same multiresolution app icon and asInvoker/high-DPI manifest as the C++ application. The generated launcher.res and intermediate object files are not shipped. This bootstrapper was not run on Windows in the build environment; report any exact console or system error when testing it.
