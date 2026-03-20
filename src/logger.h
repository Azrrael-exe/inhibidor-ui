#pragma once
#include <Arduino.h>

// ─── Global serial logger ──────────────────────────────────────────────────
//
// Serial is shared between G5500 LLP binary frames (via RotorService) and
// debug ASCII output. Sending ASCII while the G5500 is in POLL_SENT state
// places bytes on its RX line that it may interpret as a new LLP command,
// corrupting the protocol.
//
// Logger::canLog() gates every LOG() call: it returns false whenever
// RotorService reports the serial is busy (POLL_SENT), silently dropping
// the message instead of corrupting the bus.
//
// Set LOG_ENABLED to 0 for production builds.
// ──────────────────────────────────────────────────────────────────────────

#define LOG_ENABLED 0

class RotorService;  // forward declaration — avoids circular include with services/RotorService.h

namespace Logger {
    // Call once in setup(), after constructing RotorService.
    // Passing nullptr disables the gate (all logs go through unconditionally).
    void init(const RotorService* rs);

    // Returns true if Serial is currently free for ASCII output.
    bool canLog();
}

#if LOG_ENABLED
#  define LOG(tag, msg)      do { if (Logger::canLog()) { Serial.print(F("[" tag "] ")); Serial.println(F(msg)); } } while(0)
#  define LOG_F(tag, msg, v) do { if (Logger::canLog()) { Serial.print(F("[" tag "] ")); Serial.print(F(msg)); Serial.println(v); } } while(0)
#else
#  define LOG(tag, msg)
#  define LOG_F(tag, msg, v)
#endif
