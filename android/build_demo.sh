#!/bin/sh
# Build an offline UI demo APK. No emulator core is included.
# Requires Java, Android SDK build-tools, android.jar, Python 3, and zip.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${JAVA_HOME:?Set JAVA_HOME to a JDK (or JRE with ECJ_JAR)}"
: "${ANDROID_JAR:?Set ANDROID_JAR to an Android platform android.jar}"
: "${BUILD_TOOLS:?Set BUILD_TOOLS to an Android SDK build-tools directory}"
OUT=${1:-"$ROOT/build-android-demo"}
mkdir -p "$OUT"; OUT=$(CDPATH= cd -- "$OUT" && pwd)
mkdir -p "$OUT/assets" "$OUT/classes" "$OUT/dex"
python3 "$ROOT/android/prepare_assets.py" "$OUT/assets"
"$BUILD_TOOLS/aapt2" link -I "$ANDROID_JAR" --manifest "$ROOT/android/app/src/main/AndroidManifest.xml" -A "$OUT/assets" -o "$OUT/unsigned.apk"
SOURCE="$ROOT/android/app/src/main/java/com/nightfall/demo/MainActivity.java"
if [ -n "${ECJ_JAR:-}" ]; then
  "$JAVA_HOME/bin/java" -jar "$ECJ_JAR" -source 1.6 -target 1.6 -bootclasspath "$ANDROID_JAR" -d "$OUT/classes" "$SOURCE"
else
  "$JAVA_HOME/bin/javac" -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -d "$OUT/classes" "$SOURCE"
fi
"$JAVA_HOME/bin/java" -cp "$BUILD_TOOLS/lib/d8.jar" com.android.tools.r8.D8 --lib "$ANDROID_JAR" --min-api 23 --output "$OUT/dex" "$OUT"/classes/com/nightfall/demo/*.class
(cd "$OUT/dex" && zip -q "$OUT/unsigned.apk" classes.dex)
"$BUILD_TOOLS/zipalign" -f 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"
# A temporary, non-production debug certificate: do not use to publish a release.
if [ ! -f "$OUT/debug-key.jks" ]; then
  "$JAVA_HOME/bin/keytool" -genkeypair -keystore "$OUT/debug-key.jks" -storepass android -keypass android -alias demo -dname 'CN=Nightfall Debug Demo,O=Nightfall,C=US' -keyalg RSA -keysize 2048 -validity 10000
fi
"$JAVA_HOME/bin/java" -jar "$BUILD_TOOLS/lib/apksigner.jar" sign --ks "$OUT/debug-key.jks" --ks-key-alias demo --ks-pass pass:android --key-pass pass:android --out "$OUT/NIGHTFALL-Android-Demo.apk" "$OUT/aligned.apk"
"$JAVA_HOME/bin/java" -jar "$BUILD_TOOLS/lib/apksigner.jar" verify --verbose "$OUT/NIGHTFALL-Android-Demo.apk"
echo "Created $OUT/NIGHTFALL-Android-Demo.apk"
