#include "NavigationHandler.h"
#include "json_helpers.h"
#include <Arduino.h>

static SetNavigationAndPowerUseCase* s_useCase = nullptr;

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase) {
    s_useCase = useCase;
}

// POST /set-navigation-and-power
// Body (all fields optional):
//   { "azimuth": 180.0, "elevation": 45.0, "band_0": true, ..., "band_6": false }
// Omitted fields leave current state unchanged.
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res) {
    if (!s_useCase) {
        res.json(503, "{\"error\":\"rotor not available\"}");
        return;
    }

    const char* body = req.params;

    bool  hasAz = jsonHasKey(body, "azimuth");
    bool  hasEl = jsonHasKey(body, "elevation");
    float az    = hasAz ? jsonGetFloat(body, "azimuth",   0.0f) : 0.0f;
    float el    = hasEl ? jsonGetFloat(body, "elevation", 0.0f) : 0.0f;

    static const char* const BAND_KEYS[7] = {
        "band_0", "band_1", "band_2", "band_3", "band_4", "band_5", "band_6"
    };

    // -1 = absent (don't touch pin), 0 = LOW, 1 = HIGH
    int8_t bands[7];
    for (uint8_t i = 0; i < 7; i++) {
        bands[i] = (int8_t)jsonGetBool(body, BAND_KEYS[i], -1);
    }

    char errMsg[48] = {};
    if (!s_useCase->execute(hasAz, az, hasEl, el, bands, errMsg, sizeof(errMsg))) {
        res.badRequest(errMsg);
        return;
    }

    res.json(200, "{\"status\":\"queued\"}");
}
