# Platform contract

## Startup

`setup()` initializes pins, reads monitor state, loads Wi-Fi configuration, connects or starts AP mode, mounts LittleFS, initializes Bluepad32, loads controller configuration, and starts the web server.

LittleFS is currently mounted more than once. Avoid accidental formatting and make mount failure explicit.

## Persistence

- `/wifi_config.json`: `ssid`, `password`.
- Legacy `/ps5_config.json`: `enabled`, `macAddress`, `autoConnect`.
- New controller work must migrate to a versioned neutral schema; see `$bc250-gamepad-support`.

Use bounded ArduinoJson documents and validate required fields before replacing active configuration.

## Connectivity

- AP SSID: `BC-250-POWER-CONTROL`.
- Provisioning is HTTP-only and must work without internet.
- Station connection attempts are bounded to 20 iterations of 100 ms.
- Bluepad32 controller triggers are accepted only while the PC is off and power state is idle.

## OTA

- `/update` writes firmware with `UPDATE_SIZE_UNKNOWN`.
- `/update-fs` writes the filesystem using `U_SPIFFS`.
- Both restart after completion.
- There is currently no authentication, signature verification, or CSRF protection.
