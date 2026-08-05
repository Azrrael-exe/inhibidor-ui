#pragma once
#include <stddef.h>
#include <WString.h>

class GpsModule;
class CompassModule;
class RotorService;

// Builds the full status JSON payload (gps, heading, navigation, power) into `out`.
// `prefix` is optional flash-string content (F("...")) inserted right after the
// opening `{` (e.g. F("\"status\":\"queued\",")) and must already include its
// trailing comma if non-empty.
// Returns the number of chars written (excluding NUL), or 0 on overflow.
size_t buildStatusJson(char* out, size_t outLen,
                       GpsModule* gps, CompassModule* compass, RotorService* rotor,
                       const __FlashStringHelper* prefix = nullptr);
