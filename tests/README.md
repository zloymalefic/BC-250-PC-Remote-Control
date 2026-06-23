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

- Firmware compilation: requires Arduino IDE/CLI or PlatformIO plus the ESP32 Bluepad32 board package.
- LittleFS image packaging/upload: requires the embedded toolchain and target workflow.
- Browser rendering checks: require a browser or device test setup.
- Bluetooth pairing, relay timing, PC power sequencing, OTA flashing, and hardware validation: require authorized hardware and must not run in unattended host-side tests.
