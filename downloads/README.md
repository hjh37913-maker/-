# NIGHTFALL experimental demos

These are preview builds, not a working PS5 emulator or a game launcher. They do not run PS5 software or GTA VI.

- `nightfall-demo-windows-x64.exe`: unsigned Windows GUI cross-compiled on Linux. It executes an original synthetic CPU test; runtime on Windows was not tested.
- `nightfall-demo-android.apk`: Android 6+ offline UI demo only, without an emulator core. Debug-signed; installation and runtime on Android were not tested. Target SDK 28 may cause compatibility warnings. A later build signed with a different key cannot update this APK without uninstalling it first (which removes app-local data).

Both are experimental binaries. OS security prompts may appear. See [demo build details](../docs/DEMO_BUILDS.md) for limitations, verification and reproduction.

SHA-256:

```
52774bdd0c44a3d92fff49b048565b946739f4c457698617495f51be8b06387e  nightfall-demo-windows-x64.exe
9ed4c29836f29c775d45a38950e3e99aa8041dad9715bfe8a733df92b8824f68  nightfall-demo-android.apk
```
