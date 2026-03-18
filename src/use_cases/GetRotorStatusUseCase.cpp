#include "GetRotorStatusUseCase.h"

GetRotorStatusUseCase::GetRotorStatusUseCase(RotorService* service)
    : _service(service) {}

bool GetRotorStatusUseCase::execute(RotorStatus& out) {
    if (!_service->hasStatus()) return false;
    out = _service->getStatus();
    return true;
}
