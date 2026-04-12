#pragma once
#include <WebServer.h>
#include "../use_cases/HardStopUseCase.h"

void initHardStopHandler(HardStopUseCase* useCase);
void handleHardStop(const HttpRequest& req, HttpResponse& res);
