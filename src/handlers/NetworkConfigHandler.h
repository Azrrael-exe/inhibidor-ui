#pragma once
#include <WebServer.h>

class ActivityWatchdog;

void initNetworkConfigHandler(ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleSetNetworkConfig(const HttpRequest& req, HttpResponse& res);

// True after a successful set-network-config. main.cpp polls this in loop()
// to trigger NetworkConfig::reboot() AFTER the HTTP response has been flushed
// and the TCP connection closed by WebServer.
bool isNetworkConfigRebootPending();
