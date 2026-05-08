#include "ActivityWatchdog.h"
#include "../logger.h"

ActivityWatchdog::ActivityWatchdog(Action onTimeout, void* context)
    : _channelCount(0),
      _onTimeout(onTimeout),
      _context(context),
      _tripped(false),
      _trippedChannel(-1) {}

int ActivityWatchdog::registerChannel(const char* name, unsigned long timeoutMs) {
    if (_channelCount >= MAX_CHANNELS) return -1;
    int id = _channelCount++;
    _channels[id].name           = name;
    _channels[id].timeoutMs      = timeoutMs;
    _channels[id].lastActivityMs = millis();
    return id;
}

void ActivityWatchdog::feed(int channelId) {
    if (channelId < 0 || channelId >= _channelCount) return;
    _channels[channelId].lastActivityMs = millis();
}

void ActivityWatchdog::update() {
    if (_channelCount == 0) return;

    unsigned long now = millis();
    bool allExpired = true;
    int  firstExpired = -1;
    for (int i = 0; i < _channelCount; i++) {
        bool expired = (now - _channels[i].lastActivityMs) > _channels[i].timeoutMs;
        if (expired) {
            if (firstExpired < 0) firstExpired = i;
        } else {
            allExpired = false;
        }
    }

    if (!_tripped && allExpired) {
        _tripped = true;
        _trippedChannel = firstExpired;
        if (_onTimeout) _onTimeout(_context);
        LOG_F("Watchdog", "Tripped, all channels idle. First expired: ",
              _channels[firstExpired].name);
    } else if (_tripped && !allExpired) {
        _tripped = false;
        _trippedChannel = -1;
        LOG("Watchdog", "Re-armed");
    }
}
