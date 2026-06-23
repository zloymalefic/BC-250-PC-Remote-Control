# Generic controller architecture

## `GamepadIdentity`

Capture stable diagnostic properties available from Bluepad32:

- Bluetooth address;
- vendor and product IDs;
- controller type/model;
- device type and gamepad capability;
- firmware/mode notes recorded during hardware validation.

Do not assume VID/PID is stable across controller modes. MAC remains the initial enrollment key unless hardware testing proves otherwise.

## `ControllerRegistry`

Own enabled state, authorized entries, enrollment window, legacy migration, last-seen identity, and active-controller limits.

Start with one authorized controller for behavior compatibility, but design the schema as an array so multiple controllers can be added without another migration.

## `ControllerTrigger`

Support explicit policies:

- `connect`: trigger once after an authorized gamepad becomes ready;
- `button`: trigger on a configured normalized button rising edge;
- `connect_or_button`: compatibility/testing mode only when explicitly required.

## `GamepadController`

Own Bluepad32 callbacks and polling. It may request `startPowerOn()`, but it must not contain relay timing or modify `PowerState` directly.

## Migration target

- Keep the controller module neutral.
- Use `GamepadController` for Bluepad32 lifecycle and input handling.
- Store controller settings in versioned `ControllerConfig`.
- Use `/controller_config.json` for persisted controller settings.
- Use `/api/controllers/*` for all controller HTTP endpoints.

Never maintain two writable sources of truth.
