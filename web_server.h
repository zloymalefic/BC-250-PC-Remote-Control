#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include <Update.h>
#include "LittleFS.h"
#include <ArduinoJson.h>
#include "pc_control.h"
#include "version.h"
#include "pins.h"

extern WebServer server;
extern bool pcIsOn;
extern bool shutdownRequested;
extern bool forceShutdown;
extern PowerState powerState;
extern unsigned long powerStateStartTime;
extern String wifiSSID;
extern String wifiPassword;
extern bool wifiConfigured;
extern bool apMode;
extern ControllerConfig controllerConfig;
extern GamepadController gamepadController;
extern LedState ledState;

// Funktioprototyypit
void saveWiFiConfig(String ssid, String pass);
bool getStablePcState();
void startPowerOn();
void startNormalShutdown();
void startForceShutdown();
bool saveControllerConfig();
bool updateControllerConfigFromJson(JsonObjectConst root, String& error);
bool saveLedState();
bool updateLedStateFromJson(JsonObjectConst root, String& error);

String indexHtml = "";
String updateHtml = "";
String setupHtml = "";
String ledHtml = "";
String styleCss = "";
bool filesLoaded = false;

void loadFiles() {
    if (filesLoaded) return;
    
    Serial.println("Loading web files from LittleFS...");
    
    if (LittleFS.exists("/index.html")) {
        File file = LittleFS.open("/index.html", "r");
        indexHtml = file.readString();
        file.close();
        Serial.println("  - index.html loaded");
    } else {
        Serial.println("  - index.html NOT FOUND!");
    }
    
    if (LittleFS.exists("/update.html")) {
        File file = LittleFS.open("/update.html", "r");
        updateHtml = file.readString();
        file.close();
        Serial.println("  - update.html loaded");
    } else {
        Serial.println("  - update.html NOT FOUND!");
    }
    
    if (LittleFS.exists("/setup.html")) {
        File file = LittleFS.open("/setup.html", "r");
        setupHtml = file.readString();
        file.close();
        Serial.println("  - setup.html loaded");
    } else {
        Serial.println("  - setup.html NOT FOUND!");
    }

    if (LittleFS.exists("/led.html")) {
        File file = LittleFS.open("/led.html", "r");
        ledHtml = file.readString();
        file.close();
        Serial.println("  - led.html loaded");
    } else {
        Serial.println("  - led.html NOT FOUND!");
    }
    
    if (LittleFS.exists("/style.css")) {
        File file = LittleFS.open("/style.css", "r");
        styleCss = file.readString();
        file.close();
        Serial.println("  - style.css loaded");
    } else {
        Serial.println("  - style.css NOT FOUND!");
    }
    
    filesLoaded = true;
}

void setupWebServer() {
    loadFiles();
    
    Serial.println("Setting up web server routes...");
    
    // ========== TÄRKEÄÄ: POST-reitit ENNEN GET-reittejä! ==========
    
    // FIRMWARE UPDATE - POST (tämä käsittelee tiedoston lähetyksen)
    server.on("/update", HTTP_POST, []() {
        // Tämä suoritetaan kun upload on valmis
        if (Update.hasError()) {
            server.send(500, "text/plain", "Update failed!");
            Serial.println("Update failed!");
        } else {
            server.send(200, "text/plain", "OK");
            Serial.println("Update successful! Rebooting...");
            delay(1000);
            ESP.restart();
        }
    }, []() {
        // Tämä suoritetaan uploadin aikana
        HTTPUpload& upload = server.upload();
        
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update Start: %s (size: %u bytes)\n", 
                         upload.filename.c_str(), upload.totalSize);
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
            // Tulosta edistyminen joka 100kB välein
            static unsigned int lastProgress = 0;
            unsigned int progress = (Update.progress() * 100) / Update.size();
            if (progress / 10 != lastProgress / 10) {
                Serial.printf("Progress: %u%%\n", progress);
                lastProgress = progress;
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("Update Success: %u bytes\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    });
    
    // FIRMWARE UPDATE - GET (näyttää web-sivun)
    server.on("/update", HTTP_GET, []() {
        server.send(200, "text/html", updateHtml);
    });
    
    // Etusivu
    server.on("/", []() {
        server.send(200, "text/html", indexHtml);
    });
    
    // Setup-sivu
    server.on("/setup", []() {
        server.send(200, "text/html", setupHtml);
    });

    // LED lighting page
    server.on("/led", []() {
        server.send(200, "text/html", ledHtml);
    });
    
    // CSS-tyylit
    server.on("/style.css", []() {
        server.send(200, "text/css", styleCss);
    });

    // SVG-logo
    server.on("/steam-machines.svg", []() {
        if (LittleFS.exists("/steam-machines.svg")) {
            File file = LittleFS.open("/steam-machines.svg", "r");
            server.streamFile(file, "image/svg+xml");
            file.close();
        } else {
            server.send(200, "image/svg+xml", 
                "<svg width='180' height='50' xmlns='http://www.w3.org/2000/svg'>"
                "<text x='10' y='35' font-family='Share Tech Mono' font-size='24' fill='#00d9ff'>BC-250</text>"
                "</svg>");
        }
    });

    // API: Bluetooth MAC-osoite
    server.on("/api/bluetooth/mac", HTTP_GET, []() {
        String btMac = "";
        const uint8_t* addr = BP32.localBdAddress();
        if (addr != nullptr) {
            char macStr[18];
            sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
                    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            btMac = String(macStr);
        }
        
        StaticJsonDocument<100> doc;
        doc["macAddress"] = btMac;
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    // API: WiFi-asetukset - GET
    server.on("/api/wifi/config", HTTP_GET, []() {
        StaticJsonDocument<200> doc;
        doc["ssid"] = wifiSSID;
        doc["configured"] = wifiConfigured;
        doc["apMode"] = apMode;
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    
    // API: WiFi-asetukset - POST
    server.on("/api/wifi/config", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Body missing");
            return;
        }
        
        String body = server.arg("plain");
        
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        const char* ssid = doc["ssid"];
        const char* password = doc["password"];
        
        if (!ssid || strlen(ssid) == 0) {
            server.send(400, "text/plain", "SSID required");
            return;
        }
        
        String passStr = password ? String(password) : "";
        
        saveWiFiConfig(String(ssid), passStr);
        server.send(200, "text/plain", "OK");
        delay(1000);
        ESP.restart();
    });
    
    // API: WiFi skannaus
    server.on("/api/wifi/scan", HTTP_GET, []() {
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true);
            StaticJsonDocument<100> doc;
            doc["scanning"] = true;
            
            String response;
            serializeJson(doc, response);
            server.send(200, "application/json", response);
        } else if (n >= 0) {
            StaticJsonDocument<2000> doc;
            JsonArray networks = doc.to<JsonArray>();
            
            for (int i = 0; i < n; ++i) {
                JsonObject net = networks.createNestedObject();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["encryption"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? 1 : 0;
            }
            
            String response;
            serializeJson(doc, response);
            WiFi.scanDelete();
            server.send(200, "application/json", response);
        }
    });

    // API: LED lighting state
    server.on("/api/led/state", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["on"] = ledState.on;
        doc["bri"] = ledState.brightness;
        JsonArray rgb = doc.createNestedArray("rgb");
        rgb.add(ledState.red);
        rgb.add(ledState.green);
        rgb.add(ledState.blue);

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/led/state", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Body missing");
            return;
        }

        StaticJsonDocument<256> doc;
        DeserializationError parseError = deserializeJson(doc, server.arg("plain"));
        if (parseError || !doc.is<JsonObject>()) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }

        String validationError;
        if (!updateLedStateFromJson(doc.as<JsonObjectConst>(), validationError)) {
            server.send(400, "text/plain", validationError);
            return;
        }

        if (!saveLedState()) {
            server.send(500, "text/plain", "Failed to save LED state");
            return;
        }

        server.send(200, "text/plain", "OK");
    });

    // API: Status
    server.on("/api/status", HTTP_GET, []() {
        bool currentMonitor = getStablePcState();
        bool currentOpto = digitalRead(OPTO_PIN);
        bool currentExtra = digitalRead(EXTRA_PIN);
        
        StaticJsonDocument<300> doc;
        doc["pcOn"] = currentMonitor;
        doc["shutdownRequested"] = shutdownRequested;
        doc["forceShutdown"] = forceShutdown;
        doc["optoState"] = currentOpto;
        doc["extraPinState"] = currentExtra;
        doc["monitorState"] = currentMonitor;
        doc["version"] = VERSION;
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    // API: Power ON
    server.on("/power/on", HTTP_POST, []() {
        Serial.println("API: Power ON requested");
        if (getStablePcState() == LOW) {
            startPowerOn();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(200, "text/plain", "Already on");
        }
    });

    // API: Power OFF (pakkosammutus)
    server.on("/power/off", HTTP_POST, []() {
        Serial.println("API: Power OFF requested");
        if (getStablePcState() == HIGH) {
            startForceShutdown();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(200, "text/plain", "Already off");
        }
    });

    // API: Force shutdown
    server.on("/power/force", HTTP_POST, []() {
        Serial.println("API: Force shutdown requested");
        if (getStablePcState() == HIGH) {
            startForceShutdown();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(200, "text/plain", "Already off");
        }
    });

    // API: Controller configuration
    server.on("/api/controllers/config", HTTP_GET, []() {
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

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/controllers/config", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Body missing");
            return;
        }

        StaticJsonDocument<1536> doc;
        DeserializationError parseError = deserializeJson(doc, server.arg("plain"));
        if (parseError || !doc.is<JsonObject>()) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }

        String validationError;
        if (!updateControllerConfigFromJson(doc.as<JsonObjectConst>(), validationError)) {
            server.send(400, "text/plain", validationError);
            return;
        }
        if (!saveControllerConfig()) {
            server.send(500, "text/plain", "Failed to save controller config");
            return;
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/controllers/status", HTTP_GET, []() {
        StaticJsonDocument<768> doc;
        doc["state"] = !controllerConfig.enabled ? "disabled" :
                       (gamepadController.isConnected() ? "connected" : "disconnected");
        doc["bluetoothRunning"] = gamepadController.isBluetoothRunning();
        doc["btAllowed"] = !getStablePcState() && powerState == POWER_IDLE;
        doc["connected"] = gamepadController.isConnected();
        doc["authorized"] = gamepadController.isConnectedControllerAuthorized();
        doc["bluepad32Firmware"] = BP32.firmwareVersion();

        if (gamepadController.isConnected()) {
            GamepadIdentity identity = gamepadController.getActiveIdentity();
            JsonObject active = doc.createNestedObject("active");
            active["macAddress"] = identity.macAddress;
            active["modelName"] = identity.modelName;
            active["vendorId"] = identity.vendorId;
            active["productId"] = identity.productId;
        }

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/controllers/discovered", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["available"] = gamepadController.hasLastSeenIdentity();
        if (gamepadController.hasLastSeenIdentity()) {
            GamepadIdentity identity = gamepadController.getLastSeenIdentity();
            doc["macAddress"] = identity.macAddress;
            doc["modelName"] = identity.modelName;
            doc["vendorId"] = identity.vendorId;
            doc["productId"] = identity.productId;
        }

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    server.on("/api/controllers/enroll", HTTP_POST, []() {
        String label;
        if (server.hasArg("plain") && server.arg("plain").length() > 0) {
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, server.arg("plain"))) {
                server.send(400, "text/plain", "Invalid JSON");
                return;
            }
            label = String(doc["label"] | "");
            label.trim();
            if (label.length() > 48) {
                server.send(400, "text/plain", "Label is too long");
                return;
            }
        }

        String error;
        if (!gamepadController.enrollLastSeen(label, error)) {
            server.send(409, "text/plain", error);
            return;
        }
        if (!saveControllerConfig()) {
            server.send(500, "text/plain", "Failed to save controller config");
            return;
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/api/controllers/remove", HTTP_POST, []() {
        if (!server.hasArg("plain")) {
            server.send(400, "text/plain", "Body missing");
            return;
        }
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, server.arg("plain"))) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }
        String macAddress = normalizeControllerMac(String(doc["macAddress"] | ""));
        if (!isValidControllerMac(macAddress)) {
            server.send(400, "text/plain", "Invalid controller MAC address");
            return;
        }
        if (!gamepadController.removeAuthorized(macAddress)) {
            server.send(404, "text/plain", "Controller not found");
            return;
        }
        if (!saveControllerConfig()) {
            server.send(500, "text/plain", "Failed to save controller config");
            return;
        }
        server.send(200, "text/plain", "OK");
    });

    // LittleFS OTA update
    server.on("/update-fs", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        delay(1000);
        ESP.restart();
    }, []() {
        HTTPUpload& upload = server.upload();

        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("LittleFS Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) {
                Serial.printf("LittleFS Update Success: %u bytes\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
    });

    // 404 - Not found
    server.onNotFound([]() {
        server.send(404, "text/plain", "404: Not Found");
    });

    server.begin();
    Serial.println("Web server started!");
    Serial.print("  - IP: ");
    Serial.println(WiFi.localIP());
}

#endif // WEB_SERVER_H
