# Ejemplos de Callbacks con Múltiples Parámetros

## ¿Es posible recibir más de un parámetro por context?

**¡SÍ! Puedes recibir ILIMITADOS parámetros usando structs.**

## Cómo Funciona

El callback recibe un solo puntero `void*`, pero ese puntero puede apuntar a un **struct con cualquier cantidad de campos**.

## Ejemplos Implementados

### 1. Single Parameter (1 parámetro)

```cpp
// Pasar solo un puerto serial
void onAzimuthForwardOn(void* context) {
  HardwareSerial* serial = (HardwareSerial*)context;
  serial->println("Switch ON");
}

// Registrar:
azimuthForwardSwitch.setOnTurnOn(onAzimuthForwardOn, &Serial1);
```

### 2. Multiple Parameters (3 parámetros)

```cpp
// Struct con 3 campos
struct SwitchContext {
  HardwareSerial* serial;
  const char* name;
  int switchId;
};

// Crear instancia
SwitchContext ctx = {&Serial1, "Elevation", 1};

// Usar en callback
void onElevationForwardOn(void* context) {
  SwitchContext* ctx = (SwitchContext*)context;
  ctx->serial->print(ctx->name);
  ctx->serial->print(" ID:");
  ctx->serial->println(ctx->switchId);
}

// Registrar:
elevationSwitch.setOnTurnOn(onElevationForwardOn, &ctx);
```

### 3. MANY Parameters (8+ parámetros) ⭐

```cpp
// Struct con 8 campos (¡puedes agregar más!)
struct AdvancedContext {
  HardwareSerial* serial;     // Puerto serial
  DataPack* datapack;         // Protocolo DataPack
  const char* name;           // Nombre del switch
  int switchId;               // ID del switch
  uint8_t outputPin;          // Pin de salida a controlar
  bool* systemEnabled;        // Puntero a flag global
  unsigned long* lastAction;  // Puntero a timestamp
  int speed;                  // Velocidad del motor
};

// Crear instancia con todos los parámetros
DataPack dp;
bool enabled = true;
unsigned long timestamp = 0;

AdvancedContext advCtx = {
  &Serial1,        // serial
  &dp,             // datapack
  "Azimuth",       // name
  99,              // switchId
  13,              // outputPin
  &enabled,        // systemEnabled
  &timestamp,      // lastAction
  100              // speed
};

// Usar TODOS los parámetros en el callback
void onAzimuthBackwardOn(void* context) {
  AdvancedContext* ctx = (AdvancedContext*)context;

  // Acceder a todos los parámetros
  if (!(*ctx->systemEnabled)) {
    ctx->serial->println("Sistema deshabilitado");
    return;
  }

  *ctx->lastAction = millis();  // Modificar variable externa

  ctx->serial->print(ctx->name);
  ctx->serial->print(" ID:");
  ctx->serial->println(ctx->switchId);
  ctx->serial->print("Speed: ");
  ctx->serial->println(ctx->speed);

  digitalWrite(ctx->outputPin, HIGH);  // Controlar pin

  // Enviar por DataPack
  ctx->datapack->clear();
  ctx->datapack->addData(0x01, ctx->switchId);
  ctx->datapack->addData(0x02, 1);  // Estado ON
  ctx->datapack->write(*ctx->serial);
}

// Registrar:
azimuthSwitch.setOnTurnOn(onAzimuthBackwardOn, &advCtx);
```

## Tipos de Datos que Puedes Pasar

### Valores Directos
```cpp
struct MyContext {
  int speed;
  float temperature;
  bool enabled;
  char buffer[20];
};
```

### Punteros (para leer/modificar variables externas)
```cpp
struct MyContext {
  int* counter;           // Modificar contador externo
  bool* systemState;      // Leer/modificar estado del sistema
  unsigned long* timer;   // Actualizar timestamp
};

// En el callback:
*ctx->counter += 1;       // Incrementar contador
*ctx->systemState = true; // Cambiar estado
```

### Objetos y Clases
```cpp
struct MyContext {
  HardwareSerial* serial;
  DataPack* protocol;
  MyMotorController* motor;
  MyLCD* display;
};

// En el callback:
ctx->motor->setSpeed(100);
ctx->display->println("Motor ON");
```

### Arrays
```cpp
struct MyContext {
  int* sensorReadings;    // Array de lecturas
  uint8_t* pinList;       // Array de pines
  int arraySize;          // Tamaño del array
};
```

## Ventajas de Este Enfoque

1. **✅ Ilimitados parámetros**: Agrega tantos campos como necesites al struct
2. **✅ Diferentes tipos**: Mezcla int, float, bool, punteros, objetos, etc.
3. **✅ Modificar variables externas**: Usa punteros para leer/escribir variables globales
4. **✅ Compatible con AVR**: No requiere `<functional>` ni lambdas
5. **✅ Memory-efficient**: Solo 4 bytes por callback (el puntero al struct)
6. **✅ Type-safe**: El cast es explícito y verificable

## Comparación de Memoria

| Método | RAM por callback | Notas |
|--------|------------------|-------|
| Function pointer | 2 bytes | Sin parámetros |
| Context pointer | 4 bytes | + tamaño del struct (una vez) |
| std::function | ~24 bytes | No disponible en AVR |

**Ejemplo**: AdvancedContext con 8 campos = ~30 bytes **compartidos** entre callbacks

## Pasos para Agregar Más Parámetros

### Paso 1: Define tu struct
```cpp
struct MyContext {
  HardwareSerial* serial;
  int param1;
  float param2;
  bool param3;
  // ... agrega los que necesites
};
```

### Paso 2: Crea una instancia
```cpp
MyContext myCtx = {&Serial1, 100, 3.14, true};
```

### Paso 3: Registra el callback
```cpp
mySwitch.setOnTurnOn(myCallback, &myCtx);
```

### Paso 4: Usa los parámetros en el callback
```cpp
void myCallback(void* context) {
  MyContext* ctx = (MyContext*)context;

  ctx->serial->println(ctx->param1);
  ctx->serial->println(ctx->param2);
  ctx->serial->println(ctx->param3);
}
```

## ¿Cuántos parámetros puedo pasar?

**¡TANTOS COMO QUIERAS!** No hay límite práctico. Puedes tener structs con 10, 20, 50+ campos.

```cpp
struct MegaContext {
  // Comunicación
  HardwareSerial* serial;
  DataPack* protocol;

  // Identificación
  const char* name;
  int id;

  // Control
  uint8_t pins[10];      // Array de pines
  int speeds[5];         // Array de velocidades

  // Estado
  bool* enabled;
  unsigned long* timer;
  int* counter;

  // Objetos
  MyMotor* motor;
  MySensor* sensor;
  MyDisplay* display;

  // ¡Y más!
  // ... agrega los que necesites
};
```

## Salida Serial Esperada

Al activar el switch con AdvancedContext verás:

```
========================================
[Azimuth Advanced #99] BACKWARD: ON
  -> Output Pin: 13
  -> Speed: 100
  -> Timestamp: 1234
  -> Rotating antenna counterclockwise
[DataPack frame data]
========================================
```

## Estadísticas del Build

- **RAM usage**: 17.7% (1446 bytes) - incluye todos los ejemplos
- **Flash usage**: 2.6% (6510 bytes) - incluye todo el código de ejemplo
- **Compilación**: ✅ Exitosa en ATmega2560

## ¿Qué pasa si NO asigno un callback?

**¡No pasa nada malo!** Es completamente seguro no asignar callbacks. El sistema simplemente ignora los eventos sin callback.

### Opciones de Registro:

```cpp
// Opción 1: Registrar AMBOS callbacks (ON y OFF)
mySwitch.setOnTurnOn(onCallback, &ctx);
mySwitch.setOnTurnOff(offCallback, &ctx);

// Opción 2: SOLO callback de ON (OFF se ignora)
mySwitch.setOnTurnOn(onCallback, &ctx);
// No hay setOnTurnOff - esto es SEGURO!

// Opción 3: SOLO callback de OFF (ON se ignora)
mySwitch.setOnTurnOff(offCallback, &ctx);
// No hay setOnTurnOn - esto es SEGURO!

// Opción 4: NINGÚN callback (solo monitorear estado)
// No registrar nada - el switch funciona pero sin callbacks
// Puedes leer el estado con: mySwitch.getState()
```

### Cómo funciona internamente:

El código verifica si el callback es `nullptr` antes de llamarlo:

```cpp
// En DigitalSwitch.cpp (líneas 43-51)
if (_currentReading == HIGH) {
  if (_onTurnOn) {              // ← Verifica si existe
    _onTurnOn(_onTurnOnContext);  // Solo llama si no es nullptr
  }
} else {
  if (_onTurnOff) {             // ← Verifica si existe
    _onTurnOff(_onTurnOffContext); // Solo llama si no es nullptr
  }
}
```

### Casos de Uso:

**1. Solo necesitas saber cuándo se activa:**
```cpp
// Solo callback para ON
motorSwitch.setOnTurnOn(startMotor, &ctx);
// OFF no importa
```

**2. Solo necesitas saber cuándo se desactiva:**
```cpp
// Solo callback para OFF
alarmSwitch.setOnTurnOff(stopAlarm, &ctx);
// ON no importa
```

**3. Solo quieres leer el estado sin callbacks:**
```cpp
// No registrar callbacks
// En loop():
if (mySwitch.getState()) {
  // El switch está ON
}
```

### Ejemplo Completo en el Código:

Ver `src/main.cpp`:
- **elevationForwardSwitch**: Solo tiene callback ON
- **elevationBackwardSwitch**: Solo tiene callback OFF
- Ambos funcionan perfectamente sin errores

## Conclusión

✨ **Sí, puedes recibir tantos parámetros como quieras usando structs!** ✨

El truco es:
1. Un solo puntero `void*` como context
2. Ese puntero apunta a un struct
3. El struct contiene TODOS los parámetros que necesites
4. En el callback, haces cast y accedes a todos los campos

✨ **Los callbacks son opcionales - no tienes que registrarlos todos!** ✨

Beneficios:
- ✅ Registra solo los eventos que necesites
- ✅ Código más limpio (no callbacks vacíos)
- ✅ Mejor rendimiento (no llamadas innecesarias)
- ✅ Completamente seguro (verificación de nullptr)

¡Es simple, eficiente y muy poderoso! 🚀
