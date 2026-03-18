#pragma once
#include <WebServer.h>
#include "../use_cases/SetNavigationAndPowerUseCase.h"

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
