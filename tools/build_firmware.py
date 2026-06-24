#!/usr/bin/env python3
"""Build and optionally upload the BC-250 firmware with Arduino CLI.

The repository keeps the original single-sketch layout at the project root.
Arduino CLI is stricter about sketch folder names, so this helper prepares a
temporary sketch directory under build/ before invoking arduino-cli.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SKETCH_NAME = "ota_pc_remote"
SKETCH_BUILD_DIR = REPO_ROOT / "build" / "arduino" / SKETCH_NAME
OUTPUT_DIR = REPO_ROOT / "build" / "arduino" / "out"
BUILD_PATH = REPO_ROOT / "build" / "arduino" / "cache-clean"
ARDUINO_CONFIG = REPO_ROOT / "arduino-cli.yaml"
DEFAULT_FQBN = "esp32-bluepad32:esp32:esp32"

SKETCH_FILES = (
    "ota_pc_remote.ino",
    "gamepad_controller.h",
    "led_power_driver.h",
    "pc_control.h",
    "pins.h",
    "version.h",
    "web_server.h",
)


def run(command: list[str]) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=REPO_ROOT, check=True)


def prepare_sketch() -> Path:
    SKETCH_BUILD_DIR.mkdir(parents=True, exist_ok=True)
    for name in SKETCH_FILES:
        shutil.copy2(REPO_ROOT / name, SKETCH_BUILD_DIR / name)
    return SKETCH_BUILD_DIR


def arduino_cli_base() -> list[str]:
    return ["arduino-cli", "--config-file", str(ARDUINO_CONFIG)]


def compile_firmware(fqbn: str) -> None:
    sketch_dir = prepare_sketch()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    BUILD_PATH.mkdir(parents=True, exist_ok=True)
    run(
        arduino_cli_base()
        + [
            "compile",
            "--fqbn",
            fqbn,
            "--clean",
            "--build-path",
            str(BUILD_PATH),
            "--output-dir",
            str(OUTPUT_DIR),
            str(sketch_dir),
        ]
    )


def upload_firmware(fqbn: str, port: str) -> None:
    compile_firmware(fqbn)
    run(
        arduino_cli_base()
        + [
            "upload",
            "--fqbn",
            fqbn,
            "--port",
            port,
            "--input-dir",
            str(OUTPUT_DIR),
            str(SKETCH_BUILD_DIR),
        ]
    )


def monitor(port: str) -> None:
    run(arduino_cli_base() + ["monitor", "--port", port, "--config", "115200"])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "action",
        choices=("prepare", "compile", "upload", "monitor"),
        help="Operation to run. Upload and monitor require --port or BC250_PORT.",
    )
    parser.add_argument(
        "--fqbn",
        default=os.environ.get("BC250_FQBN", DEFAULT_FQBN),
        help=f"Arduino FQBN. Default: {DEFAULT_FQBN}",
    )
    parser.add_argument(
        "--port",
        default=os.environ.get("BC250_PORT"),
        help="Serial port for upload/monitor, for example /dev/cu.usbserial-0001.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.action == "prepare":
            print(prepare_sketch())
        elif args.action == "compile":
            compile_firmware(args.fqbn)
        elif args.action == "upload":
            if not args.port:
                raise SystemExit("upload requires --port or BC250_PORT")
            upload_firmware(args.fqbn, args.port)
        elif args.action == "monitor":
            if not args.port:
                raise SystemExit("monitor requires --port or BC250_PORT")
            monitor(args.port)
    except FileNotFoundError as exc:
        if exc.filename == "arduino-cli":
            raise SystemExit(
                "arduino-cli was not found. Install it, then run "
                "tools/setup_arduino_cli.sh."
            ) from exc
        raise
    except subprocess.CalledProcessError as exc:
        return exc.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
