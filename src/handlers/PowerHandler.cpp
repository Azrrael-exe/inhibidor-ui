#include "PowerHandler.h"
#include "json_helpers.h"
#include "../pinout.h"
#include <Arduino.h>

static const uint8_t BAND_PINS[7] = {
    RF_BAND_0, RF_BAND_1, RF_BAND_2, RF_BAND_3, RF_BAND_4, RF_BAND_5, RF_BAND_6
};
static const char* const BAND_KEYS[7] = {
    "band_0", "band_1", "band_2", "band_3", "band_4", "band_5", "band_6"
};

// POST /set-power
// Body: { "band_0": true, ..., "band_6": false }
// Campos opcionales — los ausentes no modifican el estado actual de esa banda.
void handlePostSetPower(const HttpRequest& req, HttpResponse& res) {
    for (uint8_t i = 0; i < 7; i++) {
        int val = jsonGetBool(req.params, BAND_KEYS[i], -1);
        if (val >= 0) {
            digitalWrite(BAND_PINS[i], val ? HIGH : LOW);
        }
    }
    res.json(200, "{\"status\":\"ok\"}");
}
