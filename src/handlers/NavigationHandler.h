#pragma once
#include <WebServer.h>
#include "../use_cases/SetNavigationAndPowerUseCase.h"

class ActivityWatchdog;

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase,
                           ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
