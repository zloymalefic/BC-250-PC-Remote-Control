#ifndef GAMEPAD_CONTROLLER_H
#define GAMEPAD_CONTROLLER_H

#include <Arduino.h>
#include <Bluepad32.h>
#include "pc_control.h"

#define CONTROLLER_CONFIG_SCHEMA 1
#define MAX_AUTHORIZED_CONTROLLERS 4

struct AuthorizedController {
    String macAddress;
    String label;
    bool enabled = true;
};

struct ControllerConfig {
    uint8_t schema = CONTROLLER_CONFIG_SCHEMA;
    bool enabled = false;
    String triggerMode = "connect";
    String triggerButton = "system";
    unsigned long rearmMs = 5000;
    AuthorizedController controllers[MAX_AUTHORIZED_CONTROLLERS];
    uint8_t controllerCount = 0;
};

struct GamepadIdentity {
    String macAddress;
    String modelName;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
};

extern ControllerConfig controllerConfig;
extern PowerState powerState;
extern bool getStablePcState();
extern void startPowerOn();

inline String normalizeControllerMac(String macAddress) {
    macAddress.trim();
    macAddress.toLowerCase();
    macAddress.replace("-", ":");
    return macAddress;
}

inline bool isValidControllerMac(const String& value) {
    String macAddress = normalizeControllerMac(value);
    if (macAddress.length() != 17) return false;

    for (size_t i = 0; i < macAddress.length(); i++) {
        if ((i + 1) % 3 == 0) {
            if (macAddress[i] != ':') return false;
        } else if (!isHexadecimalDigit(macAddress[i])) {
            return false;
        }
    }
    return true;
}

inline bool isSupportedTriggerMode(const String& mode) {
    return mode == "connect" || mode == "button";
}

inline bool isSupportedTriggerButton(const String& button) {
    return button == "system" || button == "start" || button == "select" ||
           button == "capture" || button == "a" || button == "b" ||
           button == "x" || button == "y";
}

class GamepadController {
public:
    void begin(bool pcAllowsBluetooth) {
        _bluetoothRunning = true;
        _pcAllowsBluetooth = pcAllowsBluetooth;
        applyConfig();
    }

    void applyConfig() {
        if (!controllerConfig.enabled) clearActiveController(true);
        applyBluetoothState();
    }

    void setPcAllowsBluetooth(bool allowed) {
        _pcAllowsBluetooth = allowed;
        if (!allowed) clearActiveController(true);
        applyBluetoothState();
    }

    void onControllerConnected(GamepadPtr controller) {
        if (!controller) return;

        _lastSeenIdentity = readIdentity(controller);
        _hasLastSeenIdentity = true;

        if (_activeController && _activeController != controller) {
            controller->disconnect();
            return;
        }

        if (!controllerConfig.enabled || !_pcAllowsBluetooth ||
            getStablePcState() || powerState != POWER_IDLE) {
            controller->disconnect();
            return;
        }

        if (!isAuthorized(_lastSeenIdentity.macAddress)) {
            Serial.printf("Unauthorized controller discovered: %s (%s)\n",
                          _lastSeenIdentity.macAddress.c_str(),
                          _lastSeenIdentity.modelName.c_str());
            controller->disconnect();
            return;
        }

        _activeController = controller;
        _activeIdentity = _lastSeenIdentity;
        _activeAuthorized = true;
        _triggerButtonWasPressed = false;

        Serial.printf("Authorized controller connected: %s (%s, VID:%04x PID:%04x)\n",
                      _activeIdentity.macAddress.c_str(),
                      _activeIdentity.modelName.c_str(),
                      _activeIdentity.vendorId,
                      _activeIdentity.productId);

        if (controllerConfig.triggerMode == "connect") requestPowerOn();
    }

    void onControllerDisconnected(GamepadPtr controller) {
        if (_activeController == controller) clearActiveController(false);
    }

    void handle() {
        if (!_bluetoothRunning) return;

        BP32.update();
        if (!_activeController) return;

        if (!_activeController->isConnected()) {
            clearActiveController(false);
            return;
        }

        if (!_pcAllowsBluetooth || getStablePcState() || powerState != POWER_IDLE) {
            clearActiveController(true);
            return;
        }

        if (controllerConfig.triggerMode != "button") return;

        bool pressed = isTriggerButtonPressed(_activeController);
        if (pressed && !_triggerButtonWasPressed) requestPowerOn();
        _triggerButtonWasPressed = pressed;
    }

    bool isBluetoothRunning() const {
        return _bluetoothRunning;
    }

    bool isConnected() const {
        return _activeController && _activeController->isConnected();
    }

    bool isConnectedControllerAuthorized() const {
        return isConnected() && _activeAuthorized;
    }

    bool hasLastSeenIdentity() const {
        return _hasLastSeenIdentity;
    }

    GamepadIdentity getActiveIdentity() const {
        return _activeIdentity;
    }

    GamepadIdentity getLastSeenIdentity() const {
        return _lastSeenIdentity;
    }

    bool enrollLastSeen(const String& label, String& error) {
        if (!_hasLastSeenIdentity || !isValidControllerMac(_lastSeenIdentity.macAddress)) {
            error = "No valid controller has been discovered";
            return false;
        }

        for (uint8_t i = 0; i < controllerConfig.controllerCount; i++) {
            if (controllerConfig.controllers[i].macAddress == _lastSeenIdentity.macAddress) {
                controllerConfig.controllers[i].enabled = true;
                if (label.length() > 0) controllerConfig.controllers[i].label = label;
                return true;
            }
        }

        if (controllerConfig.controllerCount >= MAX_AUTHORIZED_CONTROLLERS) {
            error = "Authorized controller list is full";
            return false;
        }

        AuthorizedController& entry =
            controllerConfig.controllers[controllerConfig.controllerCount++];
        entry.macAddress = _lastSeenIdentity.macAddress;
        entry.label = label.length() > 0 ? label : _lastSeenIdentity.modelName;
        entry.enabled = true;
        return true;
    }

    bool removeAuthorized(const String& macAddress) {
        String normalized = normalizeControllerMac(macAddress);
        for (uint8_t i = 0; i < controllerConfig.controllerCount; i++) {
            if (controllerConfig.controllers[i].macAddress != normalized) continue;

            for (uint8_t j = i; j + 1 < controllerConfig.controllerCount; j++) {
                controllerConfig.controllers[j] = controllerConfig.controllers[j + 1];
            }
            controllerConfig.controllers[controllerConfig.controllerCount - 1] =
                AuthorizedController();
            controllerConfig.controllerCount--;

            if (_activeIdentity.macAddress == normalized) clearActiveController(true);
            return true;
        }
        return false;
    }

    void clearAuthorized() {
        clearActiveController(true);
        for (uint8_t i = 0; i < MAX_AUTHORIZED_CONTROLLERS; i++) {
            controllerConfig.controllers[i] = AuthorizedController();
        }
        controllerConfig.controllerCount = 0;
    }

private:
    GamepadPtr _activeController = nullptr;
    GamepadIdentity _activeIdentity;
    GamepadIdentity _lastSeenIdentity;
    bool _activeAuthorized = false;
    bool _hasLastSeenIdentity = false;
    bool _triggerButtonWasPressed = false;
    bool _pcAllowsBluetooth = false;
    bool _bluetoothRunning = false;
    bool _hasTriggered = false;
    unsigned long _lastTriggerAt = 0;

    GamepadIdentity readIdentity(GamepadPtr controller) {
        GamepadIdentity identity;
        GamepadProperties properties = controller->getProperties();
        char macAddress[18];
        snprintf(macAddress, sizeof(macAddress),
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 properties.btaddr[0], properties.btaddr[1], properties.btaddr[2],
                 properties.btaddr[3], properties.btaddr[4], properties.btaddr[5]);
        identity.macAddress = macAddress;
        identity.modelName = controller->getModelName();
        identity.vendorId = properties.vendor_id;
        identity.productId = properties.product_id;
        return identity;
    }

    bool isAuthorized(const String& macAddress) const {
        for (uint8_t i = 0; i < controllerConfig.controllerCount; i++) {
            const AuthorizedController& entry = controllerConfig.controllers[i];
            if (entry.enabled && entry.macAddress == macAddress) return true;
        }
        return false;
    }

    bool isTriggerButtonPressed(GamepadPtr controller) const {
        if (controllerConfig.triggerButton == "a") return controller->a();
        if (controllerConfig.triggerButton == "b") return controller->b();
        if (controllerConfig.triggerButton == "x") return controller->x();
        if (controllerConfig.triggerButton == "y") return controller->y();

        uint16_t miscButtons = controller->miscButtons();
        if (controllerConfig.triggerButton == "start") return miscButtons & MISC_BUTTON_START;
        if (controllerConfig.triggerButton == "select") return miscButtons & MISC_BUTTON_SELECT;
        if (controllerConfig.triggerButton == "capture") return miscButtons & MISC_BUTTON_CAPTURE;
        return miscButtons & MISC_BUTTON_SYSTEM;
    }

    void requestPowerOn() {
        unsigned long now = millis();
        if (_hasTriggered && now - _lastTriggerAt < controllerConfig.rearmMs) return;

        if (!getStablePcState() && powerState == POWER_IDLE) {
            _hasTriggered = true;
            _lastTriggerAt = now;
            Serial.println("Controller trigger accepted; starting PC power-on sequence");
            startPowerOn();
        }
    }

    void clearActiveController(bool disconnect) {
        if (disconnect && _activeController && _activeController->isConnected()) {
            _activeController->disconnect();
        }
        _activeController = nullptr;
        _activeIdentity = GamepadIdentity();
        _activeAuthorized = false;
        _triggerButtonWasPressed = false;
    }

    void applyBluetoothState() {
        bool shouldRun = controllerConfig.enabled && _pcAllowsBluetooth;
        if (shouldRun == _bluetoothRunning) return;

        if (shouldRun) {
            Serial.println("Starting Bluetooth controller service");
            btStart();
            _bluetoothRunning = true;
        } else {
            Serial.println("Stopping Bluetooth controller service");
            clearActiveController(true);
            btStop();
            _bluetoothRunning = false;
        }
    }
};

#endif
