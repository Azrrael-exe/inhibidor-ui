# inhibidor-ui

Sistema de control embebido para plataforma CONTROLLINO MAXI (ATmega2560).
Expone una API HTTP en el puerto 80 para monitoreo y control del sistema.

---

## API Reference

Base URL: `http://<ip-del-dispositivo>`

Todas las respuestas son JSON. En caso de error, la respuesta incluye un campo `error` con descripción del problema (HTTP 4xx/5xx).

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

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `gps.lat` | string | Latitud en grados decimales |
| `gps.lon` | string | Longitud en grados decimales |
| `gps.alt` | string | Altitud en metros |
| `gps.datetime` | string | Fecha y hora UTC en formato ISO 8601 |
| `heading` | string | Rumbo del compás en grados (0–360) |
| `navigation.azimuth` | string | Ángulo actual del rotor en azimuth (0–360°). `"0.0"` si el G5500 no responde. |
| `navigation.elevation` | string | Ángulo actual del rotor en elevación (0–90°). `"0.0"` si el G5500 no responde. |
| `power.band_0` … `power.band_6` | boolean | Estado actual de cada banda de RF |

**Ejemplo:**

```bash
curl http://<ip>/status
```

---

### POST /set-navigation-and-power

Envía ángulos de goto al rotor y/o activa/desactiva bandas de RF en un solo request.
Todos los campos son opcionales — los campos omitidos no modifican el estado actual.

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

| Campo | Tipo | Rango | Descripción |
|-------|------|-------|-------------|
| `azimuth` | float | 0.0 – 360.0 | Ángulo de destino en azimuth (opcional) |
| `elevation` | float | 0.0 – 90.0 | Ángulo de destino en elevación (opcional) |
| `band_0` … `band_6` | boolean | — | `true` activa la banda, `false` la desactiva (opcionales) |

**Response:**

```json
{ "status": "ok" }
```

**Errores:**

| HTTP | Condición |
|------|-----------|
| 400 | `azimuth` fuera de rango `[0, 360]` |
| 400 | `elevation` fuera de rango `[0, 90]` |
| 503 | Rotor no disponible (no inicializado) |

**Ejemplos:**

```bash
# Solo ángulos
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":180.0,"elevation":45.0}'

# Solo bandas de RF (band_0 ON, resto sin cambio)
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"band_0":true}'

# Apagar todas las bandas
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"band_0":false,"band_1":false,"band_2":false,"band_3":false,"band_4":false,"band_5":false,"band_6":false}'

# Combinado: ángulos + todas las bandas ON
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":90.0,"elevation":30.0,"band_0":true,"band_1":true,"band_2":true,"band_3":true,"band_4":true,"band_5":true,"band_6":true}'

# Error esperado — azimuth fuera de rango
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":400.0}'
# → HTTP 400  {"error":"azimuth out of range [0,360]"}
```

---

## UI de control (Streamlit)

El archivo `ui.py` es una interfaz web de página única para operar el sistema sin necesidad de usar `curl` manualmente.

### Requisitos

- [uv](https://docs.astral.sh/uv/getting-started/installation/) instalado en el sistema.

Las dependencias Python (`streamlit`, `requests`) se resuelven automáticamente al ejecutar el script — no se requiere crear un entorno virtual ni un `requirements.txt`.

### Ejecución

```bash
uv run ui.py
```

La interfaz queda disponible en `http://localhost:8501` y se abre automáticamente en el navegador.

### Funcionalidades

- **Monitoreo en tiempo real** — refresco automático configurable (1–60 s) del estado completo: GPS, heading, posición del rotor y estado de las 7 bandas RF.
- **Control de navegación** — sliders para enviar ángulos de destino al rotor (azimuth 0–450°, elevación 0–180°).
- **Control de bandas RF** — checkboxes individuales pre-rellenados con el estado actual del dispositivo, más botones de acceso rápido "All ON" / "All OFF".
- **Semántica patch** — solo se envían los campos seleccionados; los campos omitidos no modifican el estado del dispositivo.
- **Resiliencia** — si el dispositivo no responde, la UI muestra el último estado conocido con un aviso de error sin interrumpir la operación.

### Configuración desde la UI

En el panel lateral (sidebar) se puede ajustar:

| Campo | Descripción | Default |
|-------|-------------|---------|
| Device IP | Dirección IP del CONTROLLINO | `192.168.1.100` |
| Refresh interval | Segundos entre cada consulta de estado | `3` |
| Auto-refresh | Activar/desactivar refresco automático | Activado |

---

## Hardware

| Componente | Interfaz | Descripción |
|-----------|----------|-------------|
| CONTROLLINO MAXI | — | MCU principal (ATmega2560) |
| BE-880Q | Serial1 @ 38400 | Receptor GPS, protocolo NMEA |
| QMC5883L | I2C (0x0D) | Compás magnético |
| G5500 | Serial @ 115200 | Controlador de rotor (azimuth/elevación) |
| W5100 | SPI | Módulo Ethernet |

---

## Known Issues

### `ard-llp` include guard typo

The vendored `ard-llp` library has a typo in `llp.h` — the include guard macro is `#define fame_h` instead of `#define frame_h`, which means the guard never fires and the header can be included multiple times causing `DataPack` redefinition errors.

**Fix:** After PlatformIO downloads the dependency (i.e. after the first `pio run`), manually patch `.pio/libdeps/controllino_maxi/ard-llp/llp.h` line 2:

```
// Change:
#define fame_h
// To:
#define frame_h
```

This must be re-applied after `pio run --target clean` or a fresh clone.
