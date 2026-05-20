#include "WatchdogConfigHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
#include <stdio.h>

static RFOnTimeWatchdog* s_rfWd           = nullptr;
static ActivityWatchdog* s_activityWd     = nullptr;
static int               s_httpChannelId  = -1;
static int               s_controlChannelId = -1;

static constexpr long MIN_TIMEOUT_S = 1;
static constexpr long MAX_TIMEOUT_S = 3600;

void initWatchdogConfigHandler(RFOnTimeWatchdog* rfWd,
                               ActivityWatchdog* activityWd,
                               int httpChannelId,
                               int controlChannelId) {
    s_rfWd            = rfWd;
    s_activityWd      = activityWd;
    s_httpChannelId   = httpChannelId;
    s_controlChannelId = controlChannelId;
}

// GET /config/watchdog
// Returns the RF watchdog timeout/state and all activity watchdog channel timeouts.
void handleGetWatchdogConfig(const HttpRequest& req, HttpResponse& res) {
    if (s_activityWd) s_activityWd->feed(s_httpChannelId);

    char body[300];
    int n = snprintf(body, sizeof(body),
        "{\"rf_watchdog\":{\"timeout_seconds\":%lu,\"active\":%s},"
        "\"activity_watchdog\":{\"channels\":[",
        s_rfWd ? s_rfWd->getMaxOnMs() / 1000UL : 0UL,
        (s_rfWd && s_rfWd->isAnyOn()) ? "true" : "false");

    if (n < 0 || (size_t)n >= sizeof(body)) {
        char errBody[128] = "{\"error\":\"buffer overflow\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(500, errBody);
        return;
    }

    if (s_activityWd) {
        int count = s_activityWd->channelCount();
        for (int i = 0; i < count; i++) {
            const char* name = s_activityWd->channelName(i);
            int extra = snprintf(body + n, sizeof(body) - n,
                "%s{\"name\":\"%s\",\"timeout_ms\":%lu}",
                (i == 0) ? "" : ",",
                name ? name : "",
                s_activityWd->channelTimeoutMs(i));
            if (extra < 0 || (size_t)extra >= sizeof(body) - n) {
                char errBody[128] = "{\"error\":\"buffer overflow\"}";
                injectTimestamp(errBody, sizeof(errBody));
                res.json(500, errBody);
                return;
            }
            n += extra;
        }
    }

    int extra = snprintf(body + n, sizeof(body) - n, "]}}");
    if (extra < 0 || (size_t)extra >= sizeof(body) - n) {
        char errBody[128] = "{\"error\":\"buffer overflow\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(500, errBody);
        return;
    }

    injectTimestamp(body, sizeof(body));
    res.json(200, body);
}

// POST /config/watchdog
// Body (all fields optional — omit any to leave that watchdog unchanged):
//   { "rf_timeout_seconds": 300, "http_timeout_seconds": 60, "control_timeout_seconds": 60, "request_id": "..." }
void handleSetWatchdogConfig(const HttpRequest& req, HttpResponse& res) {
    if (s_activityWd) s_activityWd->feed(s_httpChannelId);

    const char* body = req.params;

    char rid[40];
    int ridState = extractRequestId(body, /*isQueryString=*/false, rid, sizeof(rid));
    if (ridState < 0) {
        char errBody[128] = "{\"error\":\"invalid request_id\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    bool anyField = false;

    if (jsonHasKey(body, "rf_timeout_seconds")) {
        if (!s_rfWd) {
            char errBody[128] = "{\"error\":\"rf watchdog not available\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(503, errBody);
            return;
        }
        long seconds = (long)jsonGetInt(body, "rf_timeout_seconds", -1);
        if (seconds < MIN_TIMEOUT_S || seconds > MAX_TIMEOUT_S) {
            char errBody[128] = "{\"error\":\"rf_timeout_seconds out of range (1..3600)\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        s_rfWd->setMaxOnMs((unsigned long)seconds * 1000UL);
        anyField = true;
    }

    if (jsonHasKey(body, "http_timeout_seconds")) {
        if (!s_activityWd || s_httpChannelId < 0) {
            char errBody[128] = "{\"error\":\"http watchdog not available\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(503, errBody);
            return;
        }
        long seconds = (long)jsonGetInt(body, "http_timeout_seconds", -1);
        if (seconds < MIN_TIMEOUT_S || seconds > MAX_TIMEOUT_S) {
            char errBody[128] = "{\"error\":\"http_timeout_seconds out of range (1..3600)\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        s_activityWd->setChannelTimeoutMs(s_httpChannelId, (unsigned long)seconds * 1000UL);
        s_activityWd->feed(s_httpChannelId);
        anyField = true;
    }

    if (jsonHasKey(body, "control_timeout_seconds")) {
        if (!s_activityWd || s_controlChannelId < 0) {
            char errBody[128] = "{\"error\":\"control watchdog not available\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(503, errBody);
            return;
        }
        long seconds = (long)jsonGetInt(body, "control_timeout_seconds", -1);
        if (seconds < MIN_TIMEOUT_S || seconds > MAX_TIMEOUT_S) {
            char errBody[128] = "{\"error\":\"control_timeout_seconds out of range (1..3600)\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        s_activityWd->setChannelTimeoutMs(s_controlChannelId, (unsigned long)seconds * 1000UL);
        s_activityWd->feed(s_controlChannelId);
        anyField = true;
    }

    if (!anyField) {
        char errBody[128] = "{\"error\":\"no watchdog field provided\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    char respBody[160];
    if (ridState == 1) {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"updated\",\"request_id\":\"%s\"}", rid);
    } else {
        snprintf(respBody, sizeof(respBody), "{\"status\":\"updated\"}");
    }
    injectTimestamp(respBody, sizeof(respBody));
    res.json(200, respBody);
}
