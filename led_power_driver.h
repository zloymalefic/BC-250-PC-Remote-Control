#ifndef LED_POWER_DRIVER_H
#define LED_POWER_DRIVER_H

#include <Arduino.h>
#include "pins.h"

class LedPowerDriver {
public:
    void begin() {
        desiredEnabled = false;
        outputEnabled = false;
        lastChangeMs = millis();
        if (isConfigured()) {
            pinMode(LED_POWER_PIN, OUTPUT);
            writeOutput(false);
        }
    }

    void setEnabled(bool enabled) {
        desiredEnabled = enabled;
        if (!isConfigured()) {
            outputEnabled = false;
            lastChangeMs = millis();
            return;
        }
        if (outputEnabled == enabled) return;

        writeOutput(enabled);
        outputEnabled = enabled;
        lastChangeMs = millis();
    }

    bool isConfigured() const {
        return LED_POWER_PIN >= 0;
    }

    bool isDesiredEnabled() const {
        return desiredEnabled;
    }

    bool isOutputEnabled() const {
        return outputEnabled;
    }

    bool isReady(unsigned long now) const {
        if (!outputEnabled) return true;
        return now - lastChangeMs >= LED_POWER_SETTLE_MS;
    }

    int pin() const {
        return LED_POWER_PIN;
    }

    bool activeHigh() const {
        return LED_POWER_ACTIVE_LEVEL == HIGH;
    }

    unsigned long settleMs() const {
        return LED_POWER_SETTLE_MS;
    }

private:
    bool desiredEnabled = false;
    bool outputEnabled = false;
    unsigned long lastChangeMs = 0;

    void writeOutput(bool enabled) {
        digitalWrite(LED_POWER_PIN, enabled ? LED_POWER_ACTIVE_LEVEL : !LED_POWER_ACTIVE_LEVEL);
    }
};

#endif
