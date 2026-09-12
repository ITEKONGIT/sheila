#!/usr/bin/env python3
"""Exit successfully when the candidate release is not older than current."""

from __future__ import annotations

import re
import sys


TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$")


def parse(tag: str) -> tuple[tuple[int, int, int], tuple[str, ...] | None]:
    match = TAG_RE.fullmatch(tag)
    if not match:
        raise ValueError(f"invalid release tag: {tag}")
    core = tuple(int(match.group(index)) for index in range(1, 4))
    prerelease = tuple(match.group(4).split(".")) if match.group(4) else None
    if prerelease and any(not part for part in prerelease):
        raise ValueError(f"invalid prerelease tag: {tag}")
    return core, prerelease


def compare(left: str, right: str) -> int:
    left_core, left_pre = parse(left)
    right_core, right_pre = parse(right)
    if left_core != right_core:
        return (left_core > right_core) - (left_core < right_core)
    if left_pre is None or right_pre is None:
        return (left_pre is None) - (right_pre is None)
    for left_part, right_part in zip(left_pre, right_pre):
        if left_part == right_part:
            continue
        if left_part.isdigit() and right_part.isdigit():
            return (int(left_part) > int(right_part)) - (int(left_part) < int(right_part))
        if left_part.isdigit() != right_part.isdigit():
            return -1 if left_part.isdigit() else 1
        return (left_part > right_part) - (left_part < right_part)
    return (len(left_pre) > len(right_pre)) - (len(left_pre) < len(right_pre))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: compare-release-tags.py CURRENT CANDIDATE")
    sys.exit(0 if compare(sys.argv[1], sys.argv[2]) <= 0 else 1)
