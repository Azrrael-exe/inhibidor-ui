#include "NetworkConfigHandler.h"
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
        res.json(400, "{\"error\":\"invalid request_id\"}");
        return;
    }

    char mode[12];
    if (!jsonGetString(body, "mode", mode, sizeof(mode))) {
        res.json(400, "{\"error\":\"missing mode\"}");
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
            res.json(400, "{\"error\":\"missing ip/subnet/gateway\"}");
            return;
        }
        if (!parseIPv4(ipStr,      cfg.ip))      { res.json(400, "{\"error\":\"invalid ip\"}");      return; }
        if (!parseIPv4(subnetStr,  cfg.subnet))  { res.json(400, "{\"error\":\"invalid subnet\"}");  return; }
        if (!parseIPv4(gatewayStr, cfg.gateway)) { res.json(400, "{\"error\":\"invalid gateway\"}"); return; }
        cfg.useEepromConfig = true;
    } else {
        res.json(400, "{\"error\":\"invalid mode\"}");
        return;
    }

    char err[40];
    if (!NetworkConfig::validate(cfg, err, sizeof(err))) {
        char body400[80];
        snprintf(body400, sizeof(body400), "{\"error\":\"%s\"}", err);
        res.json(400, body400);
        return;
    }

    if (!NetworkConfig::save(cfg)) {
        res.json(500, "{\"error\":\"eeprom write failed\"}");
        return;
    }

    char respBody[96];
    if (ridState == 1) {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"saved\",\"reboot\":true,\"request_id\":\"%s\"}", rid);
    } else {
        snprintf(respBody, sizeof(respBody),
                 "{\"status\":\"saved\",\"reboot\":true}");
    }
    res.json(200, respBody);

    s_rebootPending = true;
}
