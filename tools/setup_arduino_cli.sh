#!/usr/bin/env bash
set -euo pipefail

BLUEPAD32_CORE_VERSION="${BC250_BLUEPAD32_CORE_VERSION:-4.1.0}"
ARDUINOJSON_VERSION="${BC250_ARDUINOJSON_VERSION:-6.21.5}"

arduino-cli core update-index --config-file arduino-cli.yaml
arduino-cli core install "esp32-bluepad32:esp32@${BLUEPAD32_CORE_VERSION}" --config-file arduino-cli.yaml
arduino-cli lib install "ArduinoJson@${ARDUINOJSON_VERSION}" --config-file arduino-cli.yaml

arduino-cli core list --config-file arduino-cli.yaml
arduino-cli lib list --config-file arduino-cli.yaml
