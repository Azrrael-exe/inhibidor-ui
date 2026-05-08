#pragma once
#include <WebServer.h>
#include "../services/RFOnTimeWatchdog.h"

class ActivityWatchdog;

void initRFWatchdogHandler(RFOnTimeWatchdog* wd,
                           ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleGetRFWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
void handleSetRFWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
