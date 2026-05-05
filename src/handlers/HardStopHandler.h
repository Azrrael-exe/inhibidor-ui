#pragma once
#include <WebServer.h>
#include "../use_cases/HardStopUseCase.h"

class NetworkWatchdog;

void initHardStopHandler(HardStopUseCase* useCase, NetworkWatchdog* watchdog = nullptr);
void handleHardStop(const HttpRequest& req, HttpResponse& res);
