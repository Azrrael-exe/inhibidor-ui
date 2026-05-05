#pragma once
#include <WebServer.h>
#include "../use_cases/SetNavigationAndPowerUseCase.h"

class NetworkWatchdog;

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase, NetworkWatchdog* watchdog = nullptr);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
