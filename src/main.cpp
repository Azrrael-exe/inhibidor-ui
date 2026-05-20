#include <Arduino.h>
#include <avr/wdt.h>

#include "logger.h"
#include "pinout.h"
#include <DigitalSwitch.h>
#include "callback.h"
#include <Controllino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <WebServer.h>
#include "handlers/GpsCompassHandler.h"
#include "handlers/NavigationHandler.h"
#include "handlers/HardStopHandler.h"
#include "handlers/HommingHandler.h"
#include "handlers/NetworkConfigHandler.h"
#include "handlers/WatchdogConfigHandler.h"
#include "handlers/timestamp.h"
#include "modules/GpsModule.h"
#include "modules/CompassModule.h"
#include "services/RotorService.h"
#include "services/NetworkConfig.h"
#include "services/ActivityWatchdog.h"
#include "services/RFOnTimeWatchdog.h"
#include "services/SerialConfigService.h"
#include "use_cases/SetNavigationAndPowerUseCase.h"
#include "use_cases/HardStopUseCase.h"
#include "use_cases/HommingUseCase.h"

// ─── Network configuration ────────────────────────────────────────────────────
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress fallbackIp(192, 168, 1, 100);

WebServer webServer(80);
SerialConfigService serialConfig;

GpsModule     gpsModule;
CompassModule compassModule;
RotorService  rotorService(&Serial2);  // _txPack + _rxPack = 364 bytes in .bss

SetNavigationAndPowerUseCase setNavAndPowerUseCase(&rotorService);
HardStopUseCase hardStopUseCase(&rotorService);
HommingUseCase  hommingUseCase(&rotorService);

static void onWatchdogTimeout(void* ctx) {
    static_cast<HommingUseCase*>(ctx)->execute();
}
ActivityWatchdog activityWatchdog(onWatchdogTimeout, &hommingUseCase);
int httpChannelId    = -1;
int controlChannelId = -1;

static void onRFOnTimeExpired(void* ctx) {
    static_cast<HommingUseCase*>(ctx)->execute();
}
RFOnTimeWatchdog rfOnTimeWatchdog(onRFOnTimeExpired, &hommingUseCase, 5UL * 60UL * 1000UL);

DigitalSwitch azimuthForwardSwitch(AZIMUTH_FORWARD_PIN);
DigitalSwitch azimuthBackwardSwitch(AZIMUTH_BACKWARD_PIN);
DigitalSwitch elevationForwardSwitch(ELEVATION_FORWARD_PIN);
DigitalSwitch elevationBackwardSwitch(ELEVATION_BACKWARD_PIN);
DigitalSwitch rfPowerSwitch(RF_POWER_PIN, 50);
DigitalSwitch hommingSwitch(HOMMING_SWITCH_PIN);

G5500CommandContext azimuthForwardContext   = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_FORWARD,   &rotorService };
G5500CommandContext azimuthBackwardContext  = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_BACKWARD,  &rotorService };
G5500CommandContext azimuthStopContext      = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_STOP,      &rotorService };
G5500CommandContext elevationForwardContext  = { &Serial2, ELEVATION_HEADER, ELEVATION_FORWARD,  &rotorService };
G5500CommandContext elevationBackwardContext = { &Serial2, ELEVATION_HEADER, ELEVATION_BACKWARD, &rotorService };
G5500CommandContext elevationStopContext     = { &Serial2, ELEVATION_HEADER, ELEVATION_STOP,     &rotorService };


void activateRFPower(void* context) {
    if (digitalRead(RF_POWER_PIN) != HIGH) return;
    // LOG("RFPower", "Activating RF power via physical switch");
    int8_t bands[7] = { 1, 1, 1, 1, 1, 1, 1 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void deactivateRFPower(void* context) {
    if (digitalRead(RF_POWER_PIN) != LOW) return;
    // LOG("RFPower", "Deactivating RF power via physical switch");
    int8_t bands[7] = { 0, 0, 0, 0, 0, 0, 0 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void triggerHomming(void* context) {
    // LOG("Homming", "Triggering homming via physical switch or watchdog timeout");
    hommingUseCase.execute();
}

void setup() {
    // Disable watchdog promptly after a WDT-induced reset (used by NetworkConfig::reboot).
    wdt_disable();

    Serial.begin(115200);
    Serial2.begin(115200);
    gpsModule.begin(Serial1, 38400);

    NetConfig netCfg;
    bool loaded = NetworkConfig::load(netCfg);

    if (loaded && netCfg.useEepromConfig) {
        Ethernet.begin(mac,
                       IPAddress((netCfg.ip      >> 24) & 0xFF, (netCfg.ip      >> 16) & 0xFF,
                                 (netCfg.ip      >>  8) & 0xFF,  netCfg.ip      & 0xFF),
                       IPAddress(0, 0, 0, 0),
                       IPAddress((netCfg.gateway >> 24) & 0xFF, (netCfg.gateway >> 16) & 0xFF,
                                 (netCfg.gateway >>  8) & 0xFF,  netCfg.gateway & 0xFF),
                       IPAddress((netCfg.subnet  >> 24) & 0xFF, (netCfg.subnet  >> 16) & 0xFF,
                                 (netCfg.subnet  >>  8) & 0xFF,  netCfg.subnet  & 0xFF));
        LOG_F("WebServer", "Static IP from EEPROM: ", Ethernet.localIP());
    } else if (loaded) {
        // Usuario eligió DHCP explícitamente vía API.
        if (Ethernet.begin(mac) == 0) {
            Ethernet.begin(mac, fallbackIp);
            LOG_F("WebServer", "DHCP failed, using static fallback: ", Ethernet.localIP());
        } else {
            LOG_F("WebServer", "DHCP OK, IP: ", Ethernet.localIP());
        }
    } else {
        // EEPROM virgen/corrupta: estática 192.168.1.100, sin intento de DHCP.
        Ethernet.begin(mac, fallbackIp);
        LOG_F("WebServer", "Cold boot, using factory static IP: ", Ethernet.localIP());
    }
    delay(1000);

    Logger::init(&rotorService);
    serialConfig.begin(&Serial, mac);

    compassModule.begin();

    httpChannelId    = activityWatchdog.registerChannel("http",    60000UL);
    controlChannelId = activityWatchdog.registerChannel("control", 60000UL);

    setNavAndPowerUseCase.setRFOnTimeWatchdog(&rfOnTimeWatchdog);
    hardStopUseCase.setRFOnTimeWatchdog(&rfOnTimeWatchdog);
    hommingUseCase.setRFOnTimeWatchdog(&rfOnTimeWatchdog);

    initStatusHandler(&gpsModule, &compassModule, &rotorService, &activityWatchdog, httpChannelId);
    initNavigationHandler(&setNavAndPowerUseCase, &gpsModule, &compassModule, &rotorService,
                          &activityWatchdog, httpChannelId);
    initHardStopHandler(&hardStopUseCase, &activityWatchdog, httpChannelId);
    initHommingHandler(&hommingUseCase, &activityWatchdog, httpChannelId);
    initNetworkConfigHandler(mac, &activityWatchdog, httpChannelId);
    initWatchdogConfigHandler(&rfOnTimeWatchdog, &activityWatchdog, httpChannelId, controlChannelId);
    initTimestampService(&gpsModule);

    webServer.begin();
    bool routesOk = true;
    routesOk &= webServer.on("/status",                      HTTP_GET,  handleGetStatus);
    routesOk &= webServer.on("/set-navigation-and-power",    HTTP_POST, handleSetNavigationAndPower);
    routesOk &= webServer.on("/hard-stop",                   HTTP_POST, handleHardStop);
    routesOk &= webServer.on("/homming",                     HTTP_POST, handleHomming);
    routesOk &= webServer.on("/config/network",  HTTP_GET,  handleGetNetworkConfig);
    routesOk &= webServer.on("/config/network",  HTTP_POST, handleSetNetworkConfig);
    routesOk &= webServer.on("/config/watchdog", HTTP_GET,  handleGetWatchdogConfig);
    routesOk &= webServer.on("/config/watchdog", HTTP_POST, handleSetWatchdogConfig);
    if (!routesOk) {
        Serial.println(F("[WebServer] ERROR: route table full — increase WS_MAX_ROUTES"));
    }

    pinMode(RF_BAND_0, OUTPUT);
    pinMode(RF_BAND_1, OUTPUT);
    pinMode(RF_BAND_2, OUTPUT);
    pinMode(RF_BAND_3, OUTPUT);
    pinMode(RF_BAND_4, OUTPUT);
    pinMode(RF_BAND_5, OUTPUT);
    pinMode(RF_BAND_6, OUTPUT);

    pinMode(RED_LED_PIN,         OUTPUT);
    pinMode(ACTIVITY_BUZZER_PIN, OUTPUT);
    digitalWrite(RED_LED_PIN,         LOW);
    digitalWrite(ACTIVITY_BUZZER_PIN, LOW);

    // ─── Railguard: Ensure all RF bands are OFF at startup ─────────────────────
    deactivateRFPower(nullptr);

    // CONTROLLINO MAXI industrial inputs are active-HIGH (0–24V).
    // RF switch is connected to GND (active-LOW): INPUT_PULLUP keeps it HIGH when unpressed.
    azimuthForwardSwitch.begin(INPUT);
    azimuthBackwardSwitch.begin(INPUT);
    elevationForwardSwitch.begin(INPUT);
    elevationBackwardSwitch.begin(INPUT);
    rfPowerSwitch.begin(INPUT_PULLUP);
    hommingSwitch.begin(INPUT);

    azimuthForwardSwitch.setOnTurnOn(sendG5500Command, &azimuthForwardContext);
    azimuthBackwardSwitch.setOnTurnOn(sendG5500Command, &azimuthBackwardContext);
    azimuthForwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);
    azimuthBackwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);

    elevationForwardSwitch.setOnTurnOn(sendG5500Command, &elevationForwardContext);
    elevationBackwardSwitch.setOnTurnOn(sendG5500Command, &elevationBackwardContext);
    elevationForwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);
    elevationBackwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);

    // RF switch logic is inverted (connected to GND): LOW=pressed=activate, HIGH=released=deactivate
    rfPowerSwitch.setOnTurnOn(activateRFPower);   // HIGH (released)
    rfPowerSwitch.setOnTurnOff(deactivateRFPower);    // LOW (pressed)

    hommingSwitch.setOnTurnOn(triggerHomming);      // HIGH (pressed) → Homming

    // Re-sync switch state AFTER all initialization delays
    // to prevent false edge detection on first loop() iteration
    rfPowerSwitch.sync();
    hommingSwitch.sync();

    // Grace period: avoid a spurious trip before the first activity arrives.
    activityWatchdog.feed(httpChannelId);
    activityWatchdog.feed(controlChannelId);

    // Hardware WDT: reset if loop() ever blocks for > 8 seconds.
    // Kicked at the top of every loop() iteration via wdt_reset().
    wdt_enable(WDTO_8S);
}

void loop() {
    wdt_reset();

    webServer.update();
    serialConfig.update();

    // Deferred reboot after successful POST /set-network-config — runs only after
    // WebServer::update() has dispatched the response and called _client.stop(),
    // so the client gets a clean FIN before the watchdog reset fires.
    if (isNetworkConfigRebootPending()) {
        NetworkConfig::reboot();
    }

    gpsModule.update();
    compassModule.update();

    rotorService.update();

    azimuthForwardSwitch.update();
    azimuthBackwardSwitch.update();
    elevationForwardSwitch.update();
    elevationBackwardSwitch.update();
    rfPowerSwitch.update();
    hommingSwitch.update();

    bool controlActive = azimuthForwardSwitch.getState()
                      || azimuthBackwardSwitch.getState()
                      || elevationForwardSwitch.getState()
                      || elevationBackwardSwitch.getState()
                      || !rfPowerSwitch.getState();   // RF switch is active-LOW
    if (controlActive) activityWatchdog.feed(controlChannelId);

    Ethernet.maintain();

    rfOnTimeWatchdog.update();
    activityWatchdog.update();

    // Mirror "any RF band ON" → red LED + buzzer. Edge-detected to avoid
    // redundant digitalWrite in every loop iteration.
    static bool prevRfActive = false;
    bool rfActive = rfOnTimeWatchdog.isAnyOn();
    if (rfActive != prevRfActive) {
        digitalWrite(RED_LED_PIN,         rfActive ? HIGH : LOW);
        digitalWrite(ACTIVITY_BUZZER_PIN, rfActive ? HIGH : LOW);
        prevRfActive = rfActive;
    }
}
