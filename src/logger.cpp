#include "logger.h"
#include "services/RotorService.h"

static const RotorService* s_rs = nullptr;

void Logger::init(const RotorService* rs) {
    s_rs = rs;
}

bool Logger::canLog() {
    if (!s_rs) return true;
    return s_rs->isSerialFree();
}
