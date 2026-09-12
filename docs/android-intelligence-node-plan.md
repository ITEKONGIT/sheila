# Android Intelligence Node

Status: implementation baseline for `codex/android-intelligence-node`.

This branch is the starting point for an Android host version of sSheila. The
phone is an authoritative local node, not a replica: it owns the SQLite
workspace and object store and serves the existing browser client over the
same Wi-Fi network or a phone-created hotspot.

## Product contract

- The node presents itself as **Sheila Intelligence Node**.
- The canonical address is always `http://sheila.local`.
- The raw IP is shown only when mDNS/hostname discovery fails.
- Neither hostname nor IP fallback displays a port.
- The Android host must therefore pass a port-80 capability and reachability
  check before it can report Ready.
- A port-80 conflict or denial is a blocking compatibility fault; it must not
  silently become `:18877`.
- The existing `18877` backend may remain internal behind a mobile front door
  while the desktop port-80 integration is completed separately.
- If two legitimate nodes compete for `sheila.local`, one owns the canonical
  name and the other reports its IP fallback. Device-specific alternate
  hostnames are not part of the product contract.

## First security layer

The first mobile release is explicitly activated, encrypted-at-rest and
trusted-LAN-only. Authentication and authorization are later layers, not a
claim that the first release is safe on an untrusted network.

When the node is stopped, the listener, WebSocket connections, database and
runtime decryption keys are closed or cleared. When the owner taps Start, the
service unlocks the vault, verifies storage, opens the database, starts the
port-80 front door and advertises `sheila.local`.

Initial storage rules:

- Keep the database and objects in private Android app storage.
- Do not request broad filesystem access.
- Use Android Keystore for the Sheila master key.
- Use envelope encryption for object bodies with authenticated encryption.
- Set `allowBackup=false` until encrypted export/import is implemented.
- Do not place keys in logs, notifications, URLs, QR metadata or APK resources.
- Treat a rooted or compromised phone as outside the protection boundary.

This layer protects data at rest and prevents access while the node is stopped.
Until authentication exists, any device that can reach the approved local
subnet can still call the running site.

## Initial implementation block

Block A is deliberately small and establishes the boundaries needed for every
later slice. It does not add authentication yet.

### A1. Extract a reusable native runtime

Create `HostRuntime` from the process-only orchestration currently in
`src/main.cpp`:

```cpp
struct RuntimeConfig {
    std::filesystem::path dataRoot;
    std::string bindAddress;
    std::uint16_t backendPort;
    std::uint16_t frontDoorPort;
    std::vector<Network> allowedNetworks;
};

class HostRuntime {
public:
    void start(RuntimeConfig config);
    void updateNetworks(std::vector<Network> networks);
    void stop();
    [[nodiscard]] HostStatus status() const;
};
```

`main.cpp` becomes a desktop adapter. Android will call the same runtime via
JNI. The runtime must be start/stop safe, must install the subnet gate before
opening listeners, and must expose liveness separately from readiness.

### A2. Define the Android endpoint state

The Android UI, notification, QR generator and system API must consume one
endpoint state rather than composing URLs independently:

```text
STARTING
READY_CANONICAL  -> http://sheila.local
READY_IP_FALLBACK -> http://<approved-lan-ip>
BLOCKED           -> compatibility or storage fault
STOPPED
```

The endpoint state owns the user-facing copy. No UI path may append a port.

### A3. Add the secure-vault gateway

Implement the Android-side lifecycle boundary:

```text
initializeSecureVault()
unlockVaultForNode()
lockVault()
clearRuntimeSecrets()
isVaultReady()
```

Native startup is refused if the vault cannot unlock or the database integrity
check fails. Keystore invalidation enters recovery; it never deletes data.

### A4. Add the Android service shell

Create a non-exported foreground service with a visible notification:

```text
startNode()
stopNode()
recoverAfterProcessDeath()
onNetworkAvailable()
onNetworkLost()
```

This block may report a simulated or native health status while the full
Android build is being assembled, but it must not claim Ready until the real
port-80 and local-network checks pass.

### Block A exit criteria

- Desktop build behavior remains unchanged.
- Android can start and stop the runtime without process exit.
- A failed vault prevents listener startup.
- Port-80 bind and remote self-check are real, not simulated.
- Stopped state closes listeners and WebSockets.
- The UI consistently shows `sheila.local`, IP fallback or a blocking fault.
- No plaintext key or database is written to shared storage.
- Every failure returns a typed, user-safe error.

## Slice threat model

Every slice must document assets, trust boundaries, threats, error points,
failure behavior and tests before it is merged.

### Slice 0 — Native and device compatibility

Boundary: source/dependencies/APK to Android runtime.

Threats: dependency substitution, debug build leakage, missing ABI, 16 KB
page-size crash, false port probe, malicious native library.

Error points: `ABI_UNSUPPORTED`, `PAGE_SIZE_UNSUPPORTED`,
`PORT_80_UNAVAILABLE`, `MULTICAST_UNAVAILABLE`, `NATIVE_LOAD_FAILED`.

Controls: pinned dependencies, release signing, ABI/page-size validation,
actual bind-and-request probes, no secrets in build output.

### Slice 1 — Runtime lifecycle

Boundary: Activity/service/JNI to C++ runtime.

Threats: forged control Intent, double start, stop/start race, stale callback,
listener opening before the gate, native exception.

Error points: `ALREADY_RUNNING`, `PARTIAL_INITIALIZATION`, `STALE_CALLBACK`,
`SHUTDOWN_TIMEOUT`, `NATIVE_EXCEPTION`.

Controls: non-exported service, explicit state machine, callback generation
IDs, security initialization before listeners, JNI exception conversion and
thread-join verification.

### Slice 2 — Storage and transfers

Boundary: HTTP bytes to temporary files, SQLite and objects.

Threats: memory/disk exhaustion, path traversal, symlinks, overlapping chunks,
wrong-device resume, hash mismatch, partial database/object commit.

Error points: `UPLOAD_TOO_LARGE`, `INVALID_OFFSET`, `TRANSFER_NOT_OWNED`,
`HASH_MISMATCH`, `INSUFFICIENT_STORAGE`, `ATOMIC_COMMIT_FAILED`.

Controls: streaming, quotas, random paths, canonical containment checks,
device-bound transfers, SHA-256, fsync, atomic commit and recovery journal.

### Slice 3 — Android service and network lifecycle

Boundary: Android OS network/lifecycle events to active listeners.

Threats: unauthorized service control, process death during write, stale
subnet authorization, battery abuse, cellular/VPN exposure.

Error points: `SERVICE_START_TIMEOUT`, `NETWORK_PERMISSION_REVOKED`,
`PROCESS_RESTARTED`, `BATTERY_RESTRICTION_DETECTED`.

Controls: foreground notification, explicit user start, Wi-Fi/hotspot interface
allowlist, no permanent wake lock, journaled recovery and permission rechecks.

### Slice 4 — Portless front door

Boundary: client port 80 to internal backend.

Threats: port squatting, false readiness, proxy identity loss, auth bypass,
redirect loops and conflicting listeners.

Error points: `PORT_80_IN_USE`, `PORT_80_PERMISSION_DENIED`,
`FRONT_DOOR_BIND_FAILED`, `BACKEND_UNREACHABLE`, `SELF_CHECK_FAILED`.

Controls: exact ownership checks, preserve source address, shared middleware,
remote self-check and blocking failure when port 80 cannot be provided.

### Slice 5 — mDNS and `sheila.local`

Boundary: multicast discovery to endpoint selection.

Threats: fake hostname owner, stale records, duplicate nodes, secret leakage,
multicast lock abuse and advertisement on the wrong interface.

Error points: `HOSTNAME_CONFLICT`, `HOSTNAME_POINTS_ELSEWHERE`,
`ADVERTISEMENT_FAILED`, `STALE_RECORD`.

Controls: node identity verification, one canonical owner, IP fallback,
public-only TXT data, withdrawal on network changes and Wi-Fi/hotspot-only
advertising.

### Slice 6 — Pairing

Boundary: owner-approved QR challenge to device credential.

Threats: replay, brute force, QR theft, fake node, race, token leakage and
session fixation.

Error points: `PAIRING_CLOSED`, `PAIRING_EXPIRED`, `PAIRING_ALREADY_USED`,
`ATTEMPT_LIMIT_REACHED`, `OWNER_REJECTED`.

Controls: high-entropy one-time secret, URL fragment, short expiry, atomic
consumption, hashed token storage, phone confirmation, rate limiting and
immediate revocation.

### Slice 7 — API and WebSocket authorization

Boundary: LAN client to application operations.

Threats: unauthenticated mutation, IDOR, CSRF, DNS rebinding, WebSocket
hijacking, token theft, role escalation and brute-force DoS.

Error points: `MISSING_SESSION`, `SESSION_REVOKED`, `INVALID_ORIGIN`,
`INVALID_HOST`, `CSRF_FAILED`, `FORBIDDEN_ACTION`.

Controls: global middleware, strict Host/Origin checks, no wildcard CORS,
HttpOnly/SameSite cookies, CSRF tokens, role checks, rate limits and audit.

### Slice 8 — Uploaded content

Boundary: untrusted file bytes to browser preview/download.

Threats: stored XSS, SVG/HTML execution, MIME confusion, archive bombs,
parser exploits and executable installation.

Error points: `ACTIVE_CONTENT_BLOCKED`, `UNSAFE_FILENAME`,
`PREVIEW_TIMEOUT`, `ARCHIVE_LIMIT_EXCEEDED`.

Controls: attachment disposition, `nosniff`, no automatic extraction or
execution, sandboxed previews, strict CSP and parser resource limits.

### Slice 9 — At-rest cryptography

Boundary: Android storage/SQLite/object files to Keystore.

Threats: key extraction, hardcoded keys, nonce reuse, plaintext temporary
files, ciphertext corruption, invalidated keys and backup mismatch.

Error points: `KEYSTORE_UNAVAILABLE`, `KEY_INVALIDATED`,
`AUTHENTICATION_TAG_FAILED`, `RECOVERY_KEY_REQUIRED`.

Controls: Keystore-backed master key, per-object keys, AES-GCM, unique nonces,
encrypted recovery export and no silent deletion after key failure.

### Slice 10 — Transport security

Boundary: LAN traffic to HTTP/TLS.

Threats: packet capture, MITM, fake certificate and HTTP downgrade.

Error points: `TLS_PIN_MISMATCH`, `TLS_HANDSHAKE_FAILED`,
`SECURE_MODE_DOWNGRADE_BLOCKED`.

Controls: trusted-LAN mode explicitly labelled unencrypted; hardened mode uses
TLS/WSS, identity pinning and no automatic downgrade.

### Slice 11 — Recovery and restore

Boundary: backup/import to staging and live workspace.

Threats: backup tampering, path traversal, rollback, decompression bombs and
partial restore.

Error points: `BACKUP_SIGNATURE_INVALID`, `RESTORE_PATH_INVALID`,
`DATABASE_CORRUPT`, `RESTORE_VALIDATION_FAILED`.

Controls: encrypted/signed manifests, staging restore, canonical-path checks,
size limits, integrity validation and atomic cutover with rollback.

### Slice 12 — Build and release

Boundary: repository/build pipeline to signed APK/AAB.

Threats: supply-chain substitution, signing-key theft, debug configuration,
missing ABI, malicious downgrade and migration incompatibility.

Error points: `DEPENDENCY_HASH_MISMATCH`, `DEBUG_RELEASE_DETECTED`,
`ABI_MISSING`, `SIGNATURE_INVALID`, `VERSION_ROLLBACK`.

Controls: lockfiles/checksums, SBOM, native scans, signing continuity, secret
scanning, ABI/page-size checks and upgrade/rollback testing.

## Error contract

Every error returned to a client has a safe code, safe message, retryability
and opaque correlation ID:

```json
{
  "error": "PORT_80_UNAVAILABLE",
  "message": "This device cannot provide the Sheila local address.",
  "retryable": false,
  "correlationId": "opaque-id"
}
```

HTTP responses must not expose tokens, keys, SQL, stack traces, full paths or
raw native exceptions. Security-relevant failures are audited without storing
secrets.

## Visual development and integration

Use Jetpack Compose and Android Studio as the production UI loop:

1. Codex Prototype can be used to compare interaction directions before code.
2. Compose `@Preview` renders canonical, fallback, blocked, pairing, storage
   and transfer states across screen sizes, font scales and themes.
3. The Android Emulator verifies navigation and permission flows.
4. Physical phones verify mDNS, hotspot hosting, port 80, screen lock, Doze and
   OEM battery behavior.
5. Compose screenshot tests protect approved visual states.

The endpoint state model is the visual integration seam: every surface must
render the same canonical hostname, IP fallback or blocking fault.

