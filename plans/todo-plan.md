# TODO — Escenarios donde el sistema puede quedar atrapado / bloqueado

Análisis sobre [main.cpp](../src/main.cpp) y los componentes que invoca. La arquitectura es cooperativa, no bloqueante (todo gira sobre `loop()` con máquinas de estado y timeouts), así que no hay ningún `while(true)` real. Pero sí hay varios escenarios donde el sistema puede quedar **efectivamente atrapado** — bloqueado, livelock o disparando callbacks en bucle.

---

## 1. Slow-loris en el WebServer (alta probabilidad)

Ref: [WebServer.cpp:174-241](../lib/WebServer/WebServer.cpp#L174-L241), [WebServer.cpp:113-119](../lib/WebServer/WebServer.cpp#L113-L119)

- El servidor es **single-client** (`_client = _server.available()` solo en `PS_IDLE`).
- En [WebServer.cpp:177](../lib/WebServer/WebServer.cpp#L177), `_lastActivityMs = millis()` se actualiza **con cada byte** recibido.
- Un cliente que envíe 1 byte cada <3 s nunca dispara el `WS_CLIENT_TIMEOUT_MS` (3000 ms) y bloquea para siempre la atención de cualquier otro request. `/hard-stop` quedaría inalcanzable hasta reset.

- [ ] Mitigación: usar tiempo desde *inicio del request* (no última actividad), o limitar bytes/segundo.

## 2. Switches azimuth/elevation con `INPUT` flotante (alta probabilidad)

Ref: [main.cpp:96-99](../src/main.cpp#L96-L99)

- Los 4 switches AZ/EL se inicializan como `INPUT` sin pull-up. Si un cable se afloja o queda desconectado, la entrada **flota** y el debouncer (50 ms por defecto) detecta transiciones espurias.
- Cada transición dispara `sendG5500Command` → se inunda Serial2 con `forward/backward/stop` aleatorios. No es un loop de software, pero es un *livelock* funcional: el rotor recibe órdenes contradictorias en bucle.
- Solo `rfPowerSwitch` usa `INPUT_PULLUP` ([main.cpp:100](../src/main.cpp#L100)).

- [ ] Mitigación: cambiar a `INPUT_PULLUP`/`INPUT_PULLDOWN` o agregar resistencia externa fija.

## 3. Bus I²C colgado en CompassModule (media probabilidad)

Ref: [CompassModule.cpp:48-66](../src/modules/CompassModule.cpp#L48-L66)

- `Wire.requestFrom()` y `Wire.endTransmission(false)` **bloquean** internamente si el QMC5883L mantiene SDA/SCL bajo (típico tras un reset parcial o pico EMI). En AVR, sin reinicio del bus, cada `compassModule.update()` agrega ~100 ms de cuelgue → el `loop()` deja de servir HTTP y de poll-ear el rotor con la cadencia esperada.
- No hay watchdog ni recuperación de bus.

- [ ] Mitigación: implementar timeout de `Wire`, secuencia de recovery (toggle SCL 9 veces) o watchdog.

## 4. Escrituras a Serial2 durante POLL_SENT

Ref: [RotorService.cpp:17-37](../src/services/RotorService.cpp#L17-L37) vs [callback.h:26-32](../src/callback.h#L26-L32)

- `sendG5500Command` sí valida `isSerialFree()`, pero `emergencyKill()`, `stopAzimuth()`, `stopElevation()` **escriben sin chequear** el estado de polling.
- Si `/hard-stop` o `enqueuePosition` con `pending=true` ejecutan mientras el rotor está esperando respuesta, se intercala el frame TX con el RX entrante → checksum falla → `LOG("RX FAILED")`, recupera por `POLL_TIMEOUT_MS` (500 ms). Repetido bajo carga, mantiene el rotor sin status válido durante mucho tiempo (`_hasStatus` queda obsoleto; el endpoint `/status` reporta posición vieja).

- [ ] Mitigación: aplicar el mismo guard `isSerialFree()` o encolarlos en `RotorService`.

## 5. Frame LLP vacío al arranque

Ref: [main.cpp:92](../src/main.cpp#L92) → [SetNavigationAndPowerUseCase.cpp:31](../src/use_cases/SetNavigationAndPowerUseCase.cpp#L31) → [RotorService.cpp:42-56](../src/services/RotorService.cpp#L42-L56)

- `deactivateRFPower(nullptr)` llama `execute(false, 0, false, 0, …)` → `enqueuePosition(false, 0, false, 0)` con `pending=true`.
- En el primer `update()`, ambos `if (hasAz)`/`if (hasEl)` son falsos pero se ejecuta `_txPack.write(*_serial)` con el pack vacío (solo `clear()`). Se manda un frame inválido al G5500 al arranque. No es loop, pero contamina la primera trama.

- [ ] Mitigación: en `enqueuePosition`, no marcar `pending=true` si `!hasAz && !hasEl`.

## 6. DHCP bloqueante al arranque

Ref: [main.cpp:63](../src/main.cpp#L63)

- `Ethernet.begin(mac)` es **síncrono**: hasta ~60 s si no hay servidor DHCP. Durante ese tiempo, todo está congelado (sin watchdog). Visualmente parece un cuelgue.

- [ ] Mitigación: reducir timeout DHCP o ir directo a IP estática y reintentar DHCP en background.

## 7. POST con `Content-Length` grande y bytes en ráfaga

Ref: [WebServer.cpp:175](../lib/WebServer/WebServer.cpp#L175), [WebServer.cpp:180-194](../lib/WebServer/WebServer.cpp#L180-L194)

- El `while (_client.available())` no acota su trabajo por iteración. Un POST con `Content-Length: 65535` y todos los bytes ya en el buffer del W5100 se drenarían **completos en una sola llamada a `update()`**: incrementa `_bodyReceived` 65k veces, `digitalRead` no corre, GPS pierde NMEA, rotor se desfasa.

- [ ] Mitigación: romper el `while` cada N bytes y devolver control al `loop()`. Validar `_contentLength <= WS_PARAMS_LEN` y rechazar con 413.

---

## Resumen de prioridades

| # | Riesgo | Probabilidad | Impacto |
|---|---|---|---|
| 2 | Switches flotantes | Alta | Comandos al rotor en bucle |
| 1 | Slow-loris HTTP | Alta | `/hard-stop` inalcanzable |
| 3 | I²C colgado | Media | `loop()` degradado ~100 ms |
| 4 | Stop-cmds sin guard | Media | Status obsoleto |
| 7 | Burst HTTP | Baja | `loop()` bloqueado transitorio |
| 5 | Frame vacío inicial | Baja | Trama inválida al G5500 |
| 6 | DHCP bloqueante | Baja | Cuelgue ~60 s al arranque |

## Acción transversal

- [ ] Activar watchdog: `wdt_enable(WDTO_2S)` y `wdt_reset()` al final de cada `loop()`.
