#!/usr/bin/env sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    printf '%s\n' 'Run this installer as root (for example: sudo ./scripts/install-linux-systemd.sh).' >&2
    exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
    printf '%s\n' 'systemd is required, but systemctl was not found.' >&2
    exit 1
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(dirname -- "$script_dir")
source_binary=${1:-"$repo_root/build/linux/ssheila"}
service_source="$repo_root/packaging/linux/ssheila.service"
defaults_source="$repo_root/packaging/linux/ssheila.default"

if [ ! -f "$source_binary" ]; then
    printf 'Linux executable not found: %s\n' "$source_binary" >&2
    printf '%s\n' 'Build it with ./scripts/build-linux.sh, or pass its path to this installer.' >&2
    exit 1
fi

if [ ! -f "$service_source" ] || [ ! -f "$defaults_source" ]; then
    printf '%s\n' 'Linux packaging files are missing from packaging/linux.' >&2
    exit 1
fi

if ! getent group ssheila >/dev/null 2>&1; then
    groupadd --system ssheila
fi

if ! id ssheila >/dev/null 2>&1; then
    nologin_shell=/usr/sbin/nologin
    if [ ! -x "$nologin_shell" ]; then
        nologin_shell=/sbin/nologin
    fi
    if [ ! -x "$nologin_shell" ]; then
        nologin_shell=/bin/false
    fi

    useradd \
        --system \
        --gid ssheila \
        --home-dir /var/lib/ssheila \
        --shell "$nologin_shell" \
        --comment 'sSheila service account' \
        ssheila
fi

install -D -m 0755 "$source_binary" /usr/local/bin/ssheila
install -D -m 0644 "$service_source" /etc/systemd/system/ssheila.service

# Preserve administrator changes when the one-time installer is run again for an upgrade.
if [ ! -e /etc/default/ssheila ]; then
    install -D -m 0640 -o root -g ssheila "$defaults_source" /etc/default/ssheila
fi

systemctl daemon-reload
systemctl enable ssheila.service
systemctl restart ssheila.service

configured_port=18877
if [ -r /etc/default/ssheila ]; then
    # The defaults file contains shell-compatible assignments and is already
    # the service's supported administrator override mechanism.
    . /etc/default/ssheila
    configured_port=${SSHEILA_PORT:-$configured_port}
fi

attempt=0
service_state=activating
while [ "$attempt" -lt 10 ]; do
    service_state=$(systemctl is-active ssheila.service 2>/dev/null || true)
    if [ "$service_state" = active ]; then
        break
    fi
    if [ "$service_state" = failed ]; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done

if [ "$service_state" != active ]; then
    printf '%s\n' 'sSheila did not become active. Recent service output follows:' >&2
    journalctl -u ssheila.service -n 40 --no-pager >&2
    exit 1
fi

printf '%s\n' 'sSheila is installed, enabled at boot, and running as the ssheila system user.'
printf '%s\n' 'Workspace data: /var/lib/ssheila'
printf 'Open from the host subnet: http://sheila.local:%s\n' "$configured_port"
printf '%s\n' 'mDNS discovery uses UDP port 5353; allow it from the local subnet in the host firewall if one is enabled.'
printf '%s\n' 'Service status: systemctl status ssheila'
printf '%s\n' 'Live logs: journalctl -u ssheila -f'
