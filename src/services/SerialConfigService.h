#pragma once
#include <Arduino.h>

// ─── Serial JSON configuration channel ────────────────────────────────────────
//
// Reads line-delimited JSON commands from a serial Stream and replies with
// JSON. Commands recognised:
//
//   {"cmd":"get-config"}
//   {"cmd":"set-config","mode":"dhcp"}
//   {"cmd":"set-config","mode":"static","ip":"...","subnet":"...","gateway":"..."}
//   {"cmd":"reset-config"}
//
// The parser ignores any line that does not start with '{' so it coexists
// transparently with the global Logger output on the same Serial.
// ──────────────────────────────────────────────────────────────────────────

class SerialConfigService {
public:
    SerialConfigService();
    void begin(Stream* serial, const uint8_t mac[6]);
    void update();

private:
    static constexpr uint8_t LINE_BUF_LEN = 128;

    Stream*  _serial;
    char     _lineBuf[LINE_BUF_LEN];
    uint8_t  _lineLen;
    bool     _lineOverflow;
    uint8_t  _mac[6];

    void _processLine(const char* line);
    void _handleGetConfig();
    void _handleSetConfig(const char* json);
    void _handleResetConfig();
    void _emit(const char* json);
    void _emitError(const char* msg);
};
