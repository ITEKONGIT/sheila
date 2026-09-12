# Sheila Android host

This directory is reserved for the Android Intelligence Node application.

The implementation starts on the `codex/android-intelligence-node` branch and
is intentionally staged behind [the Android host plan](../docs/android-intelligence-node-plan.md).

The first implementation block is:

1. Extract a start/stop-safe native `HostRuntime`.
2. Add the Android foreground-service shell.
3. Add the Keystore-backed vault gate.
4. Add the endpoint state model (`sheila.local`, IP fallback, blocked).
5. Prove real port-80 and remote self-check behavior on physical hardware.

The current baseline contains the first testable Android vertical slice:

- Gradle/Kotlin Android project scaffold.
- Compose Intelligence Node dashboard.
- Canonical hostname, IP fallback and blocked endpoint states.
- Android Keystore AES-GCM round-trip verification.
- Non-exported foreground-service shell.
- CMake/JNI native health seam with 16 KB linker alignment.
- ABI filters for `arm64-v8a`, `armeabi-v7a` and `x86_64`.

The native health seam intentionally reports `android-jni-baseline`; it does
not claim that the C++ HTTP server is running. The next integration block
connects `HostRuntime`, storage and the real port-80 self-check.

## Local build

Open this directory in Android Studio and let it install the pinned SDK,
NDK and CMake components. From a machine with the Android SDK configured:

```powershell
./gradlew :app:testDebugUnitTest
./gradlew :app:assembleDebug
```

The repository currently does not include a Gradle wrapper binary; Android
Studio can generate it, or the developer environment can run the matching
Gradle distribution directly. Do not commit `local.properties`, build
outputs, keystores or generated native artifacts.

The Android app must not report the node as ready until the vault, storage,
network boundary, port-80 front door and endpoint self-check have all passed.

The target product address is always:

```text
http://sheila.local
```

The raw IP is a recovery display only. The product does not expose a visible
alternate port.
