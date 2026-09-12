#!/usr/bin/env python3
"""Create the small public manifest consumed by the release download page."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


def artifact(path: Path, url: str) -> dict[str, object]:
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return {
        "name": path.name,
        "url": url,
        "sha256": digest,
        "size_bytes": path.stat().st_size,
        "signature_url": f"{url}.sig",
        "certificate_url": f"{url}.pem",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-dir", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--github-release-url", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    base_url = args.base_url.rstrip("/")
    windows = next(args.release_dir.glob(f"ssheila-{args.tag}-windows-x64.exe"), None)
    linux = next(args.release_dir.glob(f"ssheila-{args.tag}-linux-x64.tar.gz"), None)
    if windows is None or linux is None:
        raise SystemExit("Both Windows and Linux release artifacts are required")

    manifest = {
        "schema_version": 1,
        "tag": args.tag,
        "version": args.tag.removeprefix("v"),
        "published_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "github_release_url": args.github_release_url,
        "checksums_url": f"{base_url}/latest/SHA256SUMS",
        "checksums_signature_url": f"{base_url}/latest/SHA256SUMS.sig",
        "manifest_signature_url": f"{base_url}/latest.json.sig",
        "manifest_certificate_url": f"{base_url}/latest.json.pem",
        "platforms": {
            "windows-x64": artifact(
                windows,
                f"{base_url}/latest/windows/sSheila.exe",
            ),
            "linux-x64": artifact(
                linux,
                f"{base_url}/latest/linux/sSheila.tar.gz",
            ),
        },
    }

    args.output.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
