#pragma once
#include <WebServer.h>
#include "../use_cases/SetNavigationAndPowerUseCase.h"

class GpsModule;
class CompassModule;
class RotorService;
class ActivityWatchdog;

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase,
                           GpsModule* gps, CompassModule* compass, RotorService* rotor,
                           ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
