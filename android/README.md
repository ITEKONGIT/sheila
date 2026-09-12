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

The Android app must not report the node as ready until the vault, storage,
network boundary, port-80 front door and endpoint self-check have all passed.

The target product address is always:

```text
http://sheila.local
```

The raw IP is a recovery display only. The product does not expose a visible
alternate port.
