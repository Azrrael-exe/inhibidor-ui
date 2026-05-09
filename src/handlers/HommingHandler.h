#pragma once
#include <WebServer.h>
#include "../use_cases/HommingUseCase.h"

class ActivityWatchdog;

void initHommingHandler(HommingUseCase* useCase,
                        ActivityWatchdog* watchdog = nullptr, int channelId = -1);
void handleHomming(const HttpRequest& req, HttpResponse& res);
