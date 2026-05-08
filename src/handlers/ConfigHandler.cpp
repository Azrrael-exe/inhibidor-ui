#include "ConfigHandler.h"
#include "json_helpers.h"
#include "../services/NetworkConfig.h"
#include "../services/ActivityWatchdog.h"
#include "../services/RFOnTimeWatchdog.h"
#include <Ethernet.h>
#include <stdio.h>

static const uint8_t*    s_mac        = nullptr;
static RFOnTimeWatchdog* s_rfWd       = nullptr;
static ActivityWatchdog* s_activityWd = nullptr;
static int               s_channelId  = -1;

void initConfigHandler(const uint8_t* mac,
                       RFOnTimeWatchdog* rfWd,
                       ActivityWatchdog* activityWd,
                       int channelId) {
    s_mac        = mac;
    s_rfWd       = rfWd;
    s_activityWd = activityWd;
    s_channelId  = channelId;
}

// GET /get-config
// Returns the full runtime configuration: network (EEPROM + live), RF watchdog
// timeout/state, and activity watchdog channel timeouts.
void handleGetConfig(const HttpRequest& req, HttpResponse& res) {
    if (s_activityWd) s_activityWd->feed(s_channelId);

    NetConfig cfg;
    bool valid = NetworkConfig::load(cfg);
    if (!valid) NetworkConfig::factoryDefaults(cfg);

    char ipStr[16], subnetStr[16], gatewayStr[16], currentStr[16];
    formatIPv4(cfg.ip,      ipStr,      sizeof(ipStr));
    formatIPv4(cfg.subnet,  subnetStr,  sizeof(subnetStr));
    formatIPv4(cfg.gateway, gatewayStr, sizeof(gatewayStr));

    IPAddress current = Ethernet.localIP();
    uint32_t curBE = ((uint32_t)current[0] << 24) | ((uint32_t)current[1] << 16)
                   | ((uint32_t)current[2] << 8)  |  (uint32_t)current[3];
    formatIPv4(curBE, currentStr, sizeof(currentStr));

    char macStr[18] = "";
    if (s_mac) {
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5]);
    }

    char body[512];
    int n = snprintf(body, sizeof(body),
        "{\"network\":{\"mode\":\"%s\",\"ip\":\"%s\",\"subnet\":\"%s\","
        "\"gateway\":\"%s\",\"currentIp\":\"%s\",\"macAddress\":\"%s\"},"
        "\"rf_watchdog\":{\"timeout_seconds\":%lu,\"active\":%s},"
        "\"activity_watchdog\":{\"channels\":[",
        cfg.useEepromConfig ? "static" : "dhcp",
        ipStr, subnetStr, gatewayStr, currentStr, macStr,
        s_rfWd ? s_rfWd->getMaxOnMs() / 1000UL : 0UL,
        (s_rfWd && s_rfWd->isAnyOn()) ? "true" : "false");

    if (n < 0 || (size_t)n >= sizeof(body)) {
        res.json(500, "{\"error\":\"buffer overflow\"}");
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
                res.json(500, "{\"error\":\"buffer overflow\"}");
                return;
            }
            n += extra;
        }
    }

    int extra = snprintf(body + n, sizeof(body) - n, "]}}");
    if (extra < 0 || (size_t)extra >= sizeof(body) - n) {
        res.json(500, "{\"error\":\"buffer overflow\"}");
        return;
    }

    res.json(200, body);
}
