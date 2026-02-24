#include "GpsModule.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

void GpsModule::begin(HardwareSerial& serial, long baud) {
    _serial = &serial;
    _serial->begin(baud);
    _idx = 0;
    memset(&_data, 0, sizeof(_data));
}

void GpsModule::update() {
    while (_serial->available()) {
        char c = (char)_serial->read();
        if (c == '$') {
            _idx = 0;
            _buf[_idx++] = c;
        } else if (c == '\n') {
            if (_idx > 0 && _idx < sizeof(_buf)) {
                _buf[_idx] = '\0';
                parseSentence();
            }
            _idx = 0;
        } else if (_idx > 0 && _idx < (sizeof(_buf) - 1)) {
            _buf[_idx++] = c;
        }
    }
}

// Extrae el campo N (base 0) de una cadena CSV separada por comas.
// Retorna puntero a out o NULL si no existe.
const char* GpsModule::getField(const char* s, uint8_t fieldNum, char* out, uint8_t outLen) {
    uint8_t field = 0;
    uint8_t i = 0;
    while (*s && field < fieldNum) {
        if (*s == ',') field++;
        s++;
    }
    if (field != fieldNum) { out[0] = '\0'; return NULL; }
    while (*s && *s != ',' && *s != '*' && i < (outLen - 1)) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return out;
}

bool GpsModule::verifyChecksum(const char* sentence) {
    // Formato: $...*XX\r\n
    const char* p = sentence;
    if (*p != '$') return false;
    p++;
    uint8_t calc = 0;
    while (*p && *p != '*') {
        calc ^= (uint8_t)*p++;
    }
    if (*p != '*') return false;
    p++;
    uint8_t expected = (uint8_t)strtol(p, NULL, 16);
    return calc == expected;
}

// Convierte formato NMEA DDDMM.MMMM con indicador de dirección a grados decimales
float GpsModule::nmeaToDecimal(const char* nmea, char dir) {
    if (!nmea || nmea[0] == '\0') return 0.0f;
    float raw = atof(nmea);
    int   deg = (int)(raw / 100);
    float min = raw - (deg * 100.0f);
    float decimal = deg + min / 60.0f;
    if (dir == 'S' || dir == 'W') decimal = -decimal;
    return decimal;
}

void GpsModule::parseSentence() {
    if (!verifyChecksum(_buf)) return;

    // Marcar que llegó al menos una trama válida
    _data.nmea_ok = true;

    // Comparar los primeros 5 chars del tipo de sentencia (sin el $)
    if (strncmp(_buf + 1, "GPRMC", 5) == 0 || strncmp(_buf + 1, "GNRMC", 5) == 0) {
        parseRMC(_buf);
    } else if (strncmp(_buf + 1, "GPGGA", 5) == 0 || strncmp(_buf + 1, "GNGGA", 5) == 0) {
        parseGGA(_buf);
    }
}

// $GPRMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
// Campo: 0=tipo,1=time,2=status,3=lat,4=latDir,5=lon,6=lonDir,7=speed(knots),8=course,9=date
void GpsModule::parseRMC(const char* s) {
    char f[16];

    // Time: hhmmss.ss — disponible incluso sin fix
    getField(s, 1, f, sizeof(f));
    if (f[0] >= '0' && f[0] <= '9') {
        _data.hour   = (f[0] - '0') * 10 + (f[1] - '0');
        _data.minute = (f[2] - '0') * 10 + (f[3] - '0');
        _data.second = (f[4] - '0') * 10 + (f[5] - '0');
    }

    // Fecha: ddmmyy — disponible incluso sin fix
    getField(s, 9, f, sizeof(f));
    if (f[0] >= '0' && f[0] <= '9') {
        _data.day   = (f[0] - '0') * 10 + (f[1] - '0');
        _data.month = (f[2] - '0') * 10 + (f[3] - '0');
        _data.year  = 2000 + (f[4] - '0') * 10 + (f[5] - '0');
    }

    // Status (campo 2): A=válido, V=sin fix
    getField(s, 2, f, sizeof(f));
    if (f[0] != 'A') {
        _data.valid = false;
        return;
    }

    // Latitud
    char latDir[4];
    getField(s, 3, f, sizeof(f));
    getField(s, 4, latDir, sizeof(latDir));
    _data.latitude = nmeaToDecimal(f, latDir[0]);

    // Longitud
    char lonDir[4];
    getField(s, 5, f, sizeof(f));
    getField(s, 6, lonDir, sizeof(lonDir));
    _data.longitude = nmeaToDecimal(f, lonDir[0]);

    // Velocidad: nudos → km/h
    getField(s, 7, f, sizeof(f));
    _data.speed_kmh = atof(f) * 1.852f;

    // Rumbo
    getField(s, 8, f, sizeof(f));
    _data.course = atof(f);

    _data.valid = true;
}

// $GPGGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
// Campo: 0=tipo,1=time,2=lat,3=latDir,4=lon,5=lonDir,6=quality,7=sats,8=hdop,9=alt
void GpsModule::parseGGA(const char* s) {
    char f[16];

    // Satélites visibles — siempre disponible, incluso sin fix
    getField(s, 7, f, sizeof(f));
    _data.satellites = (uint8_t)atoi(f);

    // Fix quality (0 = sin fix); posición/altitud solo con fix
    getField(s, 6, f, sizeof(f));
    if (atoi(f) == 0) return;

    // Latitud
    char latDir[4], lonDir[4];
    getField(s, 2, f, sizeof(f));
    getField(s, 3, latDir, sizeof(latDir));
    _data.latitude = nmeaToDecimal(f, latDir[0]);

    // Longitud
    getField(s, 4, f, sizeof(f));
    getField(s, 5, lonDir, sizeof(lonDir));
    _data.longitude = nmeaToDecimal(f, lonDir[0]);

    // HDOP
    getField(s, 8, f, sizeof(f));
    _data.hdop = atof(f);

    // Altitud
    getField(s, 9, f, sizeof(f));
    _data.altitude = atof(f);
}
