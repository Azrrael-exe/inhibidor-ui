#pragma once
#include <Arduino.h>

class HardStopUseCase;
class DigitalSwitch;

class NetworkWatchdog {
public:
    NetworkWatchdog(HardStopUseCase* hardStop, unsigned long timeoutMs = 10000UL);

    // Reset the inactivity timer. Call from every HTTP handler entry point.
    void notifyActivity();

    // Register the manual-control inputs that pause the failsafe when active.
    // Pass nullptr for any source that does not apply.
    void setManualOverrideSources(DigitalSwitch* azFwd, DigitalSwitch* azBwd,
                                  DigitalSwitch* elFwd, DigitalSwitch* elBwd,
                                  DigitalSwitch* rfPwr);

    // Call once in loop(). Fires HardStopUseCase::execute() once per outage
    // when the network is down and no manual override is active.
    void update();

    bool isTripped() const { return _tripped; }

private:
    HardStopUseCase* _hardStop;
    unsigned long    _timeoutMs;
    unsigned long    _lastActivityMs;
    bool             _tripped;

    DigitalSwitch* _azFwd;
    DigitalSwitch* _azBwd;
    DigitalSwitch* _elFwd;
    DigitalSwitch* _elBwd;
    DigitalSwitch* _rfPwr;

    bool isManualOverrideActive() const;
    bool isNetworkDown() const;
};
