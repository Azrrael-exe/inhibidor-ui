#pragma once
#include <WebServer.h>
#include "../services/RFOnTimeWatchdog.h"

class ActivityWatchdog;

void initWatchdogConfigHandler(RFOnTimeWatchdog* rfWd,
                               ActivityWatchdog* activityWd,
                               int httpChannelId,
                               int controlChannelId);
void handleGetWatchdogConfig(const HttpRequest& req, HttpResponse& res);
void handleSetWatchdogConfig(const HttpRequest& req, HttpResponse& res);
