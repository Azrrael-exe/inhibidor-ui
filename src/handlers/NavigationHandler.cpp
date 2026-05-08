#include "NavigationHandler.h"
#include "StatusHandler.h"
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
        res.json(503, "{\"error\":\"rotor not available\"}");
        return;
    }

    const char* body = req.params;

    // Validate request_id BEFORE queueing the command — never execute on a malformed ID.
    char rid[40];
    int ridState = extractRequestId(body, /*isQueryString=*/false, rid, sizeof(rid));
    if (ridState < 0) {
        res.json(400, "{\"error\":\"invalid request_id\"}");
        return;
    }

    bool  hasAz = jsonHasKey(body, "azimuth");
    bool  hasEl = jsonHasKey(body, "elevation");
    float az    = hasAz ? jsonGetFloat(body, "azimuth",   0.0f) : 0.0f;
    float el    = hasEl ? jsonGetFloat(body, "elevation", 0.0f) : 0.0f;

    static const char* const BAND_KEYS[7] = {
        "band_0", "band_1", "band_2", "band_3", "band_4", "band_5", "band_6"
    };

    // -1 = absent (don't touch pin), 0 = LOW, 1 = HIGH
    int8_t bands[7];
    for (uint8_t i = 0; i < 7; i++) {
        bands[i] = (int8_t)jsonGetBool(body, BAND_KEYS[i], -1);
    }

    char errMsg[48] = {};
    if (!s_useCase->execute(hasAz, az, hasEl, el, bands, errMsg, sizeof(errMsg))) {
        res.badRequest(errMsg);
        return;
    }

    char respBody[512];
    size_t n = buildStatusJson(respBody, sizeof(respBody), s_gps, s_compass, s_rotor,
                               "\"status\":\"queued\",");
    if (n == 0) {
        res.json(200, "{\"status\":\"queued\"}");
        return;
    }

    if (ridState == 1) {
        respBody[n - 1] = ',';
        int extra = snprintf(respBody + n, sizeof(respBody) - n,
                             "\"request_id\":\"%s\"}", rid);
        if (extra < 0 || (size_t)extra >= sizeof(respBody) - n) {
            res.json(200, "{\"status\":\"queued\"}");
            return;
        }
    }

    res.json(200, respBody);
}
