#!/usr/bin/env sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    printf '%s\n' 'Run this uninstaller as root (for example: sudo ./scripts/uninstall-linux-systemd.sh).' >&2
    exit 1
fi

systemctl disable --now ssheila.service 2>/dev/null || true
rm -f /etc/systemd/system/ssheila.service
rm -f /usr/local/bin/ssheila
systemctl daemon-reload

printf '%s\n' 'The sSheila service and executable were removed.'
printf '%s\n' 'Workspace data and configuration were preserved in /var/lib/ssheila and /etc/default/ssheila.'
