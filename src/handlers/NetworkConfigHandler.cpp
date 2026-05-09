#include "NetworkConfigHandler.h"
#include "timestamp.h"
#include "json_helpers.h"
#include "../services/ActivityWatchdog.h"
#include "../services/NetworkConfig.h"
#include <stdio.h>
#include <string.h>

static ActivityWatchdog* s_watchdog        = nullptr;
static int               s_channelId       = -1;
static bool              s_rebootPending   = false;

void initNetworkConfigHandler(ActivityWatchdog* watchdog, int channelId) {
    s_watchdog      = watchdog;
    s_channelId     = channelId;
    s_rebootPending = false;
}

bool isNetworkConfigRebootPending() {
    return s_rebootPending;
}

// POST /set-network-config
// Body (DHCP):   {"mode":"dhcp"}
// Body (static): {"mode":"static","ip":"...","subnet":"...","gateway":"...","request_id":"..."}
//
// On success: writes EEPROM, returns 200, and arms a deferred reboot. The
// device reboots from main.cpp::loop() once the TCP connection is closed.
void handleSetNetworkConfig(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    const char* body = req.params;

    char rid[40];
    int ridState = extractRequestId(body, /*isQueryString=*/false, rid, sizeof(rid));
    if (ridState < 0) {
        char errBody[128] = "{\"error\":\"invalid request_id\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    char mode[12];
    if (!jsonGetString(body, "mode", mode, sizeof(mode))) {
        char errBody[128] = "{\"error\":\"missing mode\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    NetConfig cfg;
    NetworkConfig::factoryDefaults(cfg);

    if (strcmp(mode, "dhcp") == 0) {
        cfg.useEepromConfig = false;
    } else if (strcmp(mode, "static") == 0) {
        char ipStr[20], subnetStr[20], gatewayStr[20];
        if (!jsonGetString(body, "ip",      ipStr,      sizeof(ipStr))      ||
            !jsonGetString(body, "subnet",  subnetStr,  sizeof(subnetStr))  ||
            !jsonGetString(body, "gateway", gatewayStr, sizeof(gatewayStr))) {
            char errBody[128] = "{\"error\":\"missing ip/subnet/gateway\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        if (!parseIPv4(ipStr,      cfg.ip))      {
            char errBody[128] = "{\"error\":\"invalid ip\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        if (!parseIPv4(subnetStr,  cfg.subnet))  {
            char errBody[128] = "{\"error\":\"invalid subnet\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        if (!parseIPv4(gatewayStr, cfg.gateway)) {
            char errBody[128] = "{\"error\":\"invalid gateway\"}";
            injectTimestamp(errBody, sizeof(errBody));
            res.json(400, errBody);
            return;
        }
        cfg.useEepromConfig = true;
    } else {
        char errBody[128] = "{\"error\":\"invalid mode\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    char err[40];
    if (!NetworkConfig::validate(cfg, err, sizeof(err))) {
        char errBody[128];
        snprintf(errBody, sizeof(errBody), "{\"error\":\"%s\"}", err);
        injectTimestamp(errBody, sizeof(errBody));
        res.json(400, errBody);
        return;
    }

    if (!NetworkConfig::save(cfg)) {
        char errBody[128] = "{\"error\":\"eeprom write failed\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(500, errBody);
        return;
    }

    char respBody[160];
    if (ridState == 1) {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"saved\",\"reboot\":true,\"request_id\":\"%s\"}", rid);
    } else {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"saved\",\"reboot\":true}");
    }
    injectTimestamp(respBody, sizeof(respBody));
    res.json(200, respBody);

    s_rebootPending = true;
}
