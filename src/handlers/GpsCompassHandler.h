#pragma once
#include <WebServer.h>
#include "../modules/GpsModule.h"
#include "../modules/CompassModule.h"

// Forward declaration — full definition pulled in by GpsCompassHandler.cpp
class RotorService;
class ActivityWatchdog;

void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor = nullptr,
                       ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleGetStatus(const HttpRequest& req, HttpResponse& res);
