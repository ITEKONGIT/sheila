# sSheila

> **Your stay-at-home wife for files and notes — always home, never in the cloud, and somehow knows exactly where you left everything.**

sSheila is a local-first, cross-device workspace written in C++20. One computer hosts the engine and owns the storage; every trusted phone, tablet, or computer on the same subnet reaches it through a browser.

Upload something from one device, retrieve it from another, or leave yourself a note and continue later. The host remains the source of truth, so the workspace does not depend on a cloud account or somebody else's storage quota.

## What works today

- One self-contained Windows executable with the interface and runtime libraries embedded
- Host-backed file upload and download
- Persistent note creation and editing
- Markdown document editor with preview, formatting shortcuts, local draft recovery, and word counts
- Full-text search powered by SQLite FTS5
- A universal inbox with file and note filters
- WebSocket refresh when another device changes the workspace
- Automatic storage initialization on first run
- Resume from the same database and object store after restart
- Host-subnet access enforcement before HTTP routing
- Windows autostart and restart-after-failure tooling
- Linux `systemd` service with boot startup, restart-on-failure, and process isolation
- Shared C++ core designed to compile on Windows and Linux

## Local-first architecture

Local-first is the ownership model, not merely an offline mode.

1. **The host owns the truth.** File bodies live on its filesystem. SQLite stores notes, metadata, relationships, and the search index.
2. **Browsers are clients, not storage silos.** Every device opens the same workspace over HTTP and sees the same durable state.
3. **The engine is self-contained.** Drogon, Trantor, SQLite/FTS5, JSON support, compression, and the web application are compiled into `sSheila.exe`.
4. **Writable data stays outside the executable.** Replacing or upgrading the binary does not replace the user's library.
5. **The LAN is the current boundary.** The server listens on the host network, while a pre-routing gate rejects addresses outside the host's active IPv4 subnets.

```text
phone / tablet / laptop
          |
          |  same-subnet HTTP + WebSocket
          v
   +------------------+
   |   sSheila.exe    |
   |------------------|
   | embedded web UI  |
   | Drogon API       |
   | orchestration    |
   | SQLite + FTS5    |
   +------------------+
          |
          v
 host filesystem: objects, uploads, notes, backups, metadata
```

Nothing is uploaded to a third-party service by sSheila. If the host is powered off or asleep, the workspace is unavailable; when the host and service return, clients continue from the persisted state.

## Trust boundary

The current release is intended for a trusted private subnet. Every device admitted by the subnet gate can currently see the shared workspace, upload and download files, and create or edit notes.

Authentication, device pairing, per-device permissions, and TLS are not implemented yet. Do not expose port `18877` through router port forwarding, and do not use the current release on an untrusted shared network.

## Run the Windows executable

Run `sSheila.exe`, then open:

```text
http://127.0.0.1:18877
```

Other devices use the LAN address printed at startup, for example:

```text
http://192.168.0.3:18877
```

Without arguments, Windows data is created under `%LOCALAPPDATA%\sSheila`. A different durable location can be selected explicitly:

```powershell
./sSheila.exe --data-dir "D:\sSheila-data"
```

Runtime options:

```text
--address <address>  Bind address (default: 0.0.0.0)
--port <port>        HTTP port (default: 18877)
--data-dir <path>    Host storage root
```

The current upload ceiling is 32 MiB per request.

## One-time Windows autostart

After building the release artifact, run this once from an Administrator PowerShell window:

```powershell
./scripts/install-windows-autostart.ps1
```

This registers sSheila at Windows startup, restarts it after failure, preserves the existing project `data/` directory, and creates a firewall rule limited to Windows' `LocalSubnet` scope.

Remove only the startup registration later with:

```powershell
./scripts/uninstall-windows-autostart.ps1
```

Stored notes and files are not deleted by the uninstaller.

## Build the single Windows executable

Requirements:

- CMake 3.24+
- Ninja
- MinGW-w64 with C++20 support
- vcpkg with `VCPKG_ROOT` set

```powershell
$env:VCPKG_MAX_CONCURRENCY = "2"
cmake --preset windows-single -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset windows-single
ctest --test-dir build/windows-single --output-on-failure
```

The Windows single-file build intentionally compiles Trantor without TLS because this milestone serves only the directly connected private subnet. The resulting executable still imports standard Windows system libraries, but requires no companion application DLLs or external web-assets directory.

## Build on Linux

```bash
sudo apt install cmake ninja-build g++ libdrogon-dev libjsoncpp-dev libsqlite3-dev \
  libpq-dev default-libmysqlclient-dev libbrotli-dev libhiredis-dev libyaml-cpp-dev
./scripts/build-linux.sh
```

The script configures the same C++20/Drogon engine, builds it, and runs the test suite. It uses distribution packages by default, or vcpkg when `VCPKG_ROOT` points to a bootstrapped checkout.

## One-time Linux system service install

On a systemd-based distribution, build sSheila and run the installer once:

```bash
./scripts/build-linux.sh
sudo ./scripts/install-linux-systemd.sh
```

The installer copies the executable to `/usr/local/bin/ssheila`, creates an unprivileged `ssheila` system account, and starts `ssheila.service`. It also enables the service at boot and configures restart-on-failure, so no terminal or signed-in desktop session needs to remain open.

Persistent files, notes, metadata, and the database live under `/var/lib/ssheila`. Runtime overrides can be placed in `/etc/default/ssheila`; re-running the installer upgrades the executable without overwriting those settings or the workspace.

Useful service commands:

```bash
systemctl status ssheila
journalctl -u ssheila -f
sudo systemctl restart ssheila
```

After installation, browse to `http://<linux-host-ip>:18877` from a device on the same subnet. The application-level subnet gate remains active. Linux firewalls differ by distribution, so allow TCP port `18877` only from the local subnet when the host firewall is enabled.

To remove the service and executable while preserving all data:

```bash
sudo ./scripts/uninstall-linux-systemd.sh
```

## API milestone

| Method | Route | Purpose |
|---|---|---|
| `GET` | `/api/v1/health` | Service health |
| `GET` | `/api/v1/system` | Host platform and storage capacity |
| `GET` | `/api/v1/items?q=...` | Inbox listing and full-text search |
| `POST` | `/api/v1/notes` | Create a note |
| `PUT` | `/api/v1/notes/{id}` | Update a note |
| `POST` | `/api/v1/files` | Upload multipart files |
| `GET` | `/api/v1/files/{id}` | Download a stored file |
| `WS` | `/api/v1/events` | Live workspace-change events |

## Next milestones

1. Authentication and device pairing
2. Stable `ssheila.local` discovery
3. Resumable and streamed large-file transfers
4. Rich-text collaboration and revision history
5. Tags, folders, previews, and transfer history
6. Native Windows Service and Linux systemd commands inside the executable (external service installers now available)
7. Backup, recovery, and signed release artifacts
