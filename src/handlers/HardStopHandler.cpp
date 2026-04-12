#include "HardStopHandler.h"

static HardStopUseCase* s_useCase = nullptr;

void initHardStopHandler(HardStopUseCase* useCase) {
    s_useCase = useCase;
}

// POST /hard-stop
void handleHardStop(const HttpRequest& req, HttpResponse& res) {
    if (!s_useCase) {
        res.json(503, "{\"error\":\"use case not available\"}");
        return;
    }

    s_useCase->execute();
    res.json(200, "{\"status\":\"hard_stop_executed\"}");
}
