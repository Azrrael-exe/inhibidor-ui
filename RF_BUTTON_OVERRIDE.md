# RF Button Override - Comportamiento Implementado

## Resumen
El botón físico de RF Power (pin A1) ahora actúa como **override de prioridad absoluta**. Mientras está presionado, controla completamente las 7 bandas de RF y bloquea todos los comandos de la API.

---

## Comportamiento de los Botones

### Estado: Botón RF PRESIONADO

| Aspecto | Comportamiento |
|---------|---|
| **Pines RF** | Todos en HIGH (2, 3, 4, 5, 6, 7, 8) |
| **Flag interno** | `rfButtonActive = true` |
| **Log** | `[RFButton] RF button pressed: all bands HIGH, API locked` |
| **API calls** | **BLOQUEADAS** con HTTP 423 |

#### Ejemplo: Usuario presiona botón, API intenta enviar comando
```
Usuario presiona botón RF (pin A1 = HIGH)
↓
Callback activateRFPower() ejecutado
├─ rfButtonActive = true
├─ digitalWrite(pin 2-8, HIGH)  ← Todas las bandas ACTIVAS
└─ LOG: "RF button pressed: all bands HIGH, API locked"

Mientras tanto, cliente API:
POST /set-navigation-and-power
Body: { "band_0": 0, "band_1": 1, "band_2": 0 }
↓
NavigationHandler::handleSetNavigationAndPower()
├─ if (rfButtonActive) → TRUE
├─ LOG: "RF button active, rejecting API command (HTTP 423)"
└─ Response: HTTP 423 Locked
   {
     "error": "RF button override active"
   }
```

---

### Estado: Botón RF SOLTADO

| Aspecto | Comportamiento |
|---------|---|
| **Pines RF** | Todos en LOW (2, 3, 4, 5, 6, 7, 8) |
| **Flag interno** | `rfButtonActive = false` |
| **Log** | `[RFButton] RF button released: all bands LOW, API unlocked` |
| **API calls** | **PERMITIDAS** nuevamente |

#### Ejemplo: Usuario suelta botón, API vuelve a funcionar
```
Usuario suelta botón RF (pin A1 = LOW)
↓
Callback deactivateRFPower() ejecutado
├─ rfButtonActive = false
├─ digitalWrite(pin 2-8, LOW)  ← Todas las bandas INACTIVAS
└─ LOG: "RF button released: all bands LOW, API unlocked"

Ahora cliente API:
POST /set-navigation-and-power
Body: { "band_0": 0, "band_1": 1, "band_2": 0 }
↓
NavigationHandler::handleSetNavigationAndPower()
├─ if (rfButtonActive) → FALSE
├─ Procesa normalmente
├─ Ejecuta SetNavigationAndPowerUseCase
└─ Response: HTTP 200 OK
   {
     "status": "queued"
   }
```

---

## Escenarios Especiales

### Escenario 1: Hard Stop durante override de RF

```
Estado inicial: Botón RF presionado (todas las bandas HIGH)

Cliente API:
POST /hard-stop
↓
HardStopHandler::handleHardStop()
├─ NO verifica rfButtonActive (sin bloqueo)
├─ Ejecuta hardStopUseCase.execute()
│  ├─ Apaga todas las bandas: digitalWrite(2-8, LOW)
│  └─ Envía emergencyKill() al rotor G5500
└─ Response: HTTP 200 OK

Resultado: Hard stop ejecutado INCLUSO con botón activo
(Emergency stop tiene prioridad sobre override de RF)
```

---

### Escenario 2: Pulso rápido del botón RF

```
T=0ms: Usuario presiona botón
       rfButtonActive = true
       Pines 2-8 → HIGH

T=100ms: Usuario suelta botón
         rfButtonActive = false
         Pines 2-8 → LOW

Resultado: Todas las bandas recibieron un pulso de 100ms
           (comportamiento de botón físico momentáneo)
```

---

### Escenario 3: API intenta cambiar banda específica

```
Estado inicial: rfButtonActive = false, todas las bandas LOW

API envía:
POST /set-navigation-and-power
Body: { "band_3": 1 }  ← Solo activar banda 3

Resultado esperado: Pin 5 (RF_BAND_3) → HIGH
Resultado real: Pin 5 → HIGH ✓

Luego, usuario presiona botón RF:
rfButtonActive = true
Pines 2-8 → HIGH (todas)

Resultado: Pin 5 sigue HIGH, pero ahora bajo control del botón

API intenta revertir:
POST /set-navigation-and-power
Body: { "band_3": 0 }
↓
RECHAZADO: HTTP 423 Locked
Resultado: Pin 5 sigue HIGH (botón tiene prioridad)
```

---

## Log Messages

### Log al presionar botón
```
[RFButton] RF button pressed: all bands HIGH, API locked
```

### Log al soltar botón
```
[RFButton] RF button released: all bands LOW, API unlocked
```

### Log cuando API intenta actuar durante override
```
[NavigationHandler] RF button active, rejecting API command (HTTP 423)
```

---

## Código Relevante

### Variable de Estado (src/main.cpp)
```cpp
bool rfButtonActive = false;

static const uint8_t BAND_PINS[7] = {
    RF_BAND_0, RF_BAND_1, RF_BAND_2, RF_BAND_3,
    RF_BAND_4, RF_BAND_5, RF_BAND_6
};
```

### Callbacks (src/main.cpp)
```cpp
void activateRFPower(void* context) {
    rfButtonActive = true;
    for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(BAND_PINS[i], HIGH);
    }
    LOG("RFButton", "RF button pressed: all bands HIGH, API locked");
}

void deactivateRFPower(void* context) {
    rfButtonActive = false;
    for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(BAND_PINS[i], LOW);
    }
    LOG("RFButton", "RF button released: all bands LOW, API unlocked");
}
```

### Verificación en Handler (src/handlers/NavigationHandler.cpp)
```cpp
extern bool rfButtonActive;

void handleSetNavigationAndPower(const HttpRequest& req, HttpResponse& res) {
    if (rfButtonActive) {
        LOG("NavigationHandler", "RF button active, rejecting API command (HTTP 423)");
        res.json(423, "{\"error\":\"RF button override active\"}");
        return;
    }
    // ... resto del procesamiento
}
```

---

## Respuestas HTTP

### Éxito: Botón soltado, API funciona
```
HTTP/1.1 200 OK
Content-Type: application/json

{"status":"queued"}
```

### Error: Botón presionado, API bloqueada
```
HTTP/1.1 423 Locked
Content-Type: application/json

{"error":"RF button override active"}
```

---

## Flujo de Ejecución

```
Loop Principal
├─ webServer.update()
│  └─ Si POST /set-navigation-and-power:
│     └─ handleSetNavigationAndPower()
│        ├─ ¿rfButtonActive = true? → HTTP 423 + LOG
│        └─ ¿rfButtonActive = false? → Procesa normalmente
│
├─ rfPowerSwitch.update()
│  ├─ Si cambio LOW→HIGH: activateRFPower()
│  │  └─ rfButtonActive = true, pines HIGH, LOG
│  └─ Si cambio HIGH→LOW: deactivateRFPower()
│     └─ rfButtonActive = false, pines LOW, LOG
│
└─ ... otros updates
```

---

## Testing Checklist

- [ ] Compilar sin errores: `pio run -e controllino_maxi`
- [ ] Botón presionado → Pines 2-8 en HIGH
- [ ] Botón presionado → Log "RF button pressed: all bands HIGH, API locked"
- [ ] API request durante botón presionado → HTTP 423
- [ ] API request durante botón presionado → Log "RF button active, rejecting..."
- [ ] Botón soltado → Pines 2-8 en LOW
- [ ] Botón soltado → Log "RF button released: all bands LOW, API unlocked"
- [ ] API request con botón soltado → HTTP 200 (funciona)
- [ ] Hard stop con botón presionado → Ejecuta (no bloqueado)
