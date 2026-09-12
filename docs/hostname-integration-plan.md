# Host-owned `sheila.local` integration plan

## Scope and branch boundary

This work belongs only on `feature/expanded-file-upload-support`.

Do not edit, merge into, or deploy from `main` while this integration is being
developed. Do not push additional work to the public repository until it has
been reviewed and tested. The feature branch is public when pushed, so this
document and all future commits must contain no credentials, private network
details, or machine-specific data.

## Desired user behavior

When the host and a client are on the same reachable local network, the user
should be able to open:

```text
http://sheila.local
```

The name resolves to the hosting system's active IP address. The host owns the
advertisement; the router does not need to own a DNS record. When the host
leaves a network, the old address is no longer reachable. When the host joins a
new Wi-Fi network, it advertises the same name on the new interface.

This behavior applies to directly connected networks. Ordinary routers do not
forward `.local` mDNS traffic. A separate routed network requires an mDNS
reflector or DNS integration. Public internet access is a separate feature and
must use a real domain, routing/tunneling, TLS, and authentication.

## Implemented in the feature branch

The current integration commit provides:

- A built-in IPv4 mDNS responder for `sheila.local` on UDP `5353`.
- Advertisement using discovered host IPv4 addresses.
- Dynamic subnet access and mDNS reconfiguration.
- Linux netlink network-change monitoring.
- Windows native IPv4 address-change monitoring.
- Debounced network updates and a five-minute safety rescan.
- Graceful mDNS failure: the HTTP service can continue by IP/explicit port.
- Windows local-subnet firewall configuration for UDP `5353`.
- Installer, architecture, and README documentation.
- A deterministic mDNS validation test.

The current backend still listens on TCP `18877`. The hostname currently
works as `http://sheila.local:18877`; the no-port URL is not implemented yet.

## Remaining implementation: host-owned port-80 front door

DNS/mDNS only maps a hostname to an IP. A browser entering
`http://sheila.local` uses TCP port `80`, so the host must provide a local
TCP-80 front door that forwards to the stable backend on `18877`.

Keep the backend port stable and make the front door optional:

```text
client → sheila.local:80 → host-owned redirect → 127.0.0.1:18877
```

The implementation must:

1. Detect whether TCP port 80 is already occupied.
2. Never stop sSheila or overwrite an unrelated port-80 service.
3. Install an idempotent forwarding rule owned by sSheila.
4. Preserve the client's source address so the subnet access gate remains
   effective. Do not make the backend trust arbitrary forwarding headers.
5. Verify both the backend (`18877`) and front door (`80`) independently.
6. Keep `http://sheila.local:18877` as a documented fallback.
7. Remove only sSheila-owned forwarding and firewall rules during uninstall.

### Windows

Use the Windows host's TCP forwarding facility (for example, an explicit
`netsh interface portproxy` mapping) and a separate TCP-80 firewall rule
restricted to `LocalSubnet`. The installer must verify the exact mapping after
creation, handle an occupied port, and avoid treating a front-door failure as
a backend failure.

### Linux

Use a host-owned, persistent TCP redirect from port `80` to `18877` through the
distribution's supported firewall layer (prefer nftables, with a controlled
iptables fallback where appropriate). The installer must use an sSheila-owned
chain/rule, verify it, and remove only that chain/rule. If the host cannot
provide a safe redirect, leave the backend running and report the explicit
port fallback instead of failing the service installation.

## Fault-tolerance requirements

- Initial interface discovery failure: keep localhost available, deny remote
  clients until a valid subnet snapshot is available, and retry discovery.
- Network-change notification failure: retain the slow safety rescan.
- Transient discovery failure: preserve the last valid subnet/mDNS state.
- mDNS port conflict: keep HTTP available and report the fallback URL.
- Port-80 conflict: preserve the unrelated service and report
  `:18877` fallback.
- Backend restart: leave the front-door configuration intact.
- Host network transition: debounce events, refresh interfaces once, and
  eventually expire old mDNS records.
- Shutdown: stop the network watcher before destroying the mDNS responder and
  ensure all watcher threads join.
- Never use router port forwarding automatically.

## Testing and rollout gate

Before publication or merging to `main`:

1. Run core-only and full Linux builds/tests.
2. Run a native Windows build and test suite.
3. Validate PowerShell and shell installer syntax.
4. Test two clients on one Wi-Fi network using `http://sheila.local`.
5. Move the host to a second Wi-Fi and confirm the same hostname resolves.
6. Disconnect the host and confirm the old network stops working.
7. Test a client moving away while the host remains behind.
8. Test multiple active interfaces and port-80 conflicts.
9. Stop/restart both service managers and verify recovery.
10. Inspect the final diff for secrets and confirm `main` is unchanged.

Only after Linux and Windows pass this matrix should the feature branch be
reviewed for merge into `main`. The main-only release workflow remains the
only deployment path.

