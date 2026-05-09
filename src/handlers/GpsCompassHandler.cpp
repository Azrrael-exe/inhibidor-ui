#include "GpsCompassHandler.h"
#include "StatusHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
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

// GET /status[?request_id=<id>]
void handleGetStatus(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_gps || !s_compass) {
        char errBody[128] = "{\"error\":\"module not initialized\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    char rid[40];
    int ridState = extractRequestId(req.params, /*isQueryString=*/true, rid, sizeof(rid));
    if (ridState < 0) {
        char errBody[128] = "{\"error\":\"invalid request_id\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    char body[480];
    size_t n = buildStatusJson(body, sizeof(body), s_gps, s_compass, s_rotor);
    if (n == 0) {
        char errBody[128] = "{\"error\":\"status payload overflow\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(500, errBody);
        return;
    }

    if (ridState == 1) {
        body[n - 1] = ',';
        int extra = snprintf(body + n, sizeof(body) - n,
                             "\"request_id\":\"%s\"}", rid);
        if (extra < 0 || (size_t)extra >= sizeof(body) - n) {
            char errBody[128] = "{\"error\":\"status payload overflow\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(500, errBody);
            return;
        }
    }

    injectTimestamp(body, sizeof(body));
    res.json(200, body);
}
