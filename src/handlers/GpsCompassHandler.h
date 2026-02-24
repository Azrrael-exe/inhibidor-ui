#pragma once
#include <WebServer.h>
#include "../modules/GpsModule.h"
#include "../modules/CompassModule.h"

void initStatusHandler(GpsModule* gps, CompassModule* compass);
void handleGetStatus(const HttpRequest& req, HttpResponse& res);
