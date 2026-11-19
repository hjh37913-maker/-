# Experimental Windows and Android demos

These files are demonstrations, **not PS5 emulators or game launchers**. Neither platform was available for runtime or device testing in the Linux build sandbox. File format and signing checks do not guarantee installation or execution on a user's device.

## Windows x64

`NIGHTFALL-Windows-Demo.exe` is an unsigned native Windows GUI compiled from `tools/windows/demo.cpp` and the actual C++ machine/CPU/memory code. Its button runs an original synthetic program (MOV 40, ADD 2, synthetic HLT), expecting RAX=42. It does not install or launch games. A Windows x64 computer is required; Windows runtime behavior is **untested**.

`NIGHTFALL-Windows-Package.zip` additionally contains the compiled `nightfall_cli.exe`, the optional visual browser dashboard, two original test fixtures, and `START-DASHBOARD.cmd`. The dashboard needs Python 3 and `py` on Windows; the native GUI does not. The dashboard server is unauthenticated, loopback-only by default, and the interpreter is not a sandbox. Use only trusted original test files. Windows SmartScreen may warn because the binaries are not signed.

Cross-compile command used on Linux with Zig 0.13.0:

```sh
zig c++ -target x86_64-windows-gnu -std=c++20 -O2 -Isrc \
  tools/windows/demo.cpp src/cpu/cpu.cpp src/core/machine.cpp src/memory/memory.cpp \
  -Wl,--subsystem,windows -luser32 -lgdi32 -o NIGHTFALL-Windows-Demo.exe
```

The GUI also builds through CMake on Windows as the `nightfall_windows_demo` target. The cross-compiled GUI was inspected as PE x86-64 with the Windows GUI subsystem and only Windows system/UCRT DLL imports; its runtime was not tested.

## Android

`NIGHTFALL-Android-Demo.apk` is a **UI-only offline demo**, with no CPU emulator or game execution. It is a WebView shell around locally bundled dashboard assets. It can show the metadata library and browser input diagnostic; actual Android WebView behavior, file selection, source links and device inputs were not tested on a device/emulator. Original programs and games cannot be run in this version. Minimum SDK 23 (Android 6); target SDK 28, so newer Android versions may show a compatibility warning or restrict installation. It has no INTERNET permission. It is signed with a newly generated, non-production demo certificate, not a verified release identity, and cannot reliably be updated by future builds with a different signing key; uninstalling would remove app-local data. Android may warn when installing an APK from outside a store.

The signature was verified with `apksigner`, and `zipalign -c` and `aapt2 dump badging` succeeded. This verifies packaging/signature only, not runtime behavior.

Source is in `android/` and assets are derived from `web/` by `android/prepare_assets.py`, which marks Android as offline and disables test execution. Build with `android/build_demo.sh`, setting `JAVA_HOME`, `ANDROID_JAR` and `BUILD_TOOLS` to a JDK and Android SDK build-tools. It creates a debug key in its build output; **never publish it as a release key**. The build performed here used JRE 17 plus ECJ 3.4 (`ECJ_JAR`), Android platform 28, and Android build-tools 30.0.3, from temporary local tooling. Toolchains and signing keys are not included in the project.

## Validation

Linux Debug, Release and combined ASan/UBSan CTest: 51/51 in each configuration after adding the demo sources. The generated Android assets also passed a jsdom smoke test confirming the offline status and disabled execution; jsdom does not test a real WebView. No new CPU instruction capability, Windows runtime execution, Android installation, PS5 software, or GTA VI execution was verified.
