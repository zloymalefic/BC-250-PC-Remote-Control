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

// ================ SETUP ================

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\n=== BC-250 STARTING ===");
    
    initPins();
    Serial.println("Pins initialized");
    
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
