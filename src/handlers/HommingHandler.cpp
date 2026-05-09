#include "HommingHandler.h"
#include "timestamp.h"
#include "../services/ActivityWatchdog.h"

static HommingUseCase*   s_useCase    = nullptr;
static ActivityWatchdog* s_watchdog   = nullptr;
static int               s_channelId  = -1;

void initHommingHandler(HommingUseCase* useCase,
                        ActivityWatchdog* watchdog, int channelId) {
    s_useCase   = useCase;
    s_watchdog  = watchdog;
    s_channelId = channelId;
}

// POST /homming
void handleHomming(const HttpRequest& req, HttpResponse& res) {
    if (s_watchdog) s_watchdog->feed(s_channelId);

    if (!s_useCase) {
        char errBody[128] = "{\"error\":\"use case not available\"}";
        injectTimestamp(errBody, sizeof(errBody));
        res.json(503, errBody);
        return;
    }

    s_useCase->execute();
    char body[128] = "{\"status\":\"homming_executed\"}";
    injectTimestamp(body, sizeof(body));
    res.json(200, body);
}
