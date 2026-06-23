#!/usr/bin/env python3
"""Run the repository's deterministic host-side tests."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    command = [sys.executable, "-B", "-m", "unittest", "discover", "-s", "tests"]
    return subprocess.call(command, cwd=REPO_ROOT)


if __name__ == "__main__":
    raise SystemExit(main())
