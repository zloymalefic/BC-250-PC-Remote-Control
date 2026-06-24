#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "LittleFS.h"
#include <Bluepad32.h>
#include <ArduinoJson.h>

#include "version.h"
#include "pins.h"
#include "pc_control.h"
#include "gamepad_controller.h"
#include "led_power_driver.h"

// Globaalit muuttujat
WebServer server(80);

bool pcIsOn = false;
bool shutdownRequested = false;
bool forceShutdown = false;
unsigned long forceShutdownStartTime = 0;
const unsigned long forceShutdownDuration = 5000;

// WiFi-muuttujat
String wifiSSID = "";
String wifiPassword = "";
bool wifiConfigured = false;
bool apMode = false;

ControllerConfig controllerConfig;

const uint8_t LED_CHANNEL_COUNT = 2;
const uint8_t LED_MAX_PIXELS_PER_CHANNEL = 50;
const uint8_t LED_EFFECT_COUNT = 5;
const char* const LED_EFFECTS[LED_EFFECT_COUNT] = {
    "static",
    "breathing",
    "colorCycle",
    "rainbow",
    "sequential"
};

struct LedChannelState {
    bool enabled = true;
    uint8_t pixelCount = 24;
    String effect = "static";
    uint8_t red = 0;
    uint8_t green = 217;
    uint8_t blue = 255;
    uint8_t secondaryRed = 255;
    uint8_t secondaryGreen = 77;
    uint8_t secondaryBlue = 109;
    uint8_t speed = 50;
    bool reverse = false;
};

struct LedState {
    bool on = true;
    uint8_t brightness = 128;
    LedChannelState channels[LED_CHANNEL_COUNT];
};

LedState ledState;
LedPowerDriver ledPowerDriver;

// OPTIMOIDUT INTERVALLIT
unsigned long lastPinRead = 0;
const unsigned long pinReadInterval = 50;

unsigned long lastServerHandle = 0;
const unsigned long serverHandleInterval = 20;

unsigned long lastPcStateHandle = 0;
const unsigned long pcStateHandleInterval = 50;

unsigned long lastButtonDebounce = 0;
const unsigned long debounceDelay = 50;

// Välimuistissa olevat pinnien tilat
bool cachedButtonState = HIGH;
bool lastStableButtonState = HIGH;
bool buttonPressed = false;

// Filtteröity PC:n tila
bool filteredPcState = false;
unsigned long lastPcChangeTime = 0;
const unsigned long pcStableDelay = 100;

// Power-tilakoneen muuttujat
PowerState powerState = POWER_IDLE;
unsigned long powerStateStartTime = 0;

GamepadController gamepadController;

// ================ PROTOTYYPIT ================
bool getStablePcState();
void startPowerOn();
void startForceShutdown();
void startNormalShutdown();
bool saveControllerConfig();
bool updateControllerConfigFromJson(JsonObjectConst root, String& error);
bool saveLedState();
bool updateLedStateFromJson(JsonObjectConst root, String& error);
void applyLedState();

void setControllerBluetoothAllowed(bool allowed) {
    gamepadController.setPcAllowsBluetooth(allowed);
}

// ================ CALLBACK-FUNKTIOT ================
void onConnectedGamepad(GamepadPtr gp) {
    Serial.println("=== UUSI OHJAIN HAVAITTU ===");
    if (gp != nullptr) {
        gamepadController.onControllerConnected(gp);
    }
}

void onDisconnectedGamepad(GamepadPtr gp) {
    Serial.println("=== OHJAIN IRROTTUNUT ===");
    gamepadController.onControllerDisconnected(gp);
}

#include "web_server.h"

// ================ WiFi-konfiguraatio ================

void saveWiFiConfig(String ssid, String pass) {
    File file = LittleFS.open("/wifi_config.json", "w");
    if (!file) return;
    
    StaticJsonDocument<200> doc;
    doc["ssid"] = ssid;
    doc["password"] = pass;
    
    serializeJson(doc, file);
    file.close();
    
    wifiSSID = ssid;
    wifiPassword = pass;
    wifiConfigured = true;
}

void loadWiFiConfig() {
    if (!LittleFS.begin(true)) {
        wifiConfigured = false;
        apMode = true;
        return;
    }
    
    if (!LittleFS.exists("/wifi_config.json")) {
        wifiConfigured = false;
        apMode = true;
        return;
    }
    
    File file = LittleFS.open("/wifi_config.json", "r");
    if (!file) return;
    
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        wifiConfigured = false;
        apMode = true;
        return;
    }
    
    wifiSSID = doc["ssid"] | "";
    wifiPassword = doc["password"] | "";
    
    wifiConfigured = (wifiSSID.length() > 0);
    apMode = !wifiConfigured;
}

bool connectToWiFi() {
    loadWiFiConfig();
    
    if (!wifiConfigured || wifiSSID.length() == 0) {
        apMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("BC-250-POWER-CONTROL", "");
        return true;
    }
    
    WiFi.mode(WIFI_STA);
    String hostname = "bc250-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.setHostname(hostname.c_str());
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(100);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        apMode = false;
        return true;
    } else {
        apMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("BC-250-POWER-CONTROL", "");
        return false;
    }
}

// ================ PC-tilan suodatus ================



// ================ Controller configuration ================

void resetControllerConfig() {
    controllerConfig = ControllerConfig();
}

bool saveControllerConfig() {
    File file = LittleFS.open("/controller_config.json", "w");
    if (!file) {
        Serial.println("Controller config: failed to open file for writing");
        return false;
    }

    StaticJsonDocument<1536> doc;
    doc["schema"] = CONTROLLER_CONFIG_SCHEMA;
    doc["enabled"] = controllerConfig.enabled;
    JsonObject trigger = doc.createNestedObject("trigger");
    trigger["mode"] = controllerConfig.triggerMode;
    trigger["button"] = controllerConfig.triggerButton;
    trigger["rearmMs"] = controllerConfig.rearmMs;

    JsonArray controllers = doc.createNestedArray("controllers");
    for (uint8_t i = 0; i < controllerConfig.controllerCount; i++) {
        JsonObject entry = controllers.createNestedObject();
        entry["macAddress"] = controllerConfig.controllers[i].macAddress;
        entry["label"] = controllerConfig.controllers[i].label;
        entry["enabled"] = controllerConfig.controllers[i].enabled;
    }

    bool success = serializeJson(doc, file) > 0;
    file.close();
    if (success) gamepadController.applyConfig();
    return success;
}

bool updateControllerConfigFromJson(JsonObjectConst root, String& error) {
    ControllerConfig next;
    next.enabled = root["enabled"] | false;

    JsonObjectConst trigger = root["trigger"];
    next.triggerMode = trigger["mode"] | "connect";
    next.triggerButton = trigger["button"] | "system";
    next.rearmMs = trigger["rearmMs"] | 5000;

    if (!isSupportedTriggerMode(next.triggerMode)) {
        error = "Unsupported trigger mode";
        return false;
    }
    if (!isSupportedTriggerButton(next.triggerButton)) {
        error = "Unsupported trigger button";
        return false;
    }
    if (next.rearmMs < 1000 || next.rearmMs > 60000) {
        error = "rearmMs must be between 1000 and 60000";
        return false;
    }

    JsonArrayConst controllers = root["controllers"];
    if (!controllers.isNull()) {
        for (JsonObjectConst item : controllers) {
            if (next.controllerCount >= MAX_AUTHORIZED_CONTROLLERS) {
                error = "Too many authorized controllers";
                return false;
            }

            String macAddress = normalizeControllerMac(item["macAddress"] | "");
            if (!isValidControllerMac(macAddress)) {
                error = "Invalid controller MAC address";
                return false;
            }

            bool duplicate = false;
            for (uint8_t i = 0; i < next.controllerCount; i++) {
                if (next.controllers[i].macAddress == macAddress) duplicate = true;
            }
            if (duplicate) {
                error = "Duplicate controller MAC address";
                return false;
            }

            AuthorizedController& entry = next.controllers[next.controllerCount++];
            entry.macAddress = macAddress;
            entry.label = String(item["label"] | "");
            entry.enabled = item["enabled"] | true;
        }
    }

    controllerConfig = next;
    return true;
}

void loadControllerConfig() {
    resetControllerConfig();
    if (!LittleFS.exists("/controller_config.json")) {
        Serial.println("Controller config: no config found; controller service disabled");
        return;
    }

    File file = LittleFS.open("/controller_config.json", "r");
    if (!file) {
        Serial.println("Controller config: failed to open config; controller service disabled");
        return;
    }

    StaticJsonDocument<1536> doc;
    DeserializationError parseError = deserializeJson(doc, file);
    file.close();
    if (parseError || (doc["schema"] | 0) != CONTROLLER_CONFIG_SCHEMA) {
        Serial.println("Controller config: invalid or unsupported config; controller service disabled");
        return;
    }

    String validationError;
    if (!updateControllerConfigFromJson(doc.as<JsonObjectConst>(), validationError)) {
        resetControllerConfig();
        Serial.printf("Controller config: %s; controller service disabled\n",
                      validationError.c_str());
        return;
    }

    Serial.printf("Controller config loaded: enabled=%s, authorized=%u, trigger=%s/%s\n",
                  controllerConfig.enabled ? "true" : "false",
                  controllerConfig.controllerCount,
                  controllerConfig.triggerMode.c_str(),
                  controllerConfig.triggerButton.c_str());
}

// ================ LED lighting state ================

bool isSupportedLedEffect(const String& effect) {
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        if (effect == LED_EFFECTS[i]) return true;
    }
    return false;
}

void applyLedState() {
    bool anyChannelEnabled = false;
    for (uint8_t i = 0; i < LED_CHANNEL_COUNT; i++) {
        if (ledState.channels[i].enabled && ledState.channels[i].pixelCount > 0) {
            anyChannelEnabled = true;
        }
    }
    bool requestedPower = ledState.on && ledState.brightness > 0 && anyChannelEnabled;
    ledPowerDriver.setEnabled(requestedPower);

    // STATUS_LED_PIN remains a local indication of requested LED power state.
    digitalWrite(STATUS_LED_PIN, requestedPower ? HIGH : LOW);
}

void writeLedStateJson(JsonDocument& doc) {
    doc["schema"] = 2;
    doc["on"] = ledState.on;
    doc["bri"] = ledState.brightness;
    doc["channelCount"] = LED_CHANNEL_COUNT;
    doc["maxPixelsPerChannel"] = LED_MAX_PIXELS_PER_CHANNEL;
    JsonObject power = doc.createNestedObject("power");
    power["configured"] = ledPowerDriver.isConfigured();
    power["pin"] = ledPowerDriver.pin();
    power["activeHigh"] = ledPowerDriver.activeHigh();
    power["enabled"] = ledPowerDriver.isOutputEnabled();
    power["requested"] = ledPowerDriver.isDesiredEnabled();
    power["ready"] = ledPowerDriver.isReady(millis());
    power["settleMs"] = ledPowerDriver.settleMs();

    JsonArray effects = doc.createNestedArray("supportedEffects");
    for (uint8_t i = 0; i < LED_EFFECT_COUNT; i++) {
        effects.add(LED_EFFECTS[i]);
    }

    JsonArray channels = doc.createNestedArray("channels");
    for (uint8_t i = 0; i < LED_CHANNEL_COUNT; i++) {
        const LedChannelState& channel = ledState.channels[i];
        JsonObject entry = channels.createNestedObject();
        entry["id"] = i + 1;
        entry["enabled"] = channel.enabled;
        entry["pixels"] = channel.pixelCount;
        entry["effect"] = channel.effect;
        JsonArray color = entry.createNestedArray("color");
        color.add(channel.red);
        color.add(channel.green);
        color.add(channel.blue);
        JsonArray secondaryColor = entry.createNestedArray("secondaryColor");
        secondaryColor.add(channel.secondaryRed);
        secondaryColor.add(channel.secondaryGreen);
        secondaryColor.add(channel.secondaryBlue);
        entry["speed"] = channel.speed;
        entry["reverse"] = channel.reverse;
    }
}

bool saveLedState() {
    File file = LittleFS.open("/led_config.json", "w");
    if (!file) {
        Serial.println("LED config: failed to open file for writing");
        return false;
    }

    StaticJsonDocument<1536> doc;
    writeLedStateJson(doc);

    bool success = serializeJson(doc, file) > 0;
    file.close();
    return success;
}

bool readLedColor(JsonVariantConst value, uint8_t& red, uint8_t& green, uint8_t& blue, String& error, const char* fieldName) {
    JsonArrayConst color = value.as<JsonArrayConst>();
    if (color.isNull() || color.size() != 3) {
        error = String(fieldName) + " must contain three values";
        return false;
    }

    int nextRed = color[0] | -1;
    int nextGreen = color[1] | -1;
    int nextBlue = color[2] | -1;
    if (nextRed < 0 || nextRed > 255 || nextGreen < 0 || nextGreen > 255 || nextBlue < 0 || nextBlue > 255) {
        error = String(fieldName) + " values must be between 0 and 255";
        return false;
    }

    red = static_cast<uint8_t>(nextRed);
    green = static_cast<uint8_t>(nextGreen);
    blue = static_cast<uint8_t>(nextBlue);
    return true;
}

bool updateLedStateFromJson(JsonObjectConst root, String& error) {
    LedState next = ledState;

    if (root.containsKey("on")) {
        next.on = root["on"] | false;
    }

    if (root.containsKey("bri")) {
        int brightness = root["bri"] | -1;
        if (brightness < 0 || brightness > 255) {
            error = "bri must be between 0 and 255";
            return false;
        }
        next.brightness = static_cast<uint8_t>(brightness);
    }

    JsonArrayConst legacyRgb = root["rgb"];
    if (!legacyRgb.isNull()) {
        if (!readLedColor(legacyRgb, next.channels[0].red, next.channels[0].green, next.channels[0].blue, error, "rgb")) {
            return false;
        }
    }

    if (root.containsKey("channels") && !root["channels"].is<JsonArrayConst>()) {
        error = "invalid LED channels";
        return false;
    }

    JsonArrayConst channels = root["channels"];
    if (!channels.isNull()) {
        if (channels.size() > LED_CHANNEL_COUNT) {
            error = "too many LED channels";
            return false;
        }

        for (JsonObjectConst item : channels) {
            int channelId = item["id"] | 0;
            if (channelId < 1 || channelId > LED_CHANNEL_COUNT) {
                error = "invalid LED channel id";
                return false;
            }

            LedChannelState& channel = next.channels[channelId - 1];

            if (item.containsKey("enabled")) {
                channel.enabled = item["enabled"] | false;
            }

            if (item.containsKey("pixels")) {
                int pixels = item["pixels"] | -1;
                if (pixels < 0 || pixels > LED_MAX_PIXELS_PER_CHANNEL) {
                    error = "pixels must be between 0 and 50";
                    return false;
                }
                channel.pixelCount = static_cast<uint8_t>(pixels);
            }

            if (item.containsKey("effect")) {
                String effect = item["effect"] | "";
                if (!isSupportedLedEffect(effect)) {
                    error = "unsupported LED effect";
                    return false;
                }
                channel.effect = effect;
            }

            if (item.containsKey("color") &&
                !readLedColor(item["color"], channel.red, channel.green, channel.blue, error, "color")) {
                return false;
            }

            if (item.containsKey("secondaryColor") &&
                !readLedColor(item["secondaryColor"], channel.secondaryRed, channel.secondaryGreen, channel.secondaryBlue, error, "secondaryColor")) {
                return false;
            }

            if (item.containsKey("speed")) {
                int speed = item["speed"] | -1;
                if (speed < 1 || speed > 100) {
                    error = "speed must be between 1 and 100";
                    return false;
                }
                channel.speed = static_cast<uint8_t>(speed);
            }

            if (item.containsKey("reverse")) {
                channel.reverse = item["reverse"] | false;
            }
        }
    }

    ledState = next;
    applyLedState();
    return true;
}

void loadLedState() {
    ledState = LedState();

    if (!LittleFS.exists("/led_config.json")) {
        applyLedState();
        return;
    }

    File file = LittleFS.open("/led_config.json", "r");
    if (!file) {
        Serial.println("LED config: failed to open config; using defaults");
        applyLedState();
        return;
    }

    StaticJsonDocument<1536> doc;
    DeserializationError parseError = deserializeJson(doc, file);
    file.close();
    int schema = doc["schema"] | 0;
    if (parseError || (schema != 1 && schema != 2)) {
        Serial.println("LED config: invalid or unsupported config; using defaults");
        applyLedState();
        return;
    }

    String validationError;
    if (!updateLedStateFromJson(doc.as<JsonObjectConst>(), validationError)) {
        ledState = LedState();
        Serial.printf("LED config: %s; using defaults\n", validationError.c_str());
        applyLedState();
        return;
    }
}

// ================ SETUP ================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== BC-250 STARTING ===");
    
    initPins();
    Serial.println("Pins initialized");
    ledPowerDriver.begin();
    Serial.printf("LED power driver: %s (pin=%d, active=%s, settle=%lums)\n",
                  ledPowerDriver.isConfigured() ? "configured" : "disabled",
                  ledPowerDriver.pin(),
                  ledPowerDriver.activeHigh() ? "HIGH" : "LOW",
                  ledPowerDriver.settleMs());
    
    Serial.print("PC_MONITOR_PIN (14): ");
    Serial.println(digitalRead(PC_MONITOR_PIN) ? "HIGH" : "LOW");
    Serial.print("OPTO_PIN (16): ");
    Serial.println(digitalRead(OPTO_PIN) ? "HIGH" : "LOW");
    Serial.print("EXTRA_PIN (32): ");  // LISÄTTY: EXTRA_PIN tila
    Serial.println(digitalRead(EXTRA_PIN) ? "HIGH" : "LOW");
    
    filteredPcState = digitalRead(PC_MONITOR_PIN);
    pcIsOn = filteredPcState;
    
    Serial.println("Loading WiFi config...");
    loadWiFiConfig();
    
    Serial.println("Connecting to WiFi...");
    connectToWiFi();
    
    Serial.print("WiFi mode: ");
    Serial.println(apMode ? "AP" : "STA");
    if (!apMode) {
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    }
    
    Serial.println("Mounting LittleFS...");
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
    } else {
        Serial.println("LittleFS mounted");
    }

    Serial.println("Setting up Bluepad32...");
    
    // VAIN NÄMÄ KAKSI RIVIÄ TARVITAAN
    BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
    BP32.enableVirtualDevice(false);
    
    Serial.println("Loading controller config...");
    loadControllerConfig();
    gamepadController.begin(!pcIsOn && powerState == POWER_IDLE);

    Serial.println("Loading LED config...");
    loadLedState();

    Serial.printf("Bluepad32 %s ready - waiting for controller pairing\n",
                  BP32.firmwareVersion());
    
    Serial.println("Setting up web server...");
    setupWebServer();
    
    Serial.println("=== BC-250 READY ===\n");
}

void loop() {
    unsigned long now = millis();
    
    // ================ YKSINKERTAINEN RATKAISU: ESP32 UUDELLEENKÄYNNISTYS ================
    static unsigned long pcOffStartTime = 0;
    
    // Seuraa kuinka kauan PC on ollut sammuneena
    if (!pcIsOn && powerState == POWER_IDLE) {
        if (pcOffStartTime == 0) {
            pcOffStartTime = now;
            Serial.println("PC sammui - uudelleenkäynnistys 2 tunnin kuluttua");
        }
        
        // JOS PC OLLUT SAMMUNEENA YLI 2 TUNTIA, KÄYNNISTÄ ESP32 UUDELLEEN
        if (now - pcOffStartTime >= 7200000) { // 2 tuntia = 7200000 ms
            Serial.println("=== PC ollut sammuneena 2 tuntia - ESP32 UUDELLEENKÄYNNISTYS ===");
            delay(1000);
            ESP.restart();
        }
    } else {
        // PC on päällä tai käynnistymässä - nollaa ajastin
        pcOffStartTime = 0;
    }
    
    // ================ POWER STATE DEBUG ================
    static unsigned long lastStatePrint = 0;
    static PowerState lastPowerState = POWER_IDLE;
    
    if (powerState != lastPowerState) {
        // Tila on vaihtunut, tulosta uusi tila
        Serial.print("STATE: ");
        switch(powerState) {
            case POWER_IDLE: Serial.print("IDLE"); break;
            case POWER_ON_START: Serial.print("ON_START"); break;
            case POWER_ON_WAITING_RELAY2: Serial.print("ON_WAITING_RELAY2"); break;
            case POWER_ON_COMPLETE: Serial.print("ON_COMPLETE"); break;
            case POWER_OFF_START: Serial.print("OFF_START"); break;
            case POWER_OFF_WAITING: Serial.print("OFF_WAITING"); break;
            case POWER_OFF_WAITING_POWEROFF: Serial.print("OFF_WAITING_POWEROFF"); break;
            case POWER_FORCE_START: Serial.print("FORCE_START"); break;
            case POWER_FORCE_WAITING: Serial.print("FORCE_WAITING"); break;
            default: Serial.print("UNKNOWN"); break;
        }
        Serial.print(" (pcIsOn=");
        Serial.print(pcIsOn ? "ON" : "OFF");
        Serial.print(", monitor=");
        Serial.print(digitalRead(PC_MONITOR_PIN) ? "HIGH" : "LOW");
        Serial.println(")");
        lastPowerState = powerState;
        lastStatePrint = now;
    }
    
    // Tulosta tila 60 sekunnin välein
    if (now - lastStatePrint >= 60000) {
        Serial.print("HEARTBEAT: ");
        Serial.print(millis() / 1000);
        Serial.print("s - State: ");
        switch(powerState) {
            case POWER_IDLE: 
                Serial.print("IDLE");
                if (!pcIsOn) {
                    Serial.print(" (uudelleenkäynnistys ");
                    Serial.print((7200000 - (now - pcOffStartTime)) / 1000);
                    Serial.print("s kuluttua)");
                }
                break;
            case POWER_ON_START: Serial.print("ON_START"); break;
            case POWER_ON_WAITING_RELAY2: Serial.print("ON_WAITING_RELAY2"); break;
            case POWER_ON_COMPLETE: Serial.print("ON_COMPLETE"); break;
            case POWER_OFF_START: Serial.print("OFF_START"); break;
            case POWER_OFF_WAITING: Serial.print("OFF_WAITING"); break;
            case POWER_OFF_WAITING_POWEROFF: Serial.print("OFF_WAITING_POWEROFF"); break;
            case POWER_FORCE_START: Serial.print("FORCE_START"); break;
            case POWER_FORCE_WAITING: Serial.print("FORCE_WAITING"); break;
            default: Serial.print("UNKNOWN"); break;
        }
        Serial.print(", PC: ");
        Serial.print(pcIsOn ? "ON" : "OFF");
        Serial.println();
        lastStatePrint = now;
    }

    // ================ PINNIN LUKU ================
    if (now - lastPinRead >= pinReadInterval) {
        cachedButtonState = digitalRead(BUTTON_PIN);
        lastPinRead = now;
    }

    // ================ PC:N TILAN KÄSITTELY ================
    if (now - lastPcStateHandle >= pcStateHandleInterval) {
        handlePcStates();
        lastPcStateHandle = now;
    }

    // ================ POWER-TILOJEN KÄSITTELY ================
    handlePowerStates();

    // ================ WEB-PALVELIN ================
    if (now - lastServerHandle >= serverHandleInterval) {
        server.handleClient();
        lastServerHandle = now;
    }
    
    // ================ BLUETOOTH CONTROLLER ================
    gamepadController.handle();
    
    // ================ PAINIKKEEN KÄSITTELY ================
    static unsigned long buttonPressStartTime = 0;
    static bool buttonPressDetected = false;
    static bool lastStableButtonState = HIGH;
    
    // Tarkista painikkeen tila debouncella
    if (cachedButtonState != lastStableButtonState) {
        lastButtonDebounce = now;
        lastStableButtonState = cachedButtonState;
    }
    
    // Jos tila on vakaa (debounce ohi)
    if ((now - lastButtonDebounce) > debounceDelay) {
        
        // Painike painettiin alas (LOW)
        if (cachedButtonState == LOW && !buttonPressDetected) {
            buttonPressDetected = true;
            buttonPressStartTime = now;
            Serial.println("BUTTON: Painike painettu alas");
        }
        
        // Painike vapautettiin (HIGH)
        if (cachedButtonState == HIGH && buttonPressDetected) {
            unsigned long pressDuration = now - buttonPressStartTime;
            buttonPressDetected = false;
            
            Serial.print("BUTTON: Painike vapautettu - kesto: ");
            Serial.print(pressDuration);
            Serial.println(" ms");
            
            // Tarkista PC:n tila (vain IDLE-tilassa)
            if (powerState == POWER_IDLE) {
                bool pcOn = getStablePcState();
                
                if (pcOn) {
                    // PC ON PÄÄLLÄ
                    if (pressDuration >= 5000) {
                        // Pitkä painallus (yli 5s) = PAKKOSAMMUTUS
                        Serial.println("BUTTON: Pitkä painallus (>5s) - PAKKOSAMMUTUS");
                        startForceShutdown();
                    } else {
                        // Lyhyt painallus (alle 5s) = NORMAALI SAMMUTUS
                        Serial.println("BUTTON: Lyhyt painallus (<5s) - NORMAALI SAMMUTUS");
                        startNormalShutdown();
                    }
                } else {
                    // PC ON SAMMUNUT
                    Serial.println("BUTTON: PC sammunut - KÄYNNISTYS");
                    startPowerOn();
                }
            } else {
                Serial.print("BUTTON: Power-tila ei IDLE - komento hylätty. Nykyinen tila: ");
                Serial.println(powerState);
            }
        }
    }
    
    // ================ PIENI VIIVE ================
    delay(1);
}
