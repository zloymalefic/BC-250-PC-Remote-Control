# BC-250 host-side tests

These tests cover deterministic behavior that can run on a normal development machine without ESP32 hardware.

## Run

```sh
python3 tools/run_host_tests.py
```

## Covered now

- Frontend `fetch()` calls and firmware `server.on()` routes stay synchronized.
- `XMLHttpRequest.open()` and static form posts, including firmware upload to `/update`, have matching firmware handlers.
- Dynamic power commands in `index.html` resolve to existing `POST /power/*` routes.
- UI-consumed JSON fields are emitted by the matching firmware handlers.
- Wi-Fi config GET does not expose the stored password.
- Controller API routes remain neutral under `/api/controllers/*`.
- Controller config validation mirrors the firmware policy for:
  - MAC normalization and validation;
  - trigger mode and button allowlists;
  - `rearmMs` bounds;
  - duplicate controller detection;
  - maximum authorized controller count.
  - persisted schema gate.

## Explicitly skipped

- Firmware compilation: run `tools/setup_arduino_cli.sh` once, then `python3 tools/build_firmware.py compile`.
- Firmware upload: requires connected authorized ESP32 hardware and an explicit serial `--port`.
- LittleFS image packaging/upload: requires an embedded filesystem image workflow and must stay separate from firmware upload.
- Browser rendering checks: require a browser or device test setup.
- Bluetooth pairing, relay timing, PC power sequencing, OTA flashing, and hardware validation: require authorized hardware and must not run in unattended host-side tests.
