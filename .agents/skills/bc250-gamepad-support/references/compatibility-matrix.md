# Controller compatibility matrix

Record:

| Field | Evidence |
|---|---|
| Model | Exact product and hardware revision |
| Firmware | Controller firmware version |
| Mode | Switch, Android, Windows/XInput, macOS, or other |
| Transport | BR/EDR or BLE |
| Bluepad32 | Package/library version |
| Identity | MAC behavior, VID, PID, parsed type |
| Connection | Pair, reconnect, disconnect |
| Inputs | System/start/select/A/B visibility |
| Trigger | Connection and button-edge behavior |
| Result | Supported, limited, unsupported, or upstream parser needed |

## 8BitDo Ultimate 2 Bluetooth

Official 8BitDo specifications state that Bluetooth connectivity is for Switch, while Windows uses 2.4 GHz or USB. Bluepad32 documents support for the 8BitDo family, common 8BitDo modes, and Switch Pro controllers, but does not list Ultimate 2 as an individually tested model.

Required spike:

1. Update and record controller firmware.
2. Select Bluetooth/Switch mode.
3. Pair with the original ESP32 target.
4. Capture MAC, VID/PID, model/type, and normalized buttons.
5. Verify whether Home maps to `MISC_BUTTON_SYSTEM`.
6. Verify reconnect after dock removal and ESP32 restart.
7. Do not mark support complete until repeated power triggers work without duplicate starts.
