# inhibidor-ui

Sistema de control embebido para plataforma CONTROLLINO MAXI (ATmega2560).
Expone una API HTTP en el puerto 80 para monitoreo y control del sistema.

---

## API Reference

Base URL: `http://<ip-del-dispositivo>`

Todas las respuestas son JSON. En caso de error, la respuesta incluye un campo `error` con descripción del problema (HTTP 4xx/5xx).

### Correlación request ↔ response (`request_id`)

Los endpoints `GET /status` y `POST /set-navigation-and-power` aceptan un campo opcional **`request_id`** que el servidor echo-devuelve en el cuerpo de la respuesta para que el cliente pueda correlacionar pares request/response (útil con reintentos, latencia variable, o múltiples clientes).

- Formato válido: `[A-Za-z0-9_-]{1,36}`. Si se envía con otro formato → `HTTP 400 {"error":"invalid request_id"}` (en POST, el comando **no** se encola).
- Si no se envía, la respuesta no incluye `request_id` (backwards-compatible).
- `POST /hard-stop` **no** acepta `request_id`.

---

### GET /status

Retorna el estado completo del sistema. Acepta `request_id` como query param opcional.

**Query params:**

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `request_id` | string (opcional) | Identificador `[A-Za-z0-9_-]{1,36}`. Si se envía, se echo-devuelve en la respuesta. |

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
  },
  "request_id": "abc123"
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
| `request_id` | string | Solo presente si fue enviado en el request, idéntico al valor recibido. |

**Errores:**

| HTTP | Condición |
|------|-----------|
| 400 | `request_id` con formato inválido (no matchea `[A-Za-z0-9_-]{1,36}`) |
| 503 | Módulos GPS/Compass no inicializados |

**Ejemplos:**

```bash
# Sin tracking
curl http://<ip>/status

# Con request_id para correlacionar (UUID hex de 32 chars)
curl "http://<ip>/status?request_id=$(uuidgen | tr -d '-' | tr 'A-Z' 'a-z')"
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
  "band_6": true,
  "request_id": "abc123"
}
```

| Campo | Tipo | Rango | Descripción |
|-------|------|-------|-------------|
| `azimuth` | float | 0.0 – 360.0 | Ángulo de destino en azimuth (opcional) |
| `elevation` | float | 0.0 – 90.0 | Ángulo de destino en elevación (opcional) |
| `band_0` … `band_6` | boolean | — | `true` activa la banda, `false` la desactiva (opcionales) |
| `request_id` | string | `[A-Za-z0-9_-]{1,36}` | Identificador opcional. Si se envía con formato inválido, el comando **no** se encola y se retorna 400. |

**Response:**

Devuelve el mismo payload que `GET /status`, prefijado con `"status":"queued"` para indicar que el comando fue aceptado. Los campos `navigation.azimuth` / `navigation.elevation` reflejan la posición **actual** del rotor (no el target recién encolado, que tarda en alcanzarse). Si el request incluyó `request_id`, se echo-devuelve como último campo.

```json
{
  "status": "queued",
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
  },
  "request_id": "abc123"
}
```

**Errores:**

| HTTP | Condición |
|------|-----------|
| 400 | `azimuth` fuera de rango `[0, 360]` |
| 400 | `elevation` fuera de rango `[0, 90]` |
| 400 | `request_id` con formato inválido (no matchea `[A-Za-z0-9_-]{1,36}`) |
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

# Con request_id para correlacionar
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":180.0,"request_id":"job_42"}'

# Error esperado — azimuth fuera de rango
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":400.0}'
# → HTTP 400  {"error":"azimuth out of range [0,360]"}

# Error esperado — request_id inválido (espacios)
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":90.0,"request_id":"has spaces"}'
# → HTTP 400  {"error":"invalid request_id"}  (el comando NO se encola)
```

---

### POST /hard-stop

Parada de emergencia. **Apaga las 7 bandas de RF** y **envía comando de stop al rotor G5500** (azimuth y elevación). No recibe body.

Este mismo flujo lo dispara el `ActivityWatchdog` automáticamente si pierde actividad de control o HTTP — el endpoint es la versión manual del mismo procedimiento.

**Request:**

Sin body. Cualquier contenido enviado se ignora. **No** acepta `request_id` (a diferencia de los otros endpoints).

**Response:**

```json
{ "status": "hard_stop_executed" }
```

**Errores:**

| HTTP | Condición |
|------|-----------|
| 503 | Use case no disponible (no inicializado) |

**Ejemplo:**

```bash
curl -X POST http://<ip>/hard-stop
```

---

## Configuración de red por Serial (JSON)

La IP, máscara y gateway del dispositivo se configuran enviando comandos JSON por el **puerto Serial USB** (`Serial`, 115200 bps, 8-N-1). La configuración se persiste en la EEPROM interna del ATmega2560 y sobrevive a power-cycles.

**¿Por qué Serial y no HTTP?** Es la única vía de recuperación garantizada cuando una IP estática mal configurada deja al equipo aislado de la red. No hay watchdogs ni rollbacks automáticos: el boot es determinista — el equipo levanta exactamente con lo que dice la EEPROM.

### Conexión

```bash
pio device monitor -b 115200
# o cualquier terminal serial: screen, minicom, picocom, putty, etc.
```

Cada comando es **una línea JSON terminada en `\n`**. El servicio convive con el `Logger` de debug en el mismo Serial: las líneas que no empiezan con `{` se ignoran silenciosamente, así que los logs no interfieren con el parser.

> ⚠️ **Importante**: el firmware solo procesa el comando cuando recibe el terminador de línea (`\n` o `\r`). Si estás escribiendo desde un monitor serial interactivo (PlatformIO, Arduino IDE, screen, minicom), asegurate de **enviar Enter / salto de línea** después del JSON. Sin newline, el comando se queda buffered indefinidamente y no recibirás respuesta.
>
> - **PlatformIO Monitor**: Enter manda `\r` por default → funciona.
> - **Arduino IDE Serial Monitor**: en el dropdown inferior derecho elegí `Newline`, `Carriage return` o `Both NL & CR`. **No** uses `No line ending`.
> - **screen / minicom**: Enter manda `\r` por default → funciona.
> - **Desde código (Python `pyserial`, etc.)**: agregá `\n` explícitamente al final del payload.

Al boot el sistema emite un banner:

```json
{"info":"config-channel","commands":["get-config","set-config","reset-config"]}
```

### Comandos

#### `get-config` — consultar estado de red

**Request:**

```json
{"cmd":"get-config"}
```

**Response:**

```json
{"mode":"static","ip":"192.168.5.50","subnet":"255.255.255.0","gateway":"192.168.5.1","currentIp":"192.168.5.50","macAddress":"DE:AD:BE:EF:FE:ED"}
```

| Campo | Descripción |
|-------|-------------|
| `mode` | `"static"` si la EEPROM tiene config válida y el flag activo; `"dhcp"` si no |
| `ip`, `subnet`, `gateway` | Config almacenada en EEPROM (en modo `dhcp` aparecen como `0.0.0.0`) |
| `currentIp` | IP **realmente asignada** al stack ethernet en este momento (la del DHCP cuando aplica) |
| `macAddress` | MAC actual del dispositivo |

#### `set-config` — escribir nueva configuración

Escribe la config a EEPROM y **reinicia el dispositivo** automáticamente para que la nueva IP tome efecto.

**Modo estático:**

```json
{"cmd":"set-config","mode":"static","ip":"192.168.5.50","subnet":"255.255.255.0","gateway":"192.168.5.1"}
```

**Modo DHCP** (apaga el flag de EEPROM, vuelve a obtención automática):

```json
{"cmd":"set-config","mode":"dhcp"}
```

**Response (éxito):**

```json
{"status":"saved","reboot":true}
```

**Errores** (no se escribe la EEPROM, no hay reboot):

| Mensaje | Causa |
|---------|-------|
| `{"error":"missing cmd"}` | El JSON no contiene la clave `cmd` |
| `{"error":"missing mode"}` | `set-config` sin clave `mode` |
| `{"error":"missing ip/subnet/gateway"}` | Modo `static` sin alguno de los 3 campos requeridos |
| `{"error":"invalid ip"}` | IP con formato inválido o `0.0.0.0` / `255.255.255.255` |
| `{"error":"invalid subnet"}` / `{"error":"non-contiguous subnet"}` | Máscara inválida o no contigua |
| `{"error":"invalid gateway"}` / `{"error":"gateway not in subnet"}` | Gateway inválido o fuera de la subred indicada |
| `{"error":"invalid mode"}` | `mode` no es `"static"` ni `"dhcp"` |

#### `reset-config` — borrar configuración

Restaura el factory default (modo DHCP) y reinicia.

```json
{"cmd":"reset-config"}
```

**Response:**

```json
{"status":"saved","reboot":true}
```

### Recuperación de una IP que dejó al equipo aislado

Si configuraste una IP estática y el equipo no responde por la red (subred equivocada, IP duplicada, gateway inalcanzable):

1. Conectá el cable USB al CONTROLLINO MAXI.
2. Abrí el monitor serial (`pio device monitor -b 115200`).
3. Enviá `{"cmd":"set-config","mode":"dhcp"}` (o reescribí la IP correcta).
4. El equipo reinicia y vuelve a estar accesible por red.

Si la EEPROM se corrompe (magic byte o CRC inválidos), el firmware trata el contenido como ausente y bootea en DHCP automáticamente — no hay forma de "brickear" el equipo por una EEPROM en mal estado.

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
- **Constructor de comandos de red** — en el sidebar, el panel "Network Config (Command Builder)" arma los JSON (`get-config`, `set-config`, `reset-config`) listos para copiar y pegar en `pio device monitor`. La UI no abre el puerto serial: solo te entrega el comando.

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
