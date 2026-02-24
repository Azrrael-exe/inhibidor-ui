#include <Arduino.h>

#include "pinout.h"
#include <DigitalSwitch.h>
#include "callback.h"
#include <Controllino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <WebServer.h>
#include "handlers/GpsCompassHandler.h"
#include "handlers/PowerHandler.h"
#include "modules/GpsModule.h"
#include "modules/CompassModule.h"

// ─── Network configuration ────────────────────────────────────────────────────
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress fallbackIp(192, 168, 1, 100);  // usado si DHCP falla

WebServer webServer(80);

GpsModule     gpsModule;
CompassModule compassModule;

DigitalSwitch azimuthForwardSwitch(AZIMUTH_FORWARD_PIN);
DigitalSwitch azimuthBackwardSwitch(AZIMUTH_BACKWARD_PIN);
DigitalSwitch elevationForwardSwitch(ELEVATION_FORWARD_PIN);
DigitalSwitch elevationBackwardSwitch(ELEVATION_BACKWARD_PIN);
DigitalSwitch rfPowerSwitch(RF_POWER_PIN);

G5500CommandContext azimuthForwardContext = {
  &Serial,
  AZIMUTH_HEADER,
  AZIMUTH_FORWARD
};
G5500CommandContext azimuthBackwardContext = {
  &Serial,
  AZIMUTH_HEADER,
  AZIMUTH_BACKWARD
};
G5500CommandContext azimuthStopContext = {
  &Serial,
  AZIMUTH_HEADER,
  AZIMUTH_STOP
};
G5500CommandContext elevationForwardContext = {
  &Serial,
  ELEVATION_HEADER,
  ELEVATION_FORWARD
};
G5500CommandContext elevationBackwardContext = {
  &Serial,
  ELEVATION_HEADER,
  ELEVATION_BACKWARD
};
G5500CommandContext elevationStopContext = {
  &Serial,
  ELEVATION_HEADER,
  ELEVATION_STOP
};

void activateRFPower(void* context) {
  digitalWrite(RF_BAND_0, HIGH);
  digitalWrite(RF_BAND_1, HIGH);
  digitalWrite(RF_BAND_2, HIGH);
  digitalWrite(RF_BAND_3, HIGH);
  digitalWrite(RF_BAND_4, HIGH);
  digitalWrite(RF_BAND_5, HIGH);
  digitalWrite(RF_BAND_6, HIGH);
}

void deactivateRFPower(void* context) {
  digitalWrite(RF_BAND_0, LOW);
  digitalWrite(RF_BAND_1, LOW);
  digitalWrite(RF_BAND_2, LOW);
  digitalWrite(RF_BAND_3, LOW);
  digitalWrite(RF_BAND_4, LOW);
  digitalWrite(RF_BAND_5, LOW);
  digitalWrite(RF_BAND_6, LOW);
}


void setup() {
  Serial.begin(115200);
  gpsModule.begin(Serial1, 38400);

  if (Ethernet.begin(mac) == 0) {
    Ethernet.begin(mac, fallbackIp);
    Serial.print(F("[WebServer] DHCP failed, using static IP: "));
  } else {
    Serial.print(F("[WebServer] DHCP OK, IP: "));
  }
  Serial.println(Ethernet.localIP());
  delay(1000);
  compassModule.begin();
  initStatusHandler(&gpsModule, &compassModule);

  webServer.begin();
  webServer.on("/status",    HTTP_GET,  handleGetStatus);
  webServer.on("/set-power", HTTP_POST, handlePostSetPower);

  pinMode(RF_BAND_0, OUTPUT);
  pinMode(RF_BAND_1, OUTPUT);
  pinMode(RF_BAND_2, OUTPUT);
  pinMode(RF_BAND_3, OUTPUT);
  pinMode(RF_BAND_4, OUTPUT);
  pinMode(RF_BAND_5, OUTPUT);
  pinMode(RF_BAND_6, OUTPUT);

  azimuthForwardSwitch.begin();
  azimuthBackwardSwitch.begin();
  elevationForwardSwitch.begin();
  elevationBackwardSwitch.begin();
  rfPowerSwitch.begin();

  azimuthForwardSwitch.setOnTurnOn(sendG5500Command, &azimuthForwardContext);
  azimuthBackwardSwitch.setOnTurnOn(sendG5500Command, &azimuthBackwardContext);
  azimuthForwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);
  azimuthBackwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);

  elevationForwardSwitch.setOnTurnOn(sendG5500Command, &elevationForwardContext);
  elevationBackwardSwitch.setOnTurnOn(sendG5500Command, &elevationBackwardContext);
  elevationForwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);
  elevationBackwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);

  rfPowerSwitch.setOnTurnOn(activateRFPower);
  rfPowerSwitch.setOnTurnOff(deactivateRFPower);
}

void loop() {
  webServer.update();

  gpsModule.update();
  compassModule.update();

  // Update all switches
  azimuthForwardSwitch.update();
  azimuthBackwardSwitch.update();
  elevationForwardSwitch.update();
  elevationBackwardSwitch.update();
  rfPowerSwitch.update();
}
