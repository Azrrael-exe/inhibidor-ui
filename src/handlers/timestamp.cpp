#include "timestamp.h"
#include "../modules/GpsModule.h"
#include <string.h>
#include <stdio.h>

static GpsModule* s_gps = nullptr;

void initTimestampService(GpsModule* gps) {
    s_gps = gps;
}

bool injectTimestamp(char* body, size_t bodyCapacity) {
    if (!body || bodyCapacity == 0) return false;

    size_t len = strlen(body);
    if (len == 0 || body[len - 1] != '}') return false;

    char suffix[60];
    int n;

    if (s_gps) {
        const GpsData& d = s_gps->getData();
        bool tv = d.nmea_ok && d.year >= 2024;
        n = snprintf(suffix, sizeof(suffix),
                     ",\"timestamp\":\"%04u-%02u-%02uT%02u:%02u:%02uZ\",\"time_valid\":%s}",
                     (unsigned)d.year,   (unsigned)d.month,  (unsigned)d.day,
                     (unsigned)d.hour,   (unsigned)d.minute, (unsigned)d.second,
                     tv ? "true" : "false");
    } else {
        n = snprintf(suffix, sizeof(suffix),
                     ",\"timestamp\":\"0000-00-00T00:00:00Z\",\"time_valid\":false}");
    }

    if (n < 0 || (size_t)n >= sizeof(suffix)) return false;
    // Need: (len-1) + n + 1 (null) <= bodyCapacity  →  len + n <= bodyCapacity
    if (len + (size_t)n > bodyCapacity) return false;

    memcpy(body + len - 1, suffix, (size_t)n + 1);
    return true;
}
