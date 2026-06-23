# Bluepad32 contract

## Normalized input

Bluepad32 maps different gamepads into common axes, buttons, D-pad, pedals, and miscellaneous buttons. Use normalized values instead of parsing controller-specific reports.

- `MISC_BUTTON_SYSTEM`: PS, Xbox, Switch Home, or equivalent.
- `MISC_BUTTON_SELECT`: Select, Share, Create, or minus.
- `MISC_BUTTON_START`: Start, Options, or plus.
- `MISC_BUTTON_CAPTURE`: Capture, mute, or equivalent.

## Controller discovery

On connection:

1. Reject null or non-gamepad controllers.
2. Capture MAC, VID, PID, controller type/model, and capability.
3. Apply enrollment/allowlist policy.
4. Retain `GamepadPtr` only while valid and clear it on disconnect.
5. Do not deliberately disconnect until the trigger policy has completed.

## Input processing

- Call `BP32.update()` from one regular non-blocking path.
- Process rising edges, not held levels.
- Reset edge state on disconnect.
- Gate power requests on stable PC-off state and `POWER_IDLE`.
- Rearm triggers so reconnect storms cannot issue repeated commands.

## Platform constraint

Many 8BitDo and Switch-compatible controllers use Bluetooth Classic BR/EDR. Bluepad32 documents BR/EDR support on the original ESP32, but not ESP32-S3/C3/C6/H2. Preserve the original ESP32 target unless the selected controller is confirmed to use BLE.

## Versioning

The repository does not pin a Bluepad32 package version. Record the installed package version before making reproducible compatibility claims.
