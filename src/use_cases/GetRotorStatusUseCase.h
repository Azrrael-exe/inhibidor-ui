#pragma once
#include "../services/RotorService.h"

class GetRotorStatusUseCase {
public:
    explicit GetRotorStatusUseCase(RotorService* service);
    bool execute(RotorStatus& out);

private:
    RotorService* _service;
};
