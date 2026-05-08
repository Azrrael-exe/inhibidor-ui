#pragma once
#include <WebServer.h>

class RFOnTimeWatchdog;
class ActivityWatchdog;

void initConfigHandler(const uint8_t* mac,
                       RFOnTimeWatchdog* rfWd,
                       ActivityWatchdog* activityWd,
                       int channelId = -1);
void handleGetConfig(const HttpRequest& req, HttpResponse& res);
