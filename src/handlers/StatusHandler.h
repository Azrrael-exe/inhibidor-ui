#pragma once
#include <stddef.h>

class GpsModule;
class CompassModule;
class RotorService;

// Builds the full status JSON payload (gps, heading, navigation, power) into `out`.
// `prefix` is optional content inserted right after the opening `{` (e.g. `"\"status\":\"queued\","`)
// and must already include its trailing comma if non-empty.
// Returns the number of chars written (excluding NUL), or 0 on overflow.
size_t buildStatusJson(char* out, size_t outLen,
                       GpsModule* gps, CompassModule* compass, RotorService* rotor,
                       const char* prefix = nullptr);
