#pragma once
#include <WebServer.h>
#include "../use_cases/HardStopUseCase.h"

class ActivityWatchdog;

void initHardStopHandler(HardStopUseCase* useCase,
                         ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleHardStop(const HttpRequest& req, HttpResponse& res);
