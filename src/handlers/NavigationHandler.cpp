#include "NavigationHandler.h"
#include "StatusHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
#include <Arduino.h>
#include <stdio.h>

static SetNavigationAndPowerUseCase* s_useCase    = nullptr;
static GpsModule*                    s_gps        = nullptr;
static CompassModule*                s_compass    = nullptr;
static RotorService*                 s_rotor      = nullptr;
static ActivityWatchdog*             s_watchdog   = nullptr;
static int                           s_channelId  = -1;

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase,
                           GpsModule* gps, CompassModule* compass, RotorService* rotor,
                           ActivityWatchdog* watchdog, int channelId) {
    s_useCase   = useCase;
    s_gps       = gps;
    s_compass   = compass;
    s_rotor     = rotor;
    s_watchdog  = watchdog;
    s_channelId = channelId;
}

// POST /set-navigation-and-power
// Body (all fields optional):
//   { "azimuth": 180.0, "elevation": 45.0, "band_0": true, ..., "band_6": false,
//     "request_id": "abc123" }
// Omitted fields leave current state unchanged.
// Response includes the same payload as GET /status, prefixed with "status":"queued"
// and (if a valid request_id was provided) suffixed with "request_id":"<id>".
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_useCase) {
        char errBody[128]; strcpy_P(errBody, PSTR("{\"error\":\"rotor not available\"}"));
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    const char* body = req.params;

    // Validate request_id BEFORE queueing the command — never execute on a malformed ID.
    char rid[40];
    int ridState = extractRequestId(body, /*isQueryString=*/false, rid, sizeof(rid));
    if (ridState < 0) {
        char errBody[128]; strcpy_P(errBody, PSTR("{\"error\":\"invalid request_id\"}"));
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    bool  hasAz = jsonHasKey(body, F("azimuth"));
    bool  hasEl = jsonHasKey(body, F("elevation"));
    float az    = hasAz ? jsonGetFloat(body, F("azimuth"),   0.0f) : 0.0f;
    float el    = hasEl ? jsonGetFloat(body, F("elevation"), 0.0f) : 0.0f;

    // -1 = absent (don't touch pin), 0 = LOW, 1 = HIGH
    // Unrolled so every key literal lives in Flash via F().
    int8_t bands[7];
    bands[0] = (int8_t)jsonGetBool(body, F("band_0"), -1);
    bands[1] = (int8_t)jsonGetBool(body, F("band_1"), -1);
    bands[2] = (int8_t)jsonGetBool(body, F("band_2"), -1);
    bands[3] = (int8_t)jsonGetBool(body, F("band_3"), -1);
    bands[4] = (int8_t)jsonGetBool(body, F("band_4"), -1);
    bands[5] = (int8_t)jsonGetBool(body, F("band_5"), -1);
    bands[6] = (int8_t)jsonGetBool(body, F("band_6"), -1);

    char errMsg[48] = {};
    if (!s_useCase->execute(hasAz, az, hasEl, el, bands, errMsg, sizeof(errMsg))) {
        char errBody[128];
        snprintf_P(errBody, sizeof(errBody), PSTR("{\"error\":\"%s\"}"), errMsg);
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    char respBody[440];
    size_t n = buildStatusJson(respBody, sizeof(respBody), s_gps, s_compass, s_rotor,
                               F("\"status\":\"queued\","));
    if (n == 0) {
        char errBody[128]; strcpy_P(errBody, PSTR("{\"status\":\"queued\"}"));
        injectTimestamp(errBody, sizeof(errBody));
        res.json(200, errBody);
        return;
    }

    if (ridState == 1) {
        respBody[n - 1] = ',';
        int extra = snprintf_P(respBody + n, sizeof(respBody) - n,
                               PSTR("\"request_id\":\"%s\"}"), rid);
        if (extra < 0 || (size_t)extra >= sizeof(respBody) - n) {
            char errBody[128]; strcpy_P(errBody, PSTR("{\"status\":\"queued\"}"));
            injectTimestamp(errBody, sizeof(errBody));
            res.json(200, errBody);
            return;
        }
    }

    injectTimestamp(respBody, sizeof(respBody));
    res.json(200, respBody);
}
