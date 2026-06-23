# HTTP contract

## Static resources

- `GET /` -> `data/index.html`
- `GET /setup` -> `data/setup.html`
- `GET /update` -> `data/update.html`
- `GET /style.css` -> `data/style.css`
- `GET /steam-machines.svg` -> filesystem or fallback SVG

## Status and power

- `GET /api/status` -> `pcOn`, `shutdownRequested`, `forceShutdown`, `optoState`, `extraPinState`, `monitorState`, `version`.
- `POST /power/on` -> starts power-on when monitor is low.
- `POST /power/off` -> currently starts forced shutdown despite the route name.
- `POST /power/force` -> starts forced shutdown.

Inspect handlers; route names do not guarantee physical behavior.

## Wi-Fi and Bluetooth

- `GET /api/bluetooth/mac`
- `GET /api/wifi/config` -> SSID and state, never password.
- `POST /api/wifi/config` with `ssid` and optional `password`; saves then restarts.
- `GET /api/wifi/scan` -> scanning object or network array.

## Controllers

- `GET|POST /api/controllers/config`
- `GET /api/controllers/status`
- `GET /api/controllers/discovered`
- `POST /api/controllers/enroll`
- `POST /api/controllers/remove`

## OTA

- `POST /update` accepts firmware.
- `POST /update-fs` accepts a filesystem image.

These routes currently have no authentication.
