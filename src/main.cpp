#include <Arduino.h>

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
#include "modules/GpsModule.h"
#include "modules/CompassModule.h"
#include "services/RotorService.h"
#include "use_cases/SetNavigationAndPowerUseCase.h"
#include "use_cases/HardStopUseCase.h"

// ─── Network configuration ────────────────────────────────────────────────────
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress fallbackIp(192, 168, 1, 100);

WebServer webServer(80);

GpsModule     gpsModule;
CompassModule compassModule;
RotorService  rotorService(&Serial2);  // _txPack + _rxPack = 364 bytes in .bss

SetNavigationAndPowerUseCase setNavAndPowerUseCase(&rotorService);
HardStopUseCase hardStopUseCase(&rotorService);

DigitalSwitch azimuthForwardSwitch(AZIMUTH_FORWARD_PIN);
DigitalSwitch azimuthBackwardSwitch(AZIMUTH_BACKWARD_PIN);
DigitalSwitch elevationForwardSwitch(ELEVATION_FORWARD_PIN);
DigitalSwitch elevationBackwardSwitch(ELEVATION_BACKWARD_PIN);
DigitalSwitch rfPowerSwitch(RF_POWER_PIN);

G5500CommandContext azimuthForwardContext   = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_FORWARD,   &rotorService };
G5500CommandContext azimuthBackwardContext  = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_BACKWARD,  &rotorService };
G5500CommandContext azimuthStopContext      = { &Serial2, AZIMUTH_HEADER,   AZIMUTH_STOP,      &rotorService };
G5500CommandContext elevationForwardContext  = { &Serial2, ELEVATION_HEADER, ELEVATION_FORWARD,  &rotorService };
G5500CommandContext elevationBackwardContext = { &Serial2, ELEVATION_HEADER, ELEVATION_BACKWARD, &rotorService };
G5500CommandContext elevationStopContext     = { &Serial2, ELEVATION_HEADER, ELEVATION_STOP,     &rotorService };

void activateRFPower(void* context) {
    int8_t bands[7] = { 1, 1, 1, 1, 1, 1, 1 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void deactivateRFPower(void* context) {
    int8_t bands[7] = { 0, 0, 0, 0, 0, 0, 0 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200);
    gpsModule.begin(Serial1, 38400);

    if (Ethernet.begin(mac) == 0) {
        Ethernet.begin(mac, fallbackIp);
        LOG_F("WebServer", "DHCP failed, using static IP: ", Ethernet.localIP());
    } else {
        LOG_F("WebServer", "DHCP OK, IP: ", Ethernet.localIP());
    }
    delay(1000);

    Logger::init(&rotorService);

    compassModule.begin();
    initStatusHandler(&gpsModule, &compassModule, &rotorService);
    initNavigationHandler(&setNavAndPowerUseCase);
    initHardStopHandler(&hardStopUseCase);

    webServer.begin();
    webServer.on("/status",                   HTTP_GET,  handleGetStatus);
    webServer.on("/set-navigation-and-power", HTTP_POST, handleSetNavigationAndPower);
    webServer.on("/hard-stop",                HTTP_POST, handleHardStop);

    pinMode(RF_BAND_0, OUTPUT);
    pinMode(RF_BAND_1, OUTPUT);
    pinMode(RF_BAND_2, OUTPUT);
    pinMode(RF_BAND_3, OUTPUT);
    pinMode(RF_BAND_4, OUTPUT);
    pinMode(RF_BAND_5, OUTPUT);
    pinMode(RF_BAND_6, OUTPUT);

    // ─── Railguard: Ensure all RF bands are OFF at startup ─────────────────────
    deactivateRFPower(nullptr);

    // CONTROLLINO MAXI industrial inputs are active-HIGH (0–24V).
    // RF switch is connected to GND (active-LOW): INPUT_PULLUP keeps it HIGH when unpressed.
    azimuthForwardSwitch.begin(INPUT);
    azimuthBackwardSwitch.begin(INPUT);
    elevationForwardSwitch.begin(INPUT);
    elevationBackwardSwitch.begin(INPUT);
    rfPowerSwitch.begin(INPUT_PULLUP);

    azimuthForwardSwitch.setOnTurnOn(sendG5500Command, &azimuthForwardContext);
    azimuthBackwardSwitch.setOnTurnOn(sendG5500Command, &azimuthBackwardContext);
    azimuthForwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);
    azimuthBackwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);

    elevationForwardSwitch.setOnTurnOn(sendG5500Command, &elevationForwardContext);
    elevationBackwardSwitch.setOnTurnOn(sendG5500Command, &elevationBackwardContext);
    elevationForwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);
    elevationBackwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);

    // RF switch logic is inverted (connected to GND): LOW=pressed=activate, HIGH=released=deactivate
    rfPowerSwitch.setOnTurnOn(deactivateRFPower);   // HIGH (released)
    rfPowerSwitch.setOnTurnOff(activateRFPower);    // LOW (pressed)

    // Re-sync switch state AFTER all initialization delays
    // to prevent false edge detection on first loop() iteration
    rfPowerSwitch.sync();
}

void loop() {
    webServer.update();

    gpsModule.update();
    compassModule.update();

    rotorService.update();

    azimuthForwardSwitch.update();
    azimuthBackwardSwitch.update();
    elevationForwardSwitch.update();
    elevationBackwardSwitch.update();
    rfPowerSwitch.update();
}
