#pragma once
#include <Arduino.h>

struct GpsData {
    bool     valid;       // fix con posición válida (status=A)
    bool     nmea_ok;     // true si se están recibiendo tramas NMEA (aunque sin fix)
    float    latitude;    // grados decimales (+N, -S)
    float    longitude;   // grados decimales (+E, -W)
    float    altitude;    // metros sobre nivel del mar
    float    speed_kmh;   // velocidad en km/h
    float    course;      // rumbo GPS en grados 0-360
    uint8_t  satellites;  // satélites visibles (disponible sin fix)
    float    hdop;        // dilución horizontal de precisión
    uint8_t  hour;        // hora UTC (disponible sin fix si hay señal)
    uint8_t  minute;
    uint8_t  second;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
};

class GpsModule {
public:
    void             begin(HardwareSerial& serial, long baud = 38400);
    void             update();
    const GpsData&   getData() const { return _data; }

private:
    bool  verifyChecksum(const char* sentence);
    void  parseSentence();
    void  parseRMC(const char* s);   // $GPRMC / $GNRMC
    void  parseGGA(const char* s);   // $GPGGA / $GNGGA
    float nmeaToDecimal(const char* nmea, char dir);
    const char* getField(const char* s, uint8_t fieldNum, char* out, uint8_t outLen);

    HardwareSerial* _serial;
    char    _buf[96];
    uint8_t _idx;
    GpsData _data;
};
