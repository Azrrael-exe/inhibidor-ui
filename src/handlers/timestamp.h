#pragma once
#include <stddef.h>

class GpsModule;

void initTimestampService(GpsModule* gps);

// Reescribe el '}' final de `body` para insertar:
//   ,"timestamp":"YYYY-MM-DDTHH:MM:SSZ","time_valid":<bool>}
// Devuelve false (sin modificar body) si no cabe o si body no termina en '}'.
bool injectTimestamp(char* body, size_t bodyCapacity);
