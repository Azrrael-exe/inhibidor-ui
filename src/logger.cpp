#include "logger.h"
#include "services/RotorService.h"

static const RotorService* s_rs = nullptr;

void Logger::init(const RotorService* rs) {
    s_rs = rs;
}

bool Logger::canLog() {
    return true;  // Serial (debug) and Serial2 (G5500) are independent buses — no gate needed
}
