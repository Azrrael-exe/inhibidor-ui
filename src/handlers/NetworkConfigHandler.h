#pragma once
#include <Arduino.h>
#include <WebServer.h>

class ActivityWatchdog;

void initNetworkConfigHandler(const uint8_t* mac,
                              ActivityWatchdog* watchdog = nullptr,
                              int channelId = -1);
void handleGetNetworkConfig(const HttpRequest& req, HttpResponse& res);
void handleSetNetworkConfig(const HttpRequest& req, HttpResponse& res);

// True after a successful POST /config/network. main.cpp polls this in loop()
// to trigger NetworkConfig::reboot() AFTER the HTTP response has been flushed
// and the TCP connection closed by WebServer.
bool isNetworkConfigRebootPending();
