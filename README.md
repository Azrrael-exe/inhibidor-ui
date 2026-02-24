# inhibidor-ui

Sistema de control embebido para plataforma CONTROLLINO MAXI (ATmega2560).
Expone una API HTTP en el puerto 80 para monitoreo y control del sistema.

---

## API Reference

Base URL: `http://<ip-del-dispositivo>`

Todas las respuestas son JSON. En caso de error, la respuesta incluye un campo `error` con descripción del problema (HTTP 4xx).

---

### GET /status

Retorna el estado completo del sistema.

**Response:**

```json
{
  "gps": {
    "lat": "19.432608",
    "lon": "-99.133209",
    "alt": "2240.0",
    "datetime": "2026-02-24T15:30:00Z"
  },
  "heading": "273.4",
  "navigation": {
    "azimuth": "0.0",
    "elevation": "0.0"
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

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `gps.lat` | string | Latitud en grados decimales |
| `gps.lon` | string | Longitud en grados decimales |
| `gps.alt` | string | Altitud en metros |
| `gps.datetime` | string | Fecha y hora UTC en formato ISO 8601 |
| `heading` | string | Rumbo del compás en grados (0–360) |
| `navigation.azimuth` | string | Azimuth objetivo actual en grados (0–360) |
| `navigation.elevation` | string | Elevación objetivo actual en grados (0–90) |
| `power.band_0` … `power.band_6` | boolean | Estado actual de cada banda de RF |

---

### POST /set-navigation

Establece los valores de azimuth y elevación para orientar el sistema.

**Request body:**

```json
{
  "azimuth": 180.0,
  "elevation": 45.0
}
```

| Campo | Tipo | Rango | Requerido |
|-------|------|-------|-----------|
| `azimuth` | float | 0.0 – 360.0 | Sí |
| `elevation` | float | 0.0 – 90.0 | Sí |

**Response:**

```json
{ "status": "ok" }
```

---

### POST /stop-navigation

Detiene completamente el movimiento del sistema. No requiere body.

**Response:**

```json
{ "status": "ok" }
```

---

### POST /set-power

Activa o desactiva las bandas de RF del sistema (bandas 1–6).
Se puede controlar una sola banda o todas en un mismo request.
Los campos no enviados no modifican el estado actual de esa banda.

**Request body:**

```json
{
  "band_0": true,
  "band_1": false,
  "band_2": true,
  "band_3": false,
  "band_4": true,
  "band_5": false,
  "band_6": true
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `band_0` … `band_6` | boolean | `true` activa la banda, `false` la desactiva (todos opcionales) |

**Response:**

```json
{ "status": "ok" }
```

---

## Hardware

| Componente | Interfaz | Descripción |
|-----------|----------|-------------|
| CONTROLLINO MAXI | — | MCU principal (ATmega2560) |
| BE-880Q | Serial1 @ 38400 | Receptor GPS, protocolo NMEA |
| QMC5883L | I2C (0x0D) | Compás magnético |
| G5500 | Serial @ 115200 | Controlador de rotor (azimuth/elevación) |
| W5100 | SPI | Módulo Ethernet |
