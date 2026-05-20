#include "StatusHandler.h"
#include "../modules/GpsModule.h"
#include "../modules/CompassModule.h"
#include "../services/RotorService.h"
#include "../use_cases/GetRotorStatusUseCase.h"
#include "../pinout.h"
#include <Arduino.h>
#include <stdio.h>

size_t buildStatusJson(char* out, size_t outLen,
                       GpsModule* gps, CompassModule* compass, RotorService* rotor,
                       const char* prefix) {
    if (!out || outLen == 0 || !gps || !compass) return 0;

    const GpsData&     g = gps->getData();
    const CompassData& c = compass->getData();

    char lat[10], lon[10], alt[10], hdg[8];
    char dt[22];  // "YYYY-MM-DDTHH:MM:SSZ\0"

    dtostrf(g.latitude,  1, 2, lat);
    dtostrf(g.longitude, 1, 2, lon);
    dtostrf(g.altitude,  1, 1, alt);
    dtostrf(c.heading,   1, 1, hdg);

    snprintf(dt, sizeof(dt), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             (unsigned)g.year,   (unsigned)g.month,  (unsigned)g.day,
             (unsigned)g.hour,   (unsigned)g.minute,  (unsigned)g.second);

    char nav_az[8] = "0.0";
    char nav_el[8] = "0.0";
    if (rotor) {
        RotorStatus rs;
        GetRotorStatusUseCase getStatus(rotor);
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

    int n = snprintf(out, outLen,
        "{"
          "%s"
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
        prefix ? prefix : "",
        lat, lon, alt, dt, hdg,
        nav_az, nav_el,
        b0, b1, b2, b3, b4, b5, b6
    );

    if (n < 0 || (size_t)n >= outLen) return 0;
    return (size_t)n;
}
