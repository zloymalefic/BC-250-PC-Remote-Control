---
name: bc250-connectivity
description: Develop and review BC-250 ESP32 platform services. Use for Wi-Fi AP/STA fallback, Bluepad32 lifecycle, LittleFS JSON persistence, firmware or filesystem OTA, startup ordering, restart behavior, and version changes. Use bc250-gamepad-support for controller identity, enrollment, triggers, compatibility, and PS5-to-generic migration.
---

# BC-250 connectivity

Maintain connectivity and persistence while preserving offline AP-mode operation and safe recovery.

## Workflow

1. Read `references/platform-contract.md`.
2. Trace startup across LittleFS, Wi-Fi, Bluepad32, configuration loading, and web server setup.
3. Identify persistence and restart side effects before editing.
4. Keep credentials private and preserve AP fallback.
5. Keep Bluetooth work bounded so HTTP and power-state handling remain responsive.
6. Review OTA success, interruption, and error paths.
7. Change `version.h` only for a requested or release-worthy version bump.

## Invariants

- Never return the Wi-Fi password from an API or print it.
- Preserve the `192.168.4.1` setup workflow unless provisioning is explicitly redesigned.
- Treat `LittleFS.begin(true)` as potentially destructive formatting.
- Do not present MAC filtering as strong authentication.
- Keep firmware and filesystem OTA targets distinct.
- Avoid long `delay()` calls or synchronous network loops in normal operation.

## Verification

- Check valid, missing, unreadable, and malformed configuration.
- Check Wi-Fi success, timeout, and AP fallback.
- Check controller support disabled, allowed identity, rejected identity, disconnect, and PC-on behavior.
- Check firmware and filesystem OTA completion and errors.
- Check that no secret appears in output.
- Compile against the target Bluepad32 ESP32 package when available.
