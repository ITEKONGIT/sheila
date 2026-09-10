# sSheila architecture

## Local-first contract

sSheila has one authoritative host. Clients do not maintain independent replicas in the current milestone; they use a browser to interact with the host's durable workspace. This keeps ownership and recovery understandable while still making the workspace available across operating systems and device types.

The contract is:

- the executable owns orchestration and API behavior;
- the host filesystem owns file bodies;
- SQLite owns structured state and full-text indexing;
- browsers remain replaceable clients;
- restarting or upgrading the executable must not reset writable state.

## Implemented runtime

```text
Embedded HTML/CSS/JavaScript
             |
      HTTP + WebSocket
             |
      Drogon controllers
        |           |
   notes/items    events
        |           |
        +-----+-----+
              |
        portable C++ core
          |          |
       SQLite      filesystem
        + FTS5      objects
```

The web resources are converted into byte arrays during CMake configuration and linked into the application. The Windows release statically links non-system dependencies, so the deployable runtime is one executable.

## Storage layout

```text
data-root/
  ssheila.db
  objects/
  notes/
  previews/
  uploads/
  versions/
  exports/
  backups/
```

SQLite stores item metadata, note content, checksums, timestamps, and the FTS5 index. Uploaded file bodies are written into `objects/`. The remaining directories reserve stable boundaries for subsequent transfer, preview, revision, export, and recovery work.

## Network boundary

The server binds to `0.0.0.0` so other devices can connect. At startup, the engine discovers active host IPv4 interfaces and their masks. Pre-routing advice checks the socket peer address against those subnets; localhost and matching subnet addresses continue, while other sources receive `403 Forbidden`.

Forwarding headers are not trusted for this decision. Windows Firewall is separately restricted to the executable, TCP port `18877`, and `LocalSubnet` remote addresses.

This is defense in depth, not user identity. Authentication, pairing, authorization, and TLS must be added before treating an untrusted network as safe.

## Portability boundary

Storage, database access, controllers, embedded resources, and orchestration are portable C++20. Platform-specific responsibilities remain narrow:

- network-interface discovery;
- default data-directory selection;
- Windows startup/firewall integration;
- planned Windows Service and Linux systemd integration;
- planned platform key storage.

## Transfer semantics

Transfers are currently host-mediated:

1. Device A uploads a file to the host.
2. The host persists its body and metadata.
3. A WebSocket event tells connected clients to refresh.
4. Device B downloads the stored object.

This is neither peer-to-peer transport nor bidirectional filesystem synchronization. Resumable upload sessions, transfer state, and source cleanup remain future milestones.

