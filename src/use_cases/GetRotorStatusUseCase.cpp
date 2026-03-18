#include "GetRotorStatusUseCase.h"

GetRotorStatusUseCase::GetRotorStatusUseCase(RotorService* service)
    : _service(service) {}

bool GetRotorStatusUseCase::execute(RotorStatus& out) {
    return _service->readStatus(out);
}
