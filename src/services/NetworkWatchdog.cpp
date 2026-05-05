#include "NetworkWatchdog.h"
#include "../use_cases/HardStopUseCase.h"
#include "../logger.h"
#include <DigitalSwitch.h>
#include <Ethernet.h>

NetworkWatchdog::NetworkWatchdog(HardStopUseCase* hardStop, unsigned long timeoutMs)
    : _hardStop(hardStop),
      _timeoutMs(timeoutMs),
      _lastActivityMs(0),
      _tripped(false),
      _azFwd(nullptr), _azBwd(nullptr),
      _elFwd(nullptr), _elBwd(nullptr),
      _rfPwr(nullptr) {}

void NetworkWatchdog::notifyActivity() {
    _lastActivityMs = millis();
}

void NetworkWatchdog::setManualOverrideSources(DigitalSwitch* azFwd, DigitalSwitch* azBwd,
                                               DigitalSwitch* elFwd, DigitalSwitch* elBwd,
                                               DigitalSwitch* rfPwr) {
    _azFwd = azFwd;
    _azBwd = azBwd;
    _elFwd = elFwd;
    _elBwd = elBwd;
    _rfPwr = rfPwr;
}

bool NetworkWatchdog::isManualOverrideActive() const {
    // Movement switches are active-HIGH (industrial 24V inputs).
    if (_azFwd && _azFwd->getState()) return true;
    if (_azBwd && _azBwd->getState()) return true;
    if (_elFwd && _elFwd->getState()) return true;
    if (_elBwd && _elBwd->getState()) return true;
    // RF power switch is wired to GND with INPUT_PULLUP: LOW = pressed = RF active.
    if (_rfPwr && !_rfPwr->getState()) return true;
    return false;
}

bool NetworkWatchdog::isNetworkDown() const {
    if (Ethernet.linkStatus() != LinkON) return true;
    if ((millis() - _lastActivityMs) > _timeoutMs) return true;
    return false;
}

void NetworkWatchdog::update() {
    if (isNetworkDown()) {
        if (!_tripped && !isManualOverrideActive() && _hardStop) {
            _hardStop->execute();
            _tripped = true;
            LOG_F("NetWatchdog", "Tripped, link=", (Ethernet.linkStatus() == LinkON));
        }
    } else {
        if (_tripped) {
            LOG("NetWatchdog", "Network recovered, re-armed");
        }
        _tripped = false;
    }
}
