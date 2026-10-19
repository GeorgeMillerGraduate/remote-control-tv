# Building and running

## PC — Windows or Linux X11

Install a full JDK 21 and Maven 3.9+. NetBeans can supply its bundled Maven. Open the directory containing pom.xml, select JDK 21, and Clean and Build.

From a terminal in DesktopRemoteServer:

```sh
mvn clean verify
java -jar target/remote-server-0.1.0.jar
```

The shade plugin creates a runnable JAR with dependencies. Run it as your ordinary desktop user, in the graphical session you want to control. Do not run as administrator/root. Initial launch invokes the JDK keytool executable to generate the local TLS identity. A JRE without keytool is insufficient for first launch.

On Linux use an X11 session. For media controls install `playerctl` and the package providing `pactl` using your distribution's package manager. Missing helpers produce an error instead of pretending the action worked.

On Windows allow Java's inbound TCP port 8765 only on the network profiles/interfaces you intend to use. UDP 8765 is optional for discovery. Restrict a VPN rule to the VPN interface/subnet. The program does not change firewall rules.

## Android

Use Android Studio with Android SDK platform 35, build tools, and JDK 17 or a compatible Studio bundled JDK. AGP 8.7.3 and Gradle 8.9 are pinned. The supplied Gradle wrapper downloads its distribution and verifies the supplied SHA-256 checksum. First sync/build requires internet access.

1. Open AndroidRemoteClient (the directory containing settings.gradle).
2. Let Gradle sync and Android Studio install missing SDK components.
3. Connect your phone with USB debugging enabled.
4. Run the app module and grant camera access when scanning.

Command line, with SDK configured in ANDROID_HOME or local.properties:

```sh
./gradlew assembleDebug
```

Windows:

```bat
gradlew.bat assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`. This is a development APK, not a signed Play Store release. Never commit local.properties, signing keys or runtime identity files.

Android native screens use XML activity roots and Java-generated controls. No Kotlin source is required; OkHttp's own implementation brings Kotlin runtime dependencies.

## Dependencies

PC: Java-WebSocket 1.6.0, Gson 2.11.0, ZXing core 3.5.3, FlatLaf 3.5.4, JNA 5.16.0, SLF4J nop 2.0.16. Tests: JUnit Jupiter 5.11.4. Maven plugins are pinned in pom.xml where explicitly configured.

Android: AppCompat 1.7.0, CameraX 1.4.1, bundled ML Kit barcode scanning 17.3.0 and OkHttp 4.12.0. Bundled scanning avoids an initial model download dependency. Minimum API 26; compile/target API 35. The pinned toolchain is a compatibility baseline rather than a claim to use the newest dependencies.
