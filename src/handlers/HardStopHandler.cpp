#include "HardStopHandler.h"
#include "../services/ActivityWatchdog.h"

static HardStopUseCase*  s_useCase    = nullptr;
static ActivityWatchdog* s_watchdog   = nullptr;
static int               s_channelId  = -1;

void initHardStopHandler(HardStopUseCase* useCase,
                         ActivityWatchdog* watchdog, int channelId) {
    s_useCase   = useCase;
    s_watchdog  = watchdog;
    s_channelId = channelId;
}

// POST /hard-stop
void handleHardStop(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_useCase) {
        res.json(503, "{\"error\":\"use case not available\"}");
        return;
    }

    s_useCase->execute();
    res.json(200, "{\"status\":\"hard_stop_executed\"}");
}
