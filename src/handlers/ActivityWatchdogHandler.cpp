#include "ActivityWatchdogHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
#include <stdio.h>

static ActivityWatchdog* s_watchdog         = nullptr;
static int               s_httpChannelId    = -1;
static int               s_controlChannelId = -1;

static constexpr long MIN_TIMEOUT_S = 1;
static constexpr long MAX_TIMEOUT_S = 3600;

void initActivityWatchdogHandler(ActivityWatchdog* watchdog,
                                 int httpChannelId, int controlChannelId) {
    s_watchdog         = watchdog;
    s_httpChannelId    = httpChannelId;
    s_controlChannelId = controlChannelId;
}

static void handleGetTimeout(const HttpRequest& req, HttpResponse& res, int channelId) {
    if (s_watchdog) s_watchdog->feed(s_httpChannelId);

    if (!s_watchdog || channelId < 0) {
        char errBody[128] = "{\"error\":\"watchdog not available\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    char body[128];
    snprintf(body, sizeof(body),
             "{\"timeout_seconds\":%lu}",
             s_watchdog->channelTimeoutMs(channelId) / 1000UL);
    injectTimestamp(body, sizeof(body));
    res.json(200, body);
}

static void handleSetTimeout(const HttpRequest& req, HttpResponse& res, int channelId) {
    if (s_watchdog) s_watchdog->feed(s_httpChannelId);

    if (!s_watchdog || channelId < 0) {
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

    s_watchdog->setChannelTimeoutMs(channelId, (unsigned long)seconds * 1000UL);
    s_watchdog->feed(channelId);

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

// GET /http-watchdog-timeout
void handleGetHttpWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    handleGetTimeout(req, res, s_httpChannelId);
}

// POST /set-http-watchdog-timeout
// Body: { "timeout_seconds": <1..3600>, "request_id": "abc" (optional) }
void handleSetHttpWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    handleSetTimeout(req, res, s_httpChannelId);
}

// GET /control-watchdog-timeout
void handleGetControlWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    handleGetTimeout(req, res, s_controlChannelId);
}

// POST /set-control-watchdog-timeout
// Body: { "timeout_seconds": <1..3600>, "request_id": "abc" (optional) }
void handleSetControlWatchdogTimeout(const HttpRequest& req, HttpResponse& res) {
    handleSetTimeout(req, res, s_controlChannelId);
}
