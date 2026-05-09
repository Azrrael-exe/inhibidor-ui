#include "RFWatchdogHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
#include <stdio.h>

static RFOnTimeWatchdog* s_wd         = nullptr;
static ActivityWatchdog* s_watchdog   = nullptr;
static int               s_channelId  = -1;

static constexpr long MIN_TIMEOUT_S = 1;
static constexpr long MAX_TIMEOUT_S = 3600;

void initRFWatchdogHandler(RFOnTimeWatchdog* wd,
                           ActivityWatchdog* watchdog, int channelId) {
    s_wd        = wd;
    s_watchdog  = watchdog;
    s_channelId = channelId;
}

// GET /rf-watchdog-timeout
void handleGetRFWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_wd) {
        char errBody[128] = "{\"error\":\"watchdog not available\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    char body[128];
    snprintf(body, sizeof(body),
             "{\"timeout_seconds\":%lu,\"active\":%s}",
             s_wd->getMaxOnMs() / 1000UL,
             s_wd->isAnyOn() ? "true" : "false");
    injectTimestamp(body, sizeof(body));
    res.json(200, body);
}

// POST /set-rf-watchdog-timeout
// Body: { "timeout_seconds": <1..3600>, "request_id": "abc" (optional) }
void handleSetRFWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_wd) {
        char errBody[128] = "{\"error\":\"watchdog not available\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    const char* body = req.params;

    char rid[40];
    int ridState = extractRequestId(body, /*isQueryString=*/false, rid, sizeof(rid));
    if (ridState < 0) {
        char errBody[128] = "{\"error\":\"invalid request_id\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    if (!jsonHasKey(body, "timeout_seconds")) {
        char errBody[128] = "{\"error\":\"missing timeout_seconds\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    long seconds = (long)jsonGetInt(body, "timeout_seconds", -1);
    if (seconds < MIN_TIMEOUT_S || seconds > MAX_TIMEOUT_S) {
        char errBody[128] = "{\"error\":\"timeout_seconds out of range (1..3600)\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    s_wd->setMaxOnMs((unsigned long)seconds * 1000UL);

    char respBody[160];
    if (ridState == 1) {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"updated\",\"timeout_seconds\":%ld,\"request_id\":\"%s\"}",
                 seconds, rid);
    } else {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"updated\",\"timeout_seconds\":%ld}",
                 seconds);
    }
    injectTimestamp(respBody, sizeof(respBody));
    res.json(200, respBody);
}
