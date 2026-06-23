---
name: bc250-gamepad-support
description: Design, implement, migrate, and verify generic Bluetooth gamepad support for BC-250-PC-Remote-Control using Bluepad32. Use for adding controllers such as 8BitDo, Switch, Xbox, Android, or generic HID gamepads, controller identity and enrollment, MAC allowlists, normalized button triggers, diagnostics, pairing UX, configuration migration, and controller compatibility testing.
---

# BC-250 gamepad support

Keep controller-specific protocol details behind a generic identity, authorization, and trigger layer.

## Workflow

1. Read `references/architecture.md`, `references/bluepad32-contract.md`, and `references/compatibility-matrix.md`.
2. Record the exact ESP32 board, Bluepad32 package version, controller firmware, and controller mode.
3. Capture controller identity and input diagnostics before adding model-specific behavior.
4. Separate these decisions:
   - Can Bluepad32 connect and parse the device?
   - Is the device authorized?
   - Which normalized event triggers power-on?
5. Preserve controller configuration through explicit schema and API migration when a migration path exists.
6. Keep Bluetooth polling non-blocking and independent from the power state machine.
7. Verify with real hardware before marking a controller as supported.

## Design rules

- Use neutral names such as `GamepadController`, `ControllerConfig`, and `/api/controllers/*`.
- Do not branch on a marketing model name when normalized Bluepad32 properties or buttons are sufficient.
- Treat MAC filtering as device selection, not strong authentication.
- Keep connection-trigger mode for broad compatibility; support configurable button-edge triggers where reliable.
- Never trigger repeatedly while a button is held or while `powerState != POWER_IDLE`.
- Reject non-gamepad devices and controllers connected while the PC is on.
- Store diagnostics without logging credentials or unrelated Bluetooth payloads.
- Centralize `btStart()` and `btStop()` ownership.

## Verification

- Test existing configuration handling, one 8BitDo controller, one unauthorized controller, and an unknown/generic gamepad.
- Test reconnect, disconnect, held button, repeated button edge, two simultaneous controllers, disabled support, and busy power state.
- Test malformed and legacy configuration.
- Check every renamed API route against the setup UI.
- Compile with the exact ESP32 + Bluepad32 board package.
- Report Bluetooth connection, parsed input, power trigger, and hardware relay validation separately.
