# inhibidor-ui

Sistema de control embebido para plataforma CONTROLLINO MAXI (ATmega2560).
Expone una API HTTP en el puerto 80 para monitoreo y control del sistema.

---

## API Reference

Base URL: `http://<ip-del-dispositivo>`

Todas las respuestas — éxito y error — son JSON e incluyen `timestamp` y `time_valid` en la raíz (ver sección siguiente).

### Timestamp en todas las respuestas

Toda respuesta HTTP (200 OK y errores 4xx/5xx) incluye en la raíz del JSON dos campos derivados de la consulta al receptor GPS al momento de armar la respuesta:

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `timestamp` | string | Fecha y hora UTC en formato ISO 8601 (`YYYY-MM-DDTHH:MM:SSZ`). |
| `time_valid` | boolean | `true` si el GPS está entregando tramas NMEA con hora parseada; `false` si aún no se recibió un RMC válido (arranque temprano, sin antena, etc.). Cuando es `false`, `timestamp` muestra `"0000-00-00T00:00:00Z"`. |

> **Nota:** si el GPS pierde señal después de haber tenido fix, `timestamp` refleja la última hora capturada (no avanza). Usar `time_valid` para discriminar la fiabilidad.

---

### Correlación request ↔ response (`request_id`)

Los endpoints `GET /status`, `POST /set-navigation-and-power`, `POST /config/network` y `POST /config/watchdog` aceptan un campo opcional **`request_id`** que el servidor echo-devuelve en el cuerpo de la respuesta para que el cliente pueda correlacionar pares request/response (útil con reintentos, latencia variable, o múltiples clientes).

- Formato válido: `[A-Za-z0-9_-]{1,36}`. Si se envía con otro formato → `HTTP 400 {"error":"invalid request_id"}` (en POST, el comando **no** se ejecuta).
- Si no se envía, la respuesta no incluye `request_id` (backwards-compatible).
- `POST /hard-stop` y `POST /homming` **no** aceptan `request_id`.

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
  "request_id": "abc123",
  "timestamp": "2026-02-24T15:30:00Z",
  "time_valid": true
}
```

> `gps.datetime` y `timestamp` raíz provienen de la misma fuente GPS y son iguales; `gps.datetime` se mantiene por compatibilidad.

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
| `azimuth` | float | 0.0 – 450.0 | Ángulo de destino en azimuth (opcional). El rango excede 360° porque el G5500 admite overlap mecánico. |
| `elevation` | float | 0.0 – 180.0 | Ángulo de destino en elevación (opcional). |
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
  "request_id": "abc123",
  "timestamp": "2026-02-24T15:30:00Z",
  "time_valid": true
}
```

**Errores:**

| HTTP | Condición |
|------|-----------|
| 400 | `azimuth` fuera de rango `[0, 450]` |
| 400 | `elevation` fuera de rango `[0, 180]` |
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
  -d '{"azimuth":500.0}'
# → HTTP 400  {"error":"azimuth out of range [0,450]"}

# Error esperado — request_id inválido (espacios)
curl -X POST http://<ip>/set-navigation-and-power \
  -H "Content-Type: application/json" \
  -d '{"azimuth":90.0,"request_id":"has spaces"}'
# → HTTP 400  {"error":"invalid request_id"}  (el comando NO se encola)
```

---

### POST /hard-stop

Parada de emergencia explícita solicitada por el operador. **Apaga las 7 bandas de RF** y **envía al G5500 el comando `SYSTEM_KILL`** (key `0xFF`, value `0x01`) — prioridad máxima en el firmware del rotor: cancela cualquier movimiento activo y detiene los motores. No recibe body.

Este endpoint es la **única** vía que ejecuta `SYSTEM_KILL`. Los watchdogs y el switch físico de homming **no** disparan este flujo — disparan `POST /homming` en su lugar.

**Request:**

Sin body. Cualquier contenido enviado se ignora. **No** acepta `request_id` (a diferencia de los otros endpoints).

**Response:**

```json
{ "status": "hard_stop_executed", "timestamp": "2026-02-24T15:30:00Z", "time_valid": true }
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

### POST /homming

Homming. **Apaga las 7 bandas de RF** y **envía al G5500 el comando `SYSTEM_HOME`** (key `0xFF`, value `0x02`) — prioridad máxima en el firmware del rotor: lleva el rotor a la posición home definida internamente por el G5500. No recibe body.

Este mismo flujo se dispara por cuatro vías:

| Fuente | Descripción |
|--------|-------------|
| `POST /homming` | Versión manual vía HTTP (este endpoint). |
| Botón físico (`HOMMING_SWITCH_PIN` / A0) | Al presionar el botón conectado al pin A0, el firmware detecta el flanco ascendente (activo-HIGH, debounce 50 ms) y ejecuta el Homming de forma inmediata e independiente del estado de la red. |
| `ActivityWatchdog` | Disparo automático si no llega actividad HTTP en 10 s o actividad de control en 60 s. |
| `RFOnTimeWatchdog` | Disparo automático si una banda RF excede el tiempo máximo encendida (default 60 s, configurable). |

**Request:**

Sin body. Cualquier contenido enviado se ignora. **No** acepta `request_id`.

**Response:**

```json
{ "status": "homming_executed", "timestamp": "2026-02-24T15:30:00Z", "time_valid": true }
```

**Errores:**

| HTTP | Condición |
|------|-----------|
| 503 | Use case no disponible (no inicializado) |

**Ejemplo:**

```bash
curl -X POST http://<ip>/homming
```

---

### GET /config/network

Retorna la configuración de red persistida en EEPROM más la IP y MAC activas del stack ethernet.

**Response:**

```json
{
  "mode": "dhcp",
  "ip": "0.0.0.0",
  "subnet": "0.0.0.0",
  "gateway": "0.0.0.0",
  "currentIp": "192.168.1.100",
  "macAddress": "DE:AD:BE:EF:FE:ED",
  "timestamp": "2026-02-24T15:30:00Z",
  "time_valid": true
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `mode` | string | `"static"` si la EEPROM tiene config válida con el flag activo; `"dhcp"` si no |
| `ip` / `subnet` / `gateway` | string | Config almacenada en EEPROM (en modo `dhcp` aparecen como `0.0.0.0`) |
| `currentIp` | string | IP **realmente asignada** al stack ethernet ahora mismo |
| `macAddress` | string | MAC del dispositivo |

**Errores:**

| HTTP | Condición |
|------|-----------|
| 500 | Buffer interno desbordado |

**Ejemplo:**

```bash
curl http://<ip>/config/network
```

---

### POST /config/network

Modifica la configuración de red persistente y **reinicia el dispositivo** automáticamente para que la nueva IP tome efecto. Equivalente HTTP del comando Serial `{"cmd":"set-config",...}`.

> ⚠️ **Operación de alto riesgo.** Una IP estática mal configurada (subred equivocada, gateway inalcanzable, IP duplicada) deja al equipo aislado por red — la única vía de recuperación es el canal Serial USB. Validá la config en una red de prueba antes de aplicarla a equipos en producción.

El reboot es diferido: el handler responde 200 primero, marca un flag, y el reset se dispara desde `loop()` una vez que `WebServer` cierra la conexión TCP. Así el cliente HTTP recibe un FIN limpio antes del reinicio.

**Request body (modo DHCP):**

```json
{ "mode": "dhcp", "request_id": "abc123" }
```

**Request body (modo estático):**

```json
{
  "mode": "static",
  "ip": "192.168.5.50",
  "subnet": "255.255.255.0",
  "gateway": "192.168.5.1",
  "request_id": "abc123"
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `mode` | string | `"static"` o `"dhcp"`. Requerido. |
| `ip`, `subnet`, `gateway` | string | Solo en modo `static`. IPs en formato `a.b.c.d`. |
| `request_id` | string (opcional) | `[A-Za-z0-9_-]{1,36}`. Se echo-devuelve en la respuesta. |

**Response (éxito):**

```json
{ "status": "saved", "reboot": true, "request_id": "abc123", "timestamp": "2026-02-24T15:30:00Z", "time_valid": true }
```

**Errores** (no se escribe la EEPROM, no hay reboot):

| HTTP | Mensaje | Causa |
|------|---------|-------|
| 400 | `missing mode` | Falta el campo `mode` |
| 400 | `invalid mode` | `mode` no es `"static"` ni `"dhcp"` |
| 400 | `missing ip/subnet/gateway` | Modo `static` sin alguno de los 3 campos |
| 400 | `invalid ip` / `invalid subnet` / `invalid gateway` | Formato inválido o `0.0.0.0` / `255.255.255.255` |
| 400 | `non-contiguous subnet` | Máscara no contigua |
| 400 | `gateway not in subnet` | Gateway fuera de la subred |
| 400 | `invalid request_id` | `request_id` no matchea `[A-Za-z0-9_-]{1,36}` |
| 500 | `eeprom write failed` | Falla al escribir EEPROM |

**Ejemplos:**

```bash
# Cambiar a IP estática
curl -X POST http://<ip>/config/network \
  -H "Content-Type: application/json" \
  -d '{"mode":"static","ip":"192.168.5.50","subnet":"255.255.255.0","gateway":"192.168.5.1"}'

# Volver a DHCP
curl -X POST http://<ip>/config/network \
  -H "Content-Type: application/json" \
  -d '{"mode":"dhcp"}'
```

---

### GET /config/watchdog

Retorna el timeout y estado del `RFOnTimeWatchdog` y los timeouts de todos los canales del `ActivityWatchdog` en una sola respuesta.

**Response:**

```json
{
  "rf_watchdog": {
    "timeout_seconds": 300,
    "active": false
  },
  "activity_watchdog": {
    "channels": [
      { "name": "http",    "timeout_ms": 60000 },
      { "name": "control", "timeout_ms": 60000 }
    ]
  },
  "timestamp": "2026-02-24T15:30:00Z",
  "time_valid": true
}
```

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `rf_watchdog.timeout_seconds` | int | Tiempo máximo que puede estar encendida alguna banda RF antes de disparar Homming (volátil) |
| `rf_watchdog.active` | boolean | `true` si al menos una banda RF está encendida ahora |
| `activity_watchdog.channels[].name` | string | Nombre del canal (`http`, `control`, etc.) |
| `activity_watchdog.channels[].timeout_ms` | int | Timeout del canal en milisegundos (volátil) |

**Errores:**

| HTTP | Condición |
|------|-----------|
| 500 | Buffer interno desbordado |

**Ejemplo:**

```bash
curl http://<ip>/config/watchdog
```

---

### POST /config/watchdog

Modifica en caliente los timeouts de los watchdogs. Todos los campos son opcionales — omitir un campo deja ese watchdog sin cambios.

Los cambios son **volátiles**: no se persisten en EEPROM y vuelven al valor por defecto tras cada reboot.

**Comportamiento por campo:**
- `rf_timeout_seconds`: actualiza `RFOnTimeWatchdog`. **No reinicia el cronómetro** — un valor más bajo puede disparar `Homming` inmediatamente si hay bandas encendidas.
- `http_timeout_seconds` / `control_timeout_seconds`: actualizan el canal correspondiente del `ActivityWatchdog` y lo alimentan automáticamente (`feed`) para evitar un trip inmediato.

**Request body** (todos los campos opcionales, rango `1..3600`):

```json
{
  "rf_timeout_seconds": 300,
  "http_timeout_seconds": 60,
  "control_timeout_seconds": 60,
  "request_id": "abc123"
}
```

**Response (éxito):**

```json
{ "status": "updated", "request_id": "abc123", "timestamp": "2026-02-24T15:30:00Z", "time_valid": true }
```

**Errores:**

| HTTP | Mensaje | Causa |
|------|---------|-------|
| 400 | `no watchdog field provided` | Body sin ningún campo de watchdog |
| 400 | `rf_timeout_seconds out of range (1..3600)` | Valor fuera de rango |
| 400 | `http_timeout_seconds out of range (1..3600)` | Valor fuera de rango |
| 400 | `control_timeout_seconds out of range (1..3600)` | Valor fuera de rango |
| 400 | `invalid request_id` | `request_id` no matchea `[A-Za-z0-9_-]{1,36}` |
| 503 | `rf watchdog not available` | Servicio no inicializado |
| 503 | `http watchdog not available` | Canal no inicializado |
| 503 | `control watchdog not available` | Canal no inicializado |

**Ejemplos:**

```bash
# Solo RF watchdog
curl -X POST http://<ip>/config/watchdog \
  -H "Content-Type: application/json" \
  -d '{"rf_timeout_seconds":120}'

# Solo activity watchdog
curl -X POST http://<ip>/config/watchdog \
  -H "Content-Type: application/json" \
  -d '{"http_timeout_seconds":30,"control_timeout_seconds":30}'

# Los tres juntos
curl -X POST http://<ip>/config/watchdog \
  -H "Content-Type: application/json" \
  -d '{"rf_timeout_seconds":300,"http_timeout_seconds":60,"control_timeout_seconds":60}'
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

Si la EEPROM se corrompe (magic byte o CRC inválidos), el firmware trata el contenido como ausente y bootea con IP estática `192.168.1.100` — no hay forma de "brickear" el equipo por una EEPROM en mal estado.

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

## Configuraciones por defecto del dispositivo

Resumen de los valores con los que el firmware levanta al boot. Se distinguen tres tipos:

- **Persistente (EEPROM)** — sobrevive a power-cycles; se modifica desde Serial o HTTP.
- **Volátil** — vuelve al default después de cada reinicio; modificable en caliente.
- **Hardcoded** — fijo en el firmware; cambia solo recompilando.

### Red

| Parámetro | Default | Tipo | Notas |
|-----------|---------|------|-------|
| Modo cold boot | `static 192.168.1.100` | Hardcoded | Si la EEPROM está virgen o con CRC inválido, el firmware **no** intenta DHCP: levanta directo en `192.168.1.100` con subnet `255.255.255.0` y gateway `192.168.1.1` (defaults de la librería Ethernet). Garantiza una IP conocida en arranques de fábrica. |
| Modo runtime | `static` o `dhcp` (según EEPROM) | EEPROM | Tras la primera escritura via `set-config` (Serial) o `POST /set-network-config`, el boot sigue lo guardado en EEPROM. Si el usuario eligió `dhcp` explícitamente, el firmware sí intenta DHCP y cae a `192.168.1.100` como fallback. |
| IP fallback DHCP | `192.168.1.100` | Hardcoded | Cuando la EEPROM dice `dhcp` y `Ethernet.begin(mac)` falla (sin servidor DHCP en la red). |
| MAC | `DE:AD:BE:EF:FE:ED` | Hardcoded | Definida en `src/main.cpp:31`. Ver [Cambiar la MAC del dispositivo](#cambiar-la-mac-del-dispositivo). |
| Puerto HTTP | `80` | Hardcoded | `WebServer webServer(80)`. |

#### Cambiar la MAC del dispositivo

La MAC es un valor **hardcoded**: a diferencia de la IP, no se configura por Serial ni HTTP. Es una decisión deliberada — el ATmega2560 está al ~88.6% de su SRAM (8 KB) y hacerla configurable en runtime consumiría memoria estática (literales de string + buffers de parseo) que el equipo no tiene de sobra. Para cambiarla se edita el firmware y se reprograma el microcontrolador.

1. Editá el arreglo `mac[]` en `src/main.cpp:31`, reemplazando los 6 bytes hex:

```cpp
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
```

2. Recompilá y subí el firmware al CONTROLLINO:

```bash
pio run -t upload
```

3. La nueva MAC toma efecto al boot. Verificala con `GET /config/network` o el comando Serial `get-config` (campo `macAddress`).

> ⚠️ **Unicidad**: si hay más de un equipo en la misma LAN, cada uno necesita una MAC **única**, o habrá conflictos ARP (los dispositivos se roban el tráfico entre sí).
>
> 💡 **Rango locally-administered**: para redes privadas conviene una MAC del rango administrado localmente — el segundo bit menos significativo del primer byte en `1` (es decir, primer byte par: `0x02`, `0x06`, `0xDE`, etc.). Así no colisiona con MACs asignadas por fabricantes. El default `0xDE` ya cumple.

### Serial

| Interfaz | Bitrate | Uso |
|----------|---------|-----|
| `Serial` (USB) | 115200 8-N-1 | `Logger` de debug + canal de configuración (`get-config` / `set-config` / `reset-config`). |
| `Serial1` | 38400 | Receptor GPS BE-880Q (NMEA). |
| `Serial2` | 115200 | Controlador de rotor G5500 (protocolo LLP). |

### Watchdogs

| Watchdog | Default | Tipo | Cómo modificarlo |
|----------|---------|------|------------------|
| `RFOnTimeWatchdog` | `300 s` | Volátil | `POST /config/watchdog` con `rf_timeout_seconds` (rango `1..3600`). Vuelve al default tras reboot. |
| `ActivityWatchdog` canal `http` | `60 000 ms` | Volátil | `POST /config/watchdog` con `http_timeout_seconds` (rango `1..3600` s). Vuelve al default tras reboot. |
| `ActivityWatchdog` canal `control` | `60 000 ms` | Volátil | `POST /config/watchdog` con `control_timeout_seconds` (rango `1..3600` s). Vuelve al default tras reboot. |

Ambos canales del `ActivityWatchdog` disparan `HommingUseCase` en timeout. El `RFOnTimeWatchdog` también dispara `HommingUseCase` cuando alguna banda RF supera el tiempo máximo encendida.

### Rotor (G5500)

| Parámetro | Default | Tipo | Notas |
|-----------|---------|------|-------|
| Poll interval | `2 000 ms` | Hardcoded | Frecuencia con la que `RotorService::update()` consulta status al G5500. |
| Poll timeout | `500 ms` | Hardcoded | Espera máxima antes de descartar la respuesta del G5500 y volver a `POLL_IDLE`. |
| Rango `azimuth` | `0.0 – 450.0` | Hardcoded | Validado por `SetNavigationAndPowerUseCase`. |
| Rango `elevation` | `0.0 – 180.0` | Hardcoded | Validado por `SetNavigationAndPowerUseCase`. |

### RF / GPIO

| Parámetro | Default | Notas |
|-----------|---------|-------|
| Bandas RF al boot | Todas OFF | Railguard explícito en `setup()` antes de habilitar interrupciones / red. |
| LED rojo + buzzer | Espejo de "alguna banda RF ON" | Edge-detected: se actualiza solo cuando cambia `rfOnTimeWatchdog.isAnyOn()`. |
| Switch Homming (A0) | Activo-HIGH, edge ascendente | Dispara `HommingUseCase` inmediato. |
| Switch RF (A1) | Activo-LOW (`INPUT_PULLUP`) | Pulsado → enciende las 7 bandas; soltado → las apaga. |
| Switches Az/El (A2-A5) | Activo-HIGH | Movimiento manual del rotor mientras se mantiene presionado. |

### Validaciones de API comunes

| Validación | Default | Aplica a |
|------------|---------|----------|
| `request_id` formato | `[A-Za-z0-9_-]{1,36}` | `GET /status`, `POST /set-navigation-and-power`, `POST /config/network`, `POST /config/watchdog`. **No** lo aceptan `/hard-stop` ni `/homming`. |
| Rango de timeouts | `1..3600` s | `POST /config/watchdog` — aplica a `rf_timeout_seconds`, `http_timeout_seconds` y `control_timeout_seconds`. |

---

## Hardware

| Componente | Interfaz | Descripción |
|-----------|----------|-------------|
| CONTROLLINO MAXI | — | MCU principal (ATmega2560) |
| BE-880Q | Serial1 @ 38400 | Receptor GPS, protocolo NMEA |
| QMC5883L | I2C (0x0D) | Compás magnético |
| G5500 | Serial @ 115200 | Controlador de rotor (azimuth/elevación) |
| W5100 | SPI | Módulo Ethernet |
| Botón Homming | A0 (`HOMMING_SWITCH_PIN`) | Entrada digital activo-HIGH. Al presionarlo dispara Homming inmediato (apaga RF + envía `SYSTEM_HOME` al rotor). |

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
