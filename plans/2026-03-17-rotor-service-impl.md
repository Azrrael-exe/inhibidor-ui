# Rotor Service & Navigation API — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `RotorService` + use-case layer to expose `GET /status` (real rotor angles) and `POST /set-navigation-and-power` (goto az/el + RF bands) on the CONTROLLINO MAXI firmware.

**Architecture:** `RotorService` owns all serial I/O with the G5500 via the `ard-llp` `DataPack` protocol. Use cases (`GetRotorStatusUseCase`, `SetNavigationAndPowerUseCase`) hold business logic. HTTP handlers delegate to use cases. `GpsCompassHandler` is extended — `NavigationHandler` is added.

**Tech Stack:** C++11, Arduino framework, PlatformIO, `ard-llp` (`DataPack`: 182 bytes per instance, `uint8_t buffer[90]` + `uint8_t inp_buffer[90]`), custom `WebServer`, ATmega2560 (8KB SRAM).

**Spec:** `plans/2026-03-17-rotor-service-design.md`

**Build command:** `~/.platformio/penv/bin/pio run`

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Modify | `lib/WebServer/WebServer.h` | Increase `WS_PARAMS_LEN` 128 → 256, update comment |
| Modify | `src/handlers/json_helpers.h` | Add `jsonGetFloat` and `jsonHasKey` helpers |
| Create | `src/services/RotorService.h` | `RotorStatus` struct + `RotorService` declaration |
| Create | `src/services/RotorService.cpp` | All G5500 serial I/O implementation |
| Create | `src/use_cases/GetRotorStatusUseCase.h` | Declaration |
| Create | `src/use_cases/GetRotorStatusUseCase.cpp` | Delegates to `RotorService::readStatus` |
| Create | `src/use_cases/SetNavigationAndPowerUseCase.h` | Declaration |
| Create | `src/use_cases/SetNavigationAndPowerUseCase.cpp` | Validates angles, calls service, writes RF pins |
| Create | `src/handlers/NavigationHandler.h` | `POST /set-navigation-and-power` declaration |
| Create | `src/handlers/NavigationHandler.cpp` | Parses body, calls use case |
| Modify | `src/handlers/GpsCompassHandler.h` | Add `RotorService*` to `initStatusHandler` |
| Modify | `src/handlers/GpsCompassHandler.cpp` | Call `GetRotorStatusUseCase` for real angles |
| Modify | `src/main.cpp` | Wire new globals/handlers, remove `PowerHandler` |

---

## SRAM budget notes

| Item | Bytes |
|------|-------|
| WebServer static | ~466 |
| `WS_PARAMS_LEN` increase (128→256) | +128 |
| `HttpRequest` stack per dispatch | **321** (1+64+256 after increase) |
| `HttpResponse` stack per dispatch | 3 |
| `RotorService::_txPack` (global member) | 182 |
| `RotorService::_rxPack` (global member) | 182 |
| `body[400]` in `handleGetStatus` stack | 400 |

`RotorService` is declared at global scope in `main.cpp` so both `DataPack` members (364 bytes total) live in `.bss`, not the stack. The peak stack during `handleGetStatus` is: `HttpRequest(321) + HttpResponse(3) + body[400] + small locals ≈ 730 bytes` — within the 2KB typical stack budget for ATmega2560.

---

## Task 1: Increase WS_PARAMS_LEN and add JSON helpers

**Files:**
- Modify: `lib/WebServer/WebServer.h`
- Modify: `src/handlers/json_helpers.h`

- [ ] **Step 1: Update `lib/WebServer/WebServer.h`**

Change line 19 and update the `HttpRequest` memory comment on line 47:

```cpp
// Line 19 — change:
#define WS_PARAMS_LEN       256

// Line 47-48 — update comment to:
 * Stack-allocated inside WebServer::update() during dispatch.
 * Memory: 1 + 64 + 256 = 321 bytes.
```

- [ ] **Step 2: Add `jsonGetFloat` and `jsonHasKey` to `src/handlers/json_helpers.h`**

Append at the end of the file:

```cpp
/**
 * Extract a float value from a flat JSON object by key name.
 * Returns defaultVal if the key is not found.
 *
 * Example: jsonGetFloat("{\"az\":180.5}", "az", 0.0f) → 180.5
 */
inline float jsonGetFloat(const char* json, const char* key, float defaultVal) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return defaultVal;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return defaultVal;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    return (float)atof(p);
}

/**
 * Returns true if the key exists anywhere in the JSON object.
 *
 * Example: jsonHasKey("{\"az\":180.5}", "az") → true
 */
inline bool jsonHasKey(const char* json, const char* key) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern) != nullptr;
}
```

- [ ] **Step 3: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add lib/WebServer/WebServer.h src/handlers/json_helpers.h
git commit -m "feat: increase WS_PARAMS_LEN to 256, add jsonGetFloat/jsonHasKey helpers"
```

---

## Task 2: RotorService

**Files:**
- Create: `src/services/RotorService.h`
- Create: `src/services/RotorService.cpp`

- [ ] **Step 1: Create `src/services/RotorService.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <llp.h>

struct RotorStatus {
    float azimuthAngle;    // degrees (G5500 key 0xAB, raw uint16_t / 10.0)
    float elevationAngle;  // degrees (G5500 key 0xBC, raw uint16_t / 10.0)
};

class RotorService {
public:
    explicit RotorService(HardwareSerial* serial);

    void gotoAzimuth(float degrees);    // DataPack key 0xDA, value (int16_t)(deg*10)
    void gotoElevation(float degrees);  // DataPack key 0xDB, value (int16_t)(deg*10)
    void stopAzimuth();                 // DataPack key 0xAA, value 0xA0
    void stopElevation();               // DataPack key 0xBB, value 0xB0

    // Sends feedback poll (0xCC/0xC2), waits for response, parses 0xAB and 0xBC keys.
    // Note: setTimeout(200) only takes effect if at least one byte has already arrived
    // in the RX buffer. If the G5500 has not started responding, available() returns
    // false immediately without waiting. 200ms is an upper bound, not a guarantee.
    // Returns false on timeout, parse failure, or missing keys.
    bool readStatus(RotorStatus& out);

private:
    HardwareSerial* _serial;
    DataPack        _txPack;  // member (182 bytes) — not stack-allocated
    DataPack        _rxPack;  // member (182 bytes) — not stack-allocated
};
```

- [ ] **Step 2: Create `src/services/RotorService.cpp`**

```cpp
#include "RotorService.h"

RotorService::RotorService(HardwareSerial* serial)
    : _serial(serial) {}

void RotorService::gotoAzimuth(float degrees) {
    _txPack.clear();
    _txPack.addData(0xDA, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::gotoElevation(float degrees) {
    _txPack.clear();
    _txPack.addData(0xDB, (int16_t)(degrees * 10));
    _txPack.write(*_serial);
}

void RotorService::stopAzimuth() {
    _txPack.clear();
    _txPack.addData(0xAA, (int16_t)0xA0);
    _txPack.write(*_serial);
}

void RotorService::stopElevation() {
    _txPack.clear();
    _txPack.addData(0xBB, (int16_t)0xB0);
    _txPack.write(*_serial);
}

bool RotorService::readStatus(RotorStatus& out) {
    // Flush RX buffer
    while (_serial->available()) _serial->read();

    // Send feedback poll
    _txPack.clear();
    _txPack.addData(0xCC, (int16_t)0xC2);
    _txPack.write(*_serial);

    // available() blocks via Stream::readBytes which respects setTimeout.
    // The 200ms timeout applies once bytes start arriving.
    _serial->setTimeout(200);
    // inp_buffer is overwritten by available() on success — clear() is not needed for RX.
    if (!_rxPack.available(*_serial)) return false;
    if (!_rxPack.hasKey(0xAB) || !_rxPack.hasKey(0xBC)) return false;

    out.azimuthAngle   = _rxPack.getData(0xAB) / 10.0f;
    out.elevationAngle = _rxPack.getData(0xBC) / 10.0f;
    return true;
}
```

- [ ] **Step 3: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`. If `<llp.h>` fails to resolve, change to `"llp.h"` (PlatformIO injects lib_deps paths as both forms).

- [ ] **Step 4: Commit**

```bash
git add src/services/RotorService.h src/services/RotorService.cpp
git commit -m "feat: add RotorService with goto, stop, and readStatus commands"
```

---

## Task 3: GetRotorStatusUseCase + extend GpsCompassHandler

**Files:**
- Create: `src/use_cases/GetRotorStatusUseCase.h`
- Create: `src/use_cases/GetRotorStatusUseCase.cpp`
- Modify: `src/handlers/GpsCompassHandler.h`
- Modify: `src/handlers/GpsCompassHandler.cpp`

- [ ] **Step 1: Create `src/use_cases/GetRotorStatusUseCase.h`**

```cpp
#pragma once
#include "../services/RotorService.h"

class GetRotorStatusUseCase {
public:
    explicit GetRotorStatusUseCase(RotorService* service);
    bool execute(RotorStatus& out);

private:
    RotorService* _service;
};
```

- [ ] **Step 2: Create `src/use_cases/GetRotorStatusUseCase.cpp`**

```cpp
#include "GetRotorStatusUseCase.h"

GetRotorStatusUseCase::GetRotorStatusUseCase(RotorService* service)
    : _service(service) {}

bool GetRotorStatusUseCase::execute(RotorStatus& out) {
    return _service->readStatus(out);
}
```

- [ ] **Step 3: Update `src/handlers/GpsCompassHandler.h`**

```cpp
#pragma once
#include <WebServer.h>
#include "../modules/GpsModule.h"
#include "../modules/CompassModule.h"
#include "../services/RotorService.h"

void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor = nullptr);
void handleGetStatus(const HttpRequest& req, HttpResponse& res);
```

- [ ] **Step 4: Replace `src/handlers/GpsCompassHandler.cpp`**

```cpp
#include "GpsCompassHandler.h"
#include "../use_cases/GetRotorStatusUseCase.h"
#include "../pinout.h"
#include <Arduino.h>
#include <stdio.h>

static GpsModule*     s_gps     = nullptr;
static CompassModule* s_compass = nullptr;
static RotorService*  s_rotor   = nullptr;

void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor) {
    s_gps     = gps;
    s_compass = compass;
    s_rotor   = rotor;
}

// GET /status
void handleGetStatus(const HttpRequest& req, HttpResponse& res) {
    if (!s_gps || !s_compass) {
        res.json(503, "{\"error\":\"module not initialized\"}");
        return;
    }

    const GpsData&     g = s_gps->getData();
    const CompassData& c = s_compass->getData();

    char lat[14], lon[14], alt[10], hdg[8];
    char dt[22];

    dtostrf(g.latitude,  1, 6, lat);
    dtostrf(g.longitude, 1, 6, lon);
    dtostrf(g.altitude,  1, 1, alt);
    dtostrf(c.heading,   1, 1, hdg);

    snprintf(dt, sizeof(dt), "%04u-%02u-%02uT%02u:%02u:%02uZ",
             (unsigned)g.year,   (unsigned)g.month,  (unsigned)g.day,
             (unsigned)g.hour,   (unsigned)g.minute,  (unsigned)g.second);

    // Rotor status — query G5500; degrade to "0.0" on timeout or if not connected
    char nav_az[8] = "0.0";
    char nav_el[8] = "0.0";
    if (s_rotor) {
        RotorStatus rs;
        GetRotorStatusUseCase getStatus(s_rotor);
        if (getStatus.execute(rs)) {
            dtostrf(rs.azimuthAngle,   1, 1, nav_az);
            dtostrf(rs.elevationAngle, 1, 1, nav_el);
        }
    }

    const char* b0 = digitalRead(RF_BAND_0) ? "true" : "false";
    const char* b1 = digitalRead(RF_BAND_1) ? "true" : "false";
    const char* b2 = digitalRead(RF_BAND_2) ? "true" : "false";
    const char* b3 = digitalRead(RF_BAND_3) ? "true" : "false";
    const char* b4 = digitalRead(RF_BAND_4) ? "true" : "false";
    const char* b5 = digitalRead(RF_BAND_5) ? "true" : "false";
    const char* b6 = digitalRead(RF_BAND_6) ? "true" : "false";

    char body[400];
    snprintf(body, sizeof(body),
        "{"
          "\"gps\":{"
            "\"lat\":\"%s\","
            "\"lon\":\"%s\","
            "\"alt\":\"%s\","
            "\"datetime\":\"%s\""
          "},"
          "\"heading\":\"%s\","
          "\"navigation\":{"
            "\"azimuth\":\"%s\","
            "\"elevation\":\"%s\""
          "},"
          "\"power\":{"
            "\"band_0\":%s,"
            "\"band_1\":%s,"
            "\"band_2\":%s,"
            "\"band_3\":%s,"
            "\"band_4\":%s,"
            "\"band_5\":%s,"
            "\"band_6\":%s"
          "}"
        "}",
        lat, lon, alt, dt, hdg,
        nav_az, nav_el,
        b0, b1, b2, b3, b4, b5, b6
    );

    res.json(200, body);
}
```

- [ ] **Step 5: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`. The existing `initStatusHandler(&gpsModule, &compassModule)` call in `main.cpp` still compiles (third param defaults to `nullptr`).

- [ ] **Step 6: Commit**

```bash
git add src/use_cases/GetRotorStatusUseCase.h src/use_cases/GetRotorStatusUseCase.cpp \
        src/handlers/GpsCompassHandler.h src/handlers/GpsCompassHandler.cpp
git commit -m "feat: add GetRotorStatusUseCase, extend /status with real rotor angles"
```

---

## Task 4: SetNavigationAndPowerUseCase

**Files:**
- Create: `src/use_cases/SetNavigationAndPowerUseCase.h`
- Create: `src/use_cases/SetNavigationAndPowerUseCase.cpp`

`bands[7]` uses `int8_t` to encode three states per band:
- `-1` = key absent — do not touch this pin
- `0` = key present, value false — drive LOW
- `1` = key present, value true — drive HIGH

This matches the `jsonGetBool` return convention (`-1`/`0`/`1`) and preserves the spec requirement that omitted fields leave current state unchanged.

- [ ] **Step 1: Create `src/use_cases/SetNavigationAndPowerUseCase.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "../services/RotorService.h"

class SetNavigationAndPowerUseCase {
public:
    explicit SetNavigationAndPowerUseCase(RotorService* service);

    // bands[i]: -1 = absent (don't touch), 0 = LOW, 1 = HIGH.
    // errorMsg: caller-owned stack buffer written on validation failure.
    // Returns true on success, false if validation fails.
    bool execute(
        bool     hasAz,    float az,
        bool     hasEl,    float el,
        int8_t   bands[7],
        char*    errorMsg, uint8_t errorMsgLen
    );

private:
    RotorService* _service;
};
```

- [ ] **Step 2: Create `src/use_cases/SetNavigationAndPowerUseCase.cpp`**

```cpp
#include "SetNavigationAndPowerUseCase.h"
#include "../pinout.h"

static const uint8_t BAND_PINS[7] = {
    RF_BAND_0, RF_BAND_1, RF_BAND_2, RF_BAND_3,
    RF_BAND_4, RF_BAND_5, RF_BAND_6
};

SetNavigationAndPowerUseCase::SetNavigationAndPowerUseCase(RotorService* service)
    : _service(service) {}

bool SetNavigationAndPowerUseCase::execute(
    bool     hasAz,    float az,
    bool     hasEl,    float el,
    int8_t   bands[7],
    char*    errorMsg, uint8_t errorMsgLen
) {
    if (hasAz && (az < 0.0f || az > 360.0f)) {
        strncpy(errorMsg, "azimuth out of range [0,360]", errorMsgLen - 1);
        errorMsg[errorMsgLen - 1] = '\0';
        return false;
    }
    if (hasEl && (el < 0.0f || el > 90.0f)) {
        strncpy(errorMsg, "elevation out of range [0,90]", errorMsgLen - 1);
        errorMsg[errorMsgLen - 1] = '\0';
        return false;
    }

    if (hasAz) _service->gotoAzimuth(az);
    if (hasEl)  _service->gotoElevation(el);

    // bands[i] == -1 means the key was absent — leave that pin unchanged.
    for (uint8_t i = 0; i < 7; i++) {
        if (bands[i] >= 0) {
            digitalWrite(BAND_PINS[i], bands[i] ? HIGH : LOW);
        }
    }

    return true;
}
```

- [ ] **Step 3: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/use_cases/SetNavigationAndPowerUseCase.h src/use_cases/SetNavigationAndPowerUseCase.cpp
git commit -m "feat: add SetNavigationAndPowerUseCase with angle validation and per-band RF control"
```

---

## Task 5: NavigationHandler

**Files:**
- Create: `src/handlers/NavigationHandler.h`
- Create: `src/handlers/NavigationHandler.cpp`

- [ ] **Step 1: Create `src/handlers/NavigationHandler.h`**

```cpp
#pragma once
#include <WebServer.h>
#include "../use_cases/SetNavigationAndPowerUseCase.h"

void initNavigationHandler(SetNavigationAndPowerUseCase* useCase);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
```

- [ ] **Step 2: Create `src/handlers/NavigationHandler.cpp`**

```cpp
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

    res.json(200, "{\"status\":\"ok\"}");
}
```

- [ ] **Step 3: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add src/handlers/NavigationHandler.h src/handlers/NavigationHandler.cpp
git commit -m "feat: add NavigationHandler for POST /set-navigation-and-power"
```

---

## Task 6: Wire main.cpp, remove PowerHandler

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>

#include "pinout.h"
#include <DigitalSwitch.h>
#include "callback.h"
#include <Controllino.h>
#include <Ethernet.h>
#include <Wire.h>
#include <WebServer.h>
#include "handlers/GpsCompassHandler.h"
#include "handlers/NavigationHandler.h"
#include "modules/GpsModule.h"
#include "modules/CompassModule.h"
#include "services/RotorService.h"
#include "use_cases/SetNavigationAndPowerUseCase.h"

// ─── Network configuration ────────────────────────────────────────────────────
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress fallbackIp(192, 168, 1, 100);

WebServer webServer(80);

GpsModule     gpsModule;
CompassModule compassModule;
RotorService  rotorService(&Serial);  // _txPack + _rxPack = 364 bytes in .bss

SetNavigationAndPowerUseCase setNavAndPowerUseCase(&rotorService);

DigitalSwitch azimuthForwardSwitch(AZIMUTH_FORWARD_PIN);
DigitalSwitch azimuthBackwardSwitch(AZIMUTH_BACKWARD_PIN);
DigitalSwitch elevationForwardSwitch(ELEVATION_FORWARD_PIN);
DigitalSwitch elevationBackwardSwitch(ELEVATION_BACKWARD_PIN);
DigitalSwitch rfPowerSwitch(RF_POWER_PIN);

G5500CommandContext azimuthForwardContext   = { &Serial, AZIMUTH_HEADER,   AZIMUTH_FORWARD   };
G5500CommandContext azimuthBackwardContext  = { &Serial, AZIMUTH_HEADER,   AZIMUTH_BACKWARD  };
G5500CommandContext azimuthStopContext      = { &Serial, AZIMUTH_HEADER,   AZIMUTH_STOP      };
G5500CommandContext elevationForwardContext  = { &Serial, ELEVATION_HEADER, ELEVATION_FORWARD  };
G5500CommandContext elevationBackwardContext = { &Serial, ELEVATION_HEADER, ELEVATION_BACKWARD };
G5500CommandContext elevationStopContext     = { &Serial, ELEVATION_HEADER, ELEVATION_STOP     };

void activateRFPower(void* context) {
    int8_t bands[7] = { 1, 1, 1, 1, 1, 1, 1 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void deactivateRFPower(void* context) {
    int8_t bands[7] = { 0, 0, 0, 0, 0, 0, 0 };
    char err[48];
    setNavAndPowerUseCase.execute(false, 0.0f, false, 0.0f, bands, err, sizeof(err));
}

void setup() {
    Serial.begin(115200);
    gpsModule.begin(Serial1, 38400);

    if (Ethernet.begin(mac) == 0) {
        Ethernet.begin(mac, fallbackIp);
        Serial.print(F("[WebServer] DHCP failed, using static IP: "));
    } else {
        Serial.print(F("[WebServer] DHCP OK, IP: "));
    }
    Serial.println(Ethernet.localIP());
    delay(1000);

    compassModule.begin();
    initStatusHandler(&gpsModule, &compassModule, &rotorService);
    initNavigationHandler(&setNavAndPowerUseCase);

    webServer.begin();
    webServer.on("/status",                   HTTP_GET,  handleGetStatus);
    webServer.on("/set-navigation-and-power", HTTP_POST, handleSetNavigationAndPower);

    pinMode(RF_BAND_0, OUTPUT);
    pinMode(RF_BAND_1, OUTPUT);
    pinMode(RF_BAND_2, OUTPUT);
    pinMode(RF_BAND_3, OUTPUT);
    pinMode(RF_BAND_4, OUTPUT);
    pinMode(RF_BAND_5, OUTPUT);
    pinMode(RF_BAND_6, OUTPUT);

    azimuthForwardSwitch.begin();
    azimuthBackwardSwitch.begin();
    elevationForwardSwitch.begin();
    elevationBackwardSwitch.begin();
    rfPowerSwitch.begin();

    azimuthForwardSwitch.setOnTurnOn(sendG5500Command, &azimuthForwardContext);
    azimuthBackwardSwitch.setOnTurnOn(sendG5500Command, &azimuthBackwardContext);
    azimuthForwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);
    azimuthBackwardSwitch.setOnTurnOff(sendG5500Command, &azimuthStopContext);

    elevationForwardSwitch.setOnTurnOn(sendG5500Command, &elevationForwardContext);
    elevationBackwardSwitch.setOnTurnOn(sendG5500Command, &elevationBackwardContext);
    elevationForwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);
    elevationBackwardSwitch.setOnTurnOff(sendG5500Command, &elevationStopContext);

    rfPowerSwitch.setOnTurnOn(activateRFPower);
    rfPowerSwitch.setOnTurnOff(deactivateRFPower);
}

void loop() {
    webServer.update();

    gpsModule.update();
    compassModule.update();

    azimuthForwardSwitch.update();
    azimuthBackwardSwitch.update();
    elevationForwardSwitch.update();
    elevationBackwardSwitch.update();
    rfPowerSwitch.update();
}
```

- [ ] **Step 2: Build**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`.

- [ ] **Step 3: Remove deprecated PowerHandler**

```bash
git rm src/handlers/PowerHandler.h src/handlers/PowerHandler.cpp
```

- [ ] **Step 4: Build again to confirm clean removal**

```bash
~/.platformio/penv/bin/pio run
```

Expected: `SUCCESS`.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat: wire RotorService and NavigationHandler in main, remove PowerHandler"
```

---

## Task 7: Verification

With the firmware flashed to the CONTROLLINO MAXI (replace `<device-ip>` with the actual IP printed on boot):

- [ ] **Step 1: Verify GET /status returns real navigation angles**

```bash
curl -s http://<device-ip>/status | python3 -m json.tool
```

Expected: `navigation.azimuth` and `navigation.elevation` are real float strings (e.g. `"182.3"`). If G5500 is not connected they degrade to `"0.0"`.

- [ ] **Step 2: Verify POST — navigation angles only**

```bash
curl -s -X POST http://<device-ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":180.0,"elevation":45.0}'
```

Expected: `{"status":"ok"}`

- [ ] **Step 3: Verify POST — bands only, partial (only band_0)**

```bash
curl -s -X POST http://<device-ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"band_0":true}'
```

Expected: `{"status":"ok"}`, only band_0 pin changes — bands 1–6 unchanged.

- [ ] **Step 4: Verify POST — full combined body**

```bash
curl -s -X POST http://<device-ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":90.0,"elevation":30.0,"band_0":true,"band_1":true,"band_2":true,"band_3":true,"band_4":true,"band_5":true,"band_6":true}'
```

Expected: `{"status":"ok"}`

- [ ] **Step 5: Verify validation errors**

```bash
# Azimuth out of range
curl -s -X POST http://<device-ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":400.0}'
```

Expected: HTTP 400, `{"error":"azimuth out of range [0,360]"}`

```bash
# Elevation out of range
curl -s -X POST http://<device-ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"elevation":95.0}'
```

Expected: HTTP 400, `{"error":"elevation out of range [0,90]"}`

- [ ] **Step 6: Final commit**

```bash
git tag v0.2.0-rotor-service
```
