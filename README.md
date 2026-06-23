


# BC-250 Remote PSU Power Controller

ESP32 and 2-channel 5V relay based remote control for a BC-250 ATX power supply. The firmware supports the web interface, physical power button, OTA updates, and Bluetooth gamepads through Bluepad32.

<img width="909" height="541" alt="wires" src="https://github.com/user-attachments/assets/8f6ec507-48f0-44cf-aa08-43292f0b47fc" />

## Key features

- Turn the PC on or off through the web interface or physical button.
- Power on from an authorized Bluetooth controller.
- Use connect or button press as the controller trigger.
- Authorize up to four controllers by Bluetooth MAC address.
- Inspect the detected model, MAC address, vendor ID, and product ID.
- Configure Wi-Fi and install firmware or filesystem updates through the web UI.

## Supported controllers

The implementation uses Bluepad32's normalized gamepad API instead of controller-specific logic. Bluepad32 supports Xbox, Switch, Android, 8BitDo, and many generic HID gamepads.

8BitDo Ultimate 2 Bluetooth must be placed in a Bluetooth-capable mode. Its Bluetooth mode is intended for Switch-compatible operation, so Bluepad32 may report it as an 8BitDo controller, Switch Pro controller, or another compatible model. Windows-oriented 2.4 GHz and USB modes do not connect to the ESP32 Bluetooth receiver.

Controller compatibility still depends on the Bluepad32 release and ESP32 target. Classic Bluetooth gamepads require an original ESP32 with BR/EDR support; ESP32-S3, C3, C6, and H2 cannot replace it for those controllers.

## Installation

Before installing libraries, you need to add the [ESP32_bluepad32](https://github.com/ricardoquesada/bluepad32) platform to your Arduino IDE.

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

## Controller setup

1. Keep the PC off and enable Bluetooth controller support on `/setup`.
2. Put the controller into Bluetooth pairing mode.
3. Wait until its identity appears under “Last discovered controller”.
4. Authorize it explicitly and save the settings.
5. Select either connection or normalized button press as the power-on trigger.

Invalid or unsupported controller configuration fails closed: no unknown controller is allowed to power on the PC.

## Notes

- Wi-Fi AP mode can be slow. Default address: http://192.168.4.1
- HTTPS is not available.
- Testing without TPMS1 pin 9 connected can cause unstable PC-state detection.

<img width="294" height="542" alt="kuva" src="https://github.com/user-attachments/assets/1544a9e2-1a29-4ba2-bede-efac3149f9f3" />
