#pragma once
#include <WebServer.h>
#include "../modules/GpsModule.h"
#include "../modules/CompassModule.h"

// Forward declaration — full definition pulled in by GpsCompassHandler.cpp
class RotorService;

void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor = nullptr);
void handleGetStatus(const HttpRequest& req, HttpResponse& res);
