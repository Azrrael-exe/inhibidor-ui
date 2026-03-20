# Rotor Service & Navigation API — Design Spec

**Date:** 2026-03-17
**Project:** inhibidor-ui (CONTROLLINO MAXI / ATmega2560)
**Status:** Approved

---

## Problem

The current codebase has no service or use-case layer. G5500 rotor commands are only triggered by physical switches via callbacks. There is no HTTP API to control rotor position or query real rotor angles — `navigation.azimuth` and `navigation.elevation` in `/status` are hardcoded to `"0.0"`.

---

## Goal

1. Expose two HTTP endpoints to control and monitor the G5500 rotor.
2. Introduce a `RotorService` and use-case layer so rotor logic is reusable and testable independently of HTTP handlers.

---

## API

### `GET /status` (extended)

No structural changes to the response. `navigation.azimuth` and `navigation.elevation` are populated with real angles read from the G5500 via serial feedback. If the G5500 does not respond (timeout), both fields degrade gracefully to `"0.0"`.

```json
{
  "gps": { "lat": "19.432608", "lon": "-99.133209", "alt": "2240.0", "datetime": "2026-02-24T15:30:00Z" },
  "heading": "273.4",
  "navigation": {
    "azimuth": "182.3",
    "elevation": "44.7"
  },
  "power": {
    "band_0": true,
    "band_1": false,
    "band_2": true,
    "band_3": false,
    "band_4": true,
    "band_5": false,
    "band_6": true
  }
}
```

---

### `POST /set-navigation-and-power`

Combines rotor goto and RF band control in a single request. All fields are optional — omitted fields leave current state unchanged.

**Request body:**
```json
{
  "azimuth": 180.0,
  "elevation": 45.0,
  "band_0": true,
  "band_1": false,
  "band_2": true,
  "band_3": false,
  "band_4": true,
  "band_5": false,
  "band_6": true
}
```

| Field | Type | Range | Required |
|-------|------|-------|----------|
| `azimuth` | float | 0.0 – 360.0 | No |
| `elevation` | float | 0.0 – 90.0 | No |
| `band_0` … `band_6` | boolean | — | No |

**Response:** `{ "status": "ok" }`

**Error responses:**

| Condition | HTTP | Body |
|-----------|------|------|
| `azimuth` out of range | 400 | `{"error":"azimuth out of range [0,360]"}` |
| `elevation` out of range | 400 | `{"error":"elevation out of range [0,90]"}` |
| Rotor serial not initialized | 503 | `{"error":"rotor not available"}` |

#### `WS_PARAMS_LEN` increase required

The worst-case body for this endpoint is approximately 130 bytes, which exceeds the current `WS_PARAMS_LEN = 128` in `lib/WebServer/WebServer.h`. This constant must be increased to `256`. Cost: +128 bytes SRAM (see budget below).

---

## Architecture

### New directories

```
src/
├── services/
│   └── RotorService.h/cpp
├── use_cases/
│   ├── GetRotorStatusUseCase.h/cpp
│   └── SetNavigationAndPowerUseCase.h/cpp
└── handlers/
    └── NavigationHandler.h/cpp          ← POST /set-navigation-and-power only
```

**Ownership of `GET /status`:** stays in `GpsCompassHandler` (extended to accept `RotorService*`). `NavigationHandler` only handles `POST /set-navigation-and-power`.

**`POST /set-power`:** deprecated. Its pin-write logic moves into `SetNavigationAndPowerUseCase`. The `PowerHandler` registration in `main.cpp` is removed.

---

## Components

### `RotorStatus` struct (defined in `RotorService.h`)

```cpp
struct RotorStatus {
    float azimuthAngle;    // degrees — raw uint16_t from key 0xAB divided by 10.0
    float elevationAngle;  // degrees — raw uint16_t from key 0xBC divided by 10.0
};
```

---

### `RotorService`

Owns the serial reference. All serial I/O with the G5500 goes through this class.

```cpp
class RotorService {
public:
    explicit RotorService(HardwareSerial* serial);

    void gotoAzimuth(float degrees);   // addData(0xDA, (int16_t)(degrees * 10))
    void gotoElevation(float degrees); // addData(0xDB, (int16_t)(degrees * 10))
    void stopAzimuth();                // addData(0xAA, (int16_t)0xA0)
    void stopElevation();              // addData(0xBB, (int16_t)0xB0)
    bool readStatus(RotorStatus& out); // send poll, parse response, 200ms timeout
                                       // returns false on timeout or parse failure

private:
    HardwareSerial* _serial;
    DataPack        _txPack;  // member variable — avoids stack allocation
    DataPack        _rxPack;  // member variable — avoids stack allocation
};
```

**Protocol encoding — DataPack API usage:**

All commands use `DataPack::addData(uint8_t key, int16_t value)`:
- `gotoAzimuth(az)` → `_txPack.addData(0xDA, (int16_t)(az * 10))`
  Valid range enforced by use case: 0–360° → 0–3600, fits in `int16_t`.
- `gotoElevation(el)` → `_txPack.addData(0xDB, (int16_t)(el * 10))`
  Valid range: 0–90° → 0–900, fits in `int16_t`.
- `stopAzimuth()` → `_txPack.addData(0xAA, (int16_t)0xA0)` (consistent with existing `sendG5500Command`)
- `stopElevation()` → `_txPack.addData(0xBB, (int16_t)0xB0)`

**`readStatus` flow:**

1. Flush `_serial` RX buffer (read and discard all pending bytes).
2. Build poll frame: `_txPack.addData(0xCC, (int16_t)0xC2)` → `_txPack.write(*_serial)`.
3. Set `_serial->setTimeout(200)` before calling `_rxPack.available(*_serial)`.
4. If `available()` returns false (timeout/parse failure), return `false`.
5. Extract: `out.azimuthAngle = _rxPack.getData(0xAB) / 10.0f`
6. Extract: `out.elevationAngle = _rxPack.getData(0xBC) / 10.0f`
7. Return `true`.

> **Blocking note:** `readStatus` blocks the main loop for up to 200ms while waiting for the G5500 serial response. This is acceptable because it is only called from within an HTTP request handler (user-initiated, not called every loop tick).

> **`getKeys()` must NOT be called** — it uses `calloc` and causes heap fragmentation on the ATmega. Use only `hasKey()` and `getData()`.

---

### `GetRotorStatusUseCase`

```cpp
class GetRotorStatusUseCase {
public:
    explicit GetRotorStatusUseCase(RotorService* service);
    bool execute(RotorStatus& out);
private:
    RotorService* _service;
};
```

Delegates directly to `RotorService::readStatus`.

---

### `SetNavigationAndPowerUseCase`

```cpp
class SetNavigationAndPowerUseCase {
public:
    explicit SetNavigationAndPowerUseCase(RotorService* service);

    bool execute(
        bool hasAz,    float az,
        bool hasEl,    float el,
        bool hasBands, bool bands[7],
        char* errorMsg, uint8_t errorMsgLen  // caller-owned stack buffer
    );

private:
    RotorService* _service;
};
```

`errorMsg` is a caller-provided stack buffer (e.g. `char err[48]`). Written via `strncpy`. No `String` objects, no heap.

Execution order:
1. If `hasAz` and `az` not in `[0.0, 360.0]` → write error, return `false`.
2. If `hasEl` and `el` not in `[0.0, 90.0]` → write error, return `false`.
3. If `hasAz` → `_service->gotoAzimuth(az)`.
4. If `hasEl` → `_service->gotoElevation(el)`.
5. If `hasBands` → `digitalWrite(RF_BAND_0..6, bands[0..6])`.
6. Return `true`.

The RF band `digitalWrite` logic lives exclusively here. `PowerHandler` is removed.

---

### `NavigationHandler`

```cpp
void initNavigationHandler(SetNavigationAndPowerUseCase* useCase);
void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res);
```

Parses JSON body using `json_helpers.h`. Extracts `azimuth`, `elevation`, `band_0`–`band_6` with presence flags. Calls `SetNavigationAndPowerUseCase::execute`. Uses a local `char err[48]` buffer for error messages.

---

### `GpsCompassHandler` extension

`initStatusHandler` gains a third optional parameter:
```cpp
void initStatusHandler(GpsModule* gps, CompassModule* compass, RotorService* rotor = nullptr);
```

Inside `handleGetStatus`:
- If `rotor != nullptr`, instantiate `GetRotorStatusUseCase` and call `execute(status)`.
- On success: format `azimuthAngle` / `elevationAngle` into `nav_az` / `nav_el` char buffers with `dtostrf`.
- On failure or `rotor == nullptr`: use `"0.0"` fallback.

---

## Data flow

```
GET /status:
  handleGetStatus
    → GetRotorStatusUseCase::execute
      → RotorService::readStatus (Serial poll, 200ms timeout)
    → fill navigation.azimuth, navigation.elevation in snprintf

POST /set-navigation-and-power:
  handleSetNavigationAndPower
    → SetNavigationAndPowerUseCase::execute
      → validate az/el
      → RotorService::gotoAzimuth (if hasAz)
      → RotorService::gotoElevation (if hasEl)
      → digitalWrite RF_BAND_x (if hasBands)
    → 200 {"status":"ok"} or 400/503 on error
```

---

## `main.cpp` changes

1. Increase `WS_PARAMS_LEN` to `256` in `lib/WebServer/WebServer.h`.
2. Remove `PowerHandler` include and its `webServer.on("/set-power", ...)` registration.
3. Add: `RotorService rotorService(&Serial);`
4. Add: `SetNavigationAndPowerUseCase setNavAndPowerUseCase(&rotorService);`
5. Change: `initStatusHandler(&gpsModule, &compassModule, &rotorService);`
6. Add: `initNavigationHandler(&setNavAndPowerUseCase);`
7. Add: `webServer.on("/set-navigation-and-power", HTTP_POST, handleSetNavigationAndPower);`

---

## SRAM budget

| Item | Bytes |
|------|-------|
| WebServer static | ~466 |
| `WS_PARAMS_LEN` increase (128→256) | +128 |
| `HttpRequest` stack (per dispatch) | 193 |
| `HttpResponse` stack (per dispatch) | 3 |
| `RotorService` members (`_txPack` + `_rxPack`) | TBD from ard-llp sizeof |
| GPS/Compass modules | existing |

`DataPack` instances are **member variables** of `RotorService` (not stack-allocated) to avoid stack overflow during the call chain: `update() → _dispatch() → handleGetStatus() → readStatus()`.

---

## Protocol reference (from rotor-controller/src/protocol.h)

| Command | Key | Value | Description |
|---------|-----|-------|-------------|
| Azimuth stop | `0xAA` | `(int16_t)0xA0` | Stop azimuth motor |
| Elevation stop | `0xBB` | `(int16_t)0xB0` | Stop elevation motor |
| Goto azimuth | `0xDA` | `(int16_t)(deg * 10)` | Go to angle (0–360°, encoded ×10) |
| Goto elevation | `0xDB` | `(int16_t)(deg * 10)` | Go to angle (0–90°, encoded ×10) |
| Read all feedback | `0xCC` | `(int16_t)0xC2` | Request angles |
| Az angle response | `0xAB` | `getData(0xAB) / 10.0f` | Azimuth angle ×10 |
| El angle response | `0xBC` | `getData(0xBC) / 10.0f` | Elevation angle ×10 |

LLP packet format: `[0x7E][LENGTH][KEY][MSB][LSB][CHECKSUM]`
Library: `ard-llp` (`DataPack` class). Permitted methods: `addData`, `write`, `available`, `hasKey`, `getData`. Do NOT call `getKeys()`.

---

## Out of scope

- `/stop-navigation` endpoint.
- Azimuth ranges above 360° (G5500 hardware supports 0–450°, API exposes 0–360°).
- Asynchronous rotor state polling.
- Voltage readback (`0xAA`/`0xBB` feedback keys) — only angle keys `0xAB`/`0xBC` used.
