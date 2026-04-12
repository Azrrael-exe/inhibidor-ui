#pragma once
#include <Arduino.h>

// ─── Global serial logger ──────────────────────────────────────────────────
//
// Serial is used exclusively for debug output.
// G5500 communication uses Serial2 (separate bus), so no conflict.
//
// Set LOG_ENABLED to 0 for production builds.
// ──────────────────────────────────────────────────────────────────────────

#define LOG_ENABLED 1

class RotorService;  // forward declaration — avoids circular include with services/RotorService.h

namespace Logger {
    // Call once in setup(), after constructing RotorService.
    // Passing nullptr disables the gate (all logs go through unconditionally).
    // NOTE: Gate is no longer necessary for Serial (G5500 uses Serial2), but kept for consistency.
    void init(const RotorService* rs);

    // Always returns true — Serial (debug) and Serial2 (G5500) are separate buses.
    bool canLog();
}

#if LOG_ENABLED
#  define LOG(tag, msg)      do { if (Logger::canLog()) { Serial.print(F("[" tag "] ")); Serial.println(F(msg)); } } while(0)
#  define LOG_F(tag, msg, v) do { if (Logger::canLog()) { Serial.print(F("[" tag "] ")); Serial.print(F(msg)); Serial.println(v); } } while(0)
#else
#  define LOG(tag, msg)
#  define LOG_F(tag, msg, v)
#endif
