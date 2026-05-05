#include "HardStopHandler.h"
#include "../services/NetworkWatchdog.h"

static HardStopUseCase* s_useCase  = nullptr;
static NetworkWatchdog* s_watchdog = nullptr;

void initHardStopHandler(HardStopUseCase* useCase, NetworkWatchdog* watchdog) {
    s_useCase  = useCase;
    s_watchdog = watchdog;
}

// POST /hard-stop
void handleHardStop(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->notifyActivity();

    if (!s_useCase) {
        res.json(503, "{\"error\":\"use case not available\"}");
        return;
    }

    s_useCase->execute();
    res.json(200, "{\"status\":\"hard_stop_executed\"}");
}
