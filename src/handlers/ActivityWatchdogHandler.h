#pragma once
#include <WebServer.h>

class ActivityWatchdog;

void initActivityWatchdogHandler(ActivityWatchdog* watchdog,
                                 int httpChannelId, int controlChannelId);

void handleGetHttpWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
void handleSetHttpWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
void handleGetControlWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
void handleSetControlWatchdogTimeout(const HttpRequest& req, HttpResponse& res);
