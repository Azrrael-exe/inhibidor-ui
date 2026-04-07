# Diseño Lógico de la API de Integración: Inhibidor-UI

## Introducción
El sistema `inhibidor-ui` es un sistema de control embebido diseñado para la plataforma CONTROLLINO MAXI (ATmega2560). Expone una interfaz de integración basada en una API HTTP a través del puerto 80, permitiendo el monitoreo y control remoto en tiempo real de los componentes hardware, incluyendo GPS, brújula magnética, rotor de antenas y bandas de radiofrecuencia (RF).

## Arquitectura de la Interfaz
La API sigue un enfoque arquitectónico orientado a recursos simples (REST-like), utilizando el protocolo HTTP estándar. Se comunica exclusivamente mediante el formato de intercambio de datos **JSON** tanto para las solicitudes como para las respuestas.

### Características Principales:
- **Puerto de Comunicación:** 80 (HTTP estándar).
- **Formato de Datos:** JSON (`Content-Type: application/json` para peticiones POST).
- **Gestión de Errores:** Las respuestas en caso de falla incluyen una estructura JSON con el campo `error` detallando el motivo, y se entregan junto a un código de estado HTTP semántico (e.g., 400 Bad Request, 503 Service Unavailable).

## Especificación Lógica de Endpoints

La API cuenta con dos operaciones principales diseñadas para dividir el ciclo de interacciones en dos flujos: adquisición (solo lectura) y actuación (escritura).

### 1. Monitoreo del Estado Global (Adquisición)
**Endpoint:** `GET /status`

Este endpoint permite consultar de manera idempotente una foto del estado integral del sistema en tiempo real. 

**Estructura Lógica de los Datos Retornados:**
- **`gps`**: Proporciona información de geolocalización extraída del receptor NMEA (latitud, longitud, altitud) y una marca de tiempo en formato ISO 8601. Los valores se presentan normalizados como cadenas de texto (`string`).
- **`heading`**: Rumbo actual provisto por el compás magnético, reportado en grados continuos (0 a 360).
- **`navigation`**: Estado actual del posicionamiento del rotor, reflejando los ángulos físicos actuales del arreglo en azimuth y elevación. Los valores se proveen en grados, devolviendo un `"0.0"` por defecto si el hardware subyacente deja de responder.
- **`power`**: Estado de operación discreto de las 7 bandas de RF de inhibición (`band_0` a `band_6`), representadas como valores booleanos (`true` indicando emitiendo, `false` en reposo).

### 2. Control Combinado de Navegación y Energía (Actuación)
**Endpoint:** `POST /set-navigation-and-power`

Este endpoint consolida la capacidad directiva sobre los actuadores (motores) y módulos de potencia transmisora (bandas) del sistema. Su diseño lógico está orientado en la flexibilidad ("Patch"): acepta actualizaciones *parciales*. Todos los atributos en el cuerpo de la solicitud son opcionales. Si un parámetro es omitido en el payload JSON, el sistema asume implícitamente que el estado de ese componente particular no debe sufrir alteraciones durante la transacción.

**Parámetros Lógicos Aceptados:**
- **Eje de Navegación:**  `azimuth` (float, entre 0.0 y 360.0) y `elevation` (float, entre 0.0 y 90.0). Definen los objetivos posicionales hacia los que el rotor debe moverse. 
- **Arreglo de Potencia RF:** `band_0` a `band_6` (boolean). Envían comandos directos de encendido (`true`) o apagado (`false`) sobre la etapa de amplificación correspondiente.

**Gestión de Excepciones Lógicas:**
- **Protección de Frontera (Boundary Checking):** El procesador valida dinámicamente los límites rotacionales. Si los ángulos están fuera de límite (e.g. elevación > 90.0), se interrumpe y se envía HTTP `400`.
- **Fallas de Acoplamiento de Hardware:** Si se recibe un comando de desplazamiento de ángulos per se, y la API reconoce que el bus que controla el motor se encuentra no disponible o no inicializado, se rechaza y previene cualquier daño con un HTTP `503`.

## Mapeo Físico vs. Abstracción Lógica
La API es una abstracción agnóstica para clientes de alto nivel. A nivel interno, el microcontrolador abstrae y orquesta las directivas HTTP hacia los periféricos correspondientes sin exponer esa complejidad al consumidor de red:
- **Red:** El HTTP sobre puerto 80 es operado en capa física por el bus de comunicaciones **SPI** y el stack del chip Wiznet W5100.
- **Telemetría Transparente:** La obtención de datos para armar el `status` incluye demultiplexar telemetría serial a 38400 baudios (receptor BE-880Q) e implementar pooling por un bus digital de dos hilos (I2C) para la magnetometría.
- **Multiplexado de Comandos:** Dentro del POST al endpoint mixto de escritura, interviene de manera segregada en hardware operando interrupciones Serial a 115200 baudios para el controlador G5500, mientras simultáneamente realiza conmutaciones GPIO de nivel lógico (on/off) para las líneas de inhibición.

## Conclusión
El diseño estructural aborda la necesidad de mantener un servidor ligero priorizando el envío y recolección eficiente. Al concentrar todo cambio de estado en un único endpoint multifunción `/set-navigation-and-power`, se eliminan round-trips indeseables en la red y se agiliza drásticamente el cambio combinado de dirección y disparo (operaciones tácticas coordinadas en la vida real), apoyándose siempre en el rigor en validaciones de valores límites (Sanity Checks).
