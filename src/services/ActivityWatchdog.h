#pragma once
#include <Arduino.h>

class ActivityWatchdog {
public:
    using Action = void (*)(void* ctx);

    ActivityWatchdog(Action onTimeout, void* context);

    // Register an activity channel. Returns the channel id (>= 0), or -1 if no
    // slots are available. The channel starts fed (lastActivityMs = millis()).
    int registerChannel(const char* name, unsigned long timeoutMs);

    // Reset the activity timer of a given channel. No-op for invalid ids.
    void feed(int channelId);

    // Call once per loop(). Fires the action once per outage when any registered
    // channel exceeds its timeout. Re-arms once all channels are within their
    // timeouts again.
    void update();

    bool isTripped() const { return _tripped; }
    int  trippedChannel() const { return _trippedChannel; }

    int           channelCount() const { return _channelCount; }
    const char*   channelName(int id) const {
        return (id >= 0 && id < _channelCount) ? _channels[id].name : nullptr;
    }
    unsigned long channelTimeoutMs(int id) const {
        return (id >= 0 && id < _channelCount) ? _channels[id].timeoutMs : 0UL;
    }

private:
    static constexpr int MAX_CHANNELS = 4;

    struct Channel {
        const char*   name;
        unsigned long timeoutMs;
        unsigned long lastActivityMs;
    };

    Channel _channels[MAX_CHANNELS];
    int     _channelCount;
    Action  _onTimeout;
    void*   _context;
    bool    _tripped;
    int     _trippedChannel;
};
