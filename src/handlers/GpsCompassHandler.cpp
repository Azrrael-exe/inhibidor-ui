#include "GpsCompassHandler.h"
#include "../use_cases/GetRotorStatusUseCase.h"
#include "../services/ActivityWatchdog.h"
#include "../pinout.h"
#include <Arduino.h>
#include <stdio.h>

static GpsModule*        s_gps        = nullptr;
static CompassModule*    s_compass    = nullptr;
static RotorService*     s_rotor      = nullptr;
static ActivityWatchdog* s_watchdog   = nullptr;
static int               s_channelId  = -1;

void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor,
                       ActivityWatchdog* watchdog, int channelId) {
    s_gps       = gps;
    s_compass   = compass;
    s_rotor     = rotor;
    s_watchdog  = watchdog;
    s_channelId = channelId;
}

// GET /status
void handleGetStatus(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_gps || !s_compass) {
        res.json(503, "{\"error\":\"module not initialized\"}");
        return;
    }

    const GpsData&     g = s_gps->getData();
    const CompassData& c = s_compass->getData();

    char lat[14], lon[14], alt[10], hdg[8];
    char dt[22];  // "YYYY-MM-DDTHH:MM:SSZ\0"

    dtostrf(g.latitude,  1, 6, lat);
    dtostrf(g.longitude, 1, 6, lon);
    dtostrf(g.altitude,  1, 1, alt);
    dtostrf(c.heading,   1, 1, hdg);

    snprintf(dt, sizeof(dt), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             (unsigned)g.year,   (unsigned)g.month,  (unsigned)g.day,
             (unsigned)g.hour,   (unsigned)g.minute,  (unsigned)g.second);

    // Rotor status — query G5500; degrade to "0.0" on timeout or if not connected
    char nav_az[8] = "0.0";
    char nav_el[8] = "0.0";
    if (s_rotor) {
        RotorStatus rs;
        GetRotorStatusUseCase getStatus(s_rotor);
        if (getStatus.execute(rs)) {
            dtostrf(rs.azimuthAngle,   1, 1, nav_az);
            dtostrf(rs.elevationAngle, 1, 1, nav_el);
        }
    }

    const char* b0 = digitalRead(RF_BAND_0) ? "true" : "false";
    const char* b1 = digitalRead(RF_BAND_1) ? "true" : "false";
    const char* b2 = digitalRead(RF_BAND_2) ? "true" : "false";
    const char* b3 = digitalRead(RF_BAND_3) ? "true" : "false";
    const char* b4 = digitalRead(RF_BAND_4) ? "true" : "false";
    const char* b5 = digitalRead(RF_BAND_5) ? "true" : "false";
    const char* b6 = digitalRead(RF_BAND_6) ? "true" : "false";

    char body[400];
    snprintf(body, sizeof(body),
        "{"
          "\"gps\":{"
            "\"lat\":\"%s\","
            "\"lon\":\"%s\","
            "\"alt\":\"%s\","
            "\"datetime\":\"%s\""
          "},"
          "\"heading\":\"%s\","
          "\"navigation\":{"
            "\"azimuth\":\"%s\","
            "\"elevation\":\"%s\""
          "},"
          "\"power\":{"
            "\"band_0\":%s,"
            "\"band_1\":%s,"
            "\"band_2\":%s,"
            "\"band_3\":%s,"
            "\"band_4\":%s,"
            "\"band_5\":%s,"
            "\"band_6\":%s"
          "}"
        "}",
        lat, lon, alt, dt, hdg,
        nav_az, nav_el,
        b0, b1, b2, b3, b4, b5, b6
    );

    res.json(200, body);
}
