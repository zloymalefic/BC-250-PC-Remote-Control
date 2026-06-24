


# BC-250 Remote PSU Power Controller

ESP32 and 2-channel 5V relay based remote control for a BC-250 ATX power supply. The firmware supports the web interface, physical power button, OTA updates, and Bluetooth gamepads through Bluepad32.

<img width="909" height="541" alt="wires" src="https://github.com/user-attachments/assets/8f6ec507-48f0-44cf-aa08-43292f0b47fc" />

## Key features

- Turn the PC on or off through the web interface or physical button.
- Power on from an authorized Bluetooth controller.
- Use connect or button press as the controller trigger.
- Authorize up to four controllers by Bluetooth MAC address.
- Inspect the detected model, MAC address, vendor ID, and product ID.
- Configure LED lighting state and an optional external LED power gate.
- Configure Wi-Fi and install firmware or filesystem updates through the web UI.

## Wiring

The PC power-control wiring stays isolated from the optional LED lighting power
gate. Do not power LED strips or fan lighting directly from an ESP32 GPIO.
Use a MOSFET or load switch sized for the LED current, a fused external 5 V LED
supply, and a shared ground with the ESP32.

```text
BC-250 / ATX power control

ESP32 GPIO16 (OPTO_PIN)  -> relay 1 input -> ATX PS_ON path
ESP32 GPIO17 (EXTRA_PIN) -> relay 2 input -> BC-250 power-button pulse
ESP32 GPIO4  (PC_MONITOR_PIN) <- TPMS1 pin 9 monitor signal
ESP32 GPIO22 (BUTTON_PIN) <- momentary button to GND, active LOW
ESP32 GPIO23 (POWER_LED_PIN) -> optional case power LED indicator
ESP32 GPIO2  (STATUS_LED_PIN) -> local firmware/status indicator

Optional LED/ARGB power gate

External 5 V LED PSU +  ---- fuse ---- LED/ARGB +5 V
External 5 V LED PSU -  ---------------- LED/ARGB GND
External 5 V LED PSU -  ---------------- ESP32 GND
ESP32 LED_POWER_PIN ---- gate/enable ---- MOSFET or load-switch control
MOSFET/load switch  ---- switched path -- LED/ARGB power return or +5 V rail
```

`LED_POWER_PIN` is `-1` by default in `pins.h`, so the firmware will not touch a
new GPIO until the wiring is explicitly assigned. The default driver polarity is
active HIGH and the default reported settle time is 50 ms.

| Function | Firmware symbol | Default GPIO | Electrical note |
|---|---:|---:|---|
| PSU relay control | `OPTO_PIN` | 16 | Safety-critical, do not reuse for LEDs |
| BC-250 power button relay | `EXTRA_PIN` | 17 | Pulse output, do not reuse for LEDs |
| PC state monitor | `PC_MONITOR_PIN` | 4 | Input from TPMS1 pin 9 |
| Physical button | `BUTTON_PIN` | 22 | Active LOW to ground |
| Optional LED power gate | `LED_POWER_PIN` | disabled (`-1`) | Assign only after MOSFET/load-switch wiring is verified |
| LED power active level | `LED_POWER_ACTIVE_LEVEL` | `HIGH` | Match the selected driver circuit |
| LED power settle time | `LED_POWER_SETTLE_MS` | 50 ms | Non-blocking API-reported stabilization window |

For addressable ARGB strips or fans, LED data wiring is separate from the power
gate and is not enabled by this firmware yet. Keep the data line short, add the
level shifting/resistor/capacitor recommended by the LED vendor, and do not
connect ARGB 5 V to ESP32 3.3 V pins.

## Supported controllers

The implementation uses Bluepad32's normalized gamepad API instead of controller-specific logic. Bluepad32 supports Xbox, Switch, Android, 8BitDo, and many generic HID gamepads.

8BitDo Ultimate 2 Bluetooth must be placed in a Bluetooth-capable mode. Its Bluetooth mode is intended for Switch-compatible operation, so Bluepad32 may report it as an 8BitDo controller, Switch Pro controller, or another compatible model. Windows-oriented 2.4 GHz and USB modes do not connect to the ESP32 Bluetooth receiver.

Controller compatibility still depends on the Bluepad32 release and ESP32 target. Classic Bluetooth gamepads require an original ESP32 with BR/EDR support; ESP32-S3, C3, C6, and H2 cannot replace it for those controllers.

## Installation

Before installing libraries, you need to add the [ESP32_bluepad32](https://github.com/ricardoquesada/bluepad32) platform to your Arduino IDE or Arduino CLI.

Open Arduino IDE.
Go to File > Preferences (or Arduino IDE > Settings on macOS).
In the "Additional Boards Manager URLs" field, add:

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
https://raw.githubusercontent.com/ricardoquesada/esp32-arduino-lib-builder/master/bluepad32_files/package_esp32_bluepad32_index.json
```

Install the `esp32_bluepad32` board package. This change was developed against the Bluepad32 Arduino package API present in version 3.7.0.

## Libraries

- LittleFS for ESP32
- ArduinoJson

## Build with Arduino CLI

Install the CLI tools:

```sh
brew install arduino-cli platformio
```

Install the Bluepad32 ESP32 board package and ArduinoJson into the repository-local `.arduino/` directory:

```sh
tools/setup_arduino_cli.sh
```

Compile firmware without connected hardware:

```sh
python3 tools/build_firmware.py compile
```

Upload to a connected ESP32 only after checking the target port and wiring:

```sh
python3 tools/build_firmware.py upload --port /dev/cu.usbserial-0001
```

The default FQBN is `esp32-bluepad32:esp32:esp32`. Override it with `--fqbn` or `BC250_FQBN` when using another Bluepad32 ESP32 board.

## Build with PlatformIO

`platformio.ini` is present for PlatformIO entrypoints and host-side checks:

```sh
PLATFORMIO_CORE_DIR=.pio PLATFORMIO_SETTING_ENABLE_TELEMETRY=no pio run -e host-tests -t hosttests
PLATFORMIO_CORE_DIR=.pio PLATFORMIO_SETTING_ENABLE_TELEMETRY=no pio run -e firmware-arduino-cli -t firmware
```

The firmware itself uses Bluepad32's Arduino ESP32 board package. That package is distributed through Arduino Boards Manager rather than as a normal PlatformIO Arduino library, so the PlatformIO firmware target intentionally delegates to the Arduino CLI flow above.

## Emulation

ESP32 emulators can be useful for low-level boot experiments, but this firmware depends on Wi-Fi AP mode, Bluepad32 Bluetooth controller handling, LittleFS web assets, GPIO relay outputs, and BC-250 monitor wiring. Do not treat emulator execution as hardware validation.

- Espressif QEMU is an ESP-IDF `idf.py qemu` workflow, not the Arduino CLI Bluepad32 board-package workflow used here.
- Wokwi can run ESP32 Arduino/custom firmware, but its ESP32 feature table marks Bluetooth as not implemented, so it cannot validate Bluepad32 controller behavior.

Without a physical ESP32 and BC-250 wiring, manual validation is limited to compilation, host-side contract tests, and review of generated firmware artifacts.

## Controller setup

1. Keep the PC off and enable Bluetooth controller support on `/setup`.
2. Put the controller into Bluetooth pairing mode.
3. Wait until its identity appears under “Last discovered controller”.
4. Authorize it explicitly and save the settings.
5. Select either connection or normalized button press as the power-on trigger.

Invalid or unsupported controller configuration fails closed: no unknown controller is allowed to power on the PC.

## Web UI and Wi-Fi setup

The embedded web UI is served from LittleFS and is designed to work without internet access.

1. On first boot, or when no saved Wi-Fi configuration exists, connect to the `BC-250-POWER-CONTROL` access point.
2. Open http://192.168.4.1 in a browser.
3. Use `/setup` to review the current Wi-Fi mode, scan nearby networks, enter the SSID and password, and save.
4. The device stores the credentials and restarts. After reboot, connect your browser to the device on the configured network.
5. Use `/` for power control, `/setup` for Wi-Fi and controller settings, and `/update` for firmware or filesystem OTA uploads.

The Wi-Fi config API returns SSID and mode state only. It must not expose the stored password in the UI, logs, or API responses. If the configured network cannot be reached, the firmware falls back to AP mode at http://192.168.4.1.

## Host-side tests

Run deterministic checks that do not require ESP32 hardware:

```sh
python3 tools/run_host_tests.py
```

These tests cover frontend/API route consistency, UI-consumed JSON fields, Wi-Fi password non-exposure, and controller configuration validation. Firmware compilation and hardware validation still require the ESP32 Bluepad32 toolchain and connected test hardware.

## Notes

- Wi-Fi AP mode can be slow. Default address: http://192.168.4.1
- HTTPS is not available.
- Testing without TPMS1 pin 9 connected can cause unstable PC-state detection.

<img width="294" height="542" alt="kuva" src="https://github.com/user-attachments/assets/1544a9e2-1a29-4ba2-bede-efac3149f9f3" />
