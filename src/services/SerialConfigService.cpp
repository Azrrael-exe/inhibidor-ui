#include "SerialConfigService.h"
#include "NetworkConfig.h"
#include "../handlers/json_helpers.h"
#include <Ethernet.h>
#include <string.h>

SerialConfigService::SerialConfigService()
    : _serial(nullptr), _lineLen(0), _lineOverflow(false) {
    memset(_mac, 0, sizeof(_mac));
}

void SerialConfigService::begin(Stream* serial, const uint8_t mac[6]) {
    _serial = serial;
    _lineLen = 0;
    _lineOverflow = false;
    memcpy(_mac, mac, 6);

    _emit(F("{\"info\":\"config-channel\",\"commands\":[\"get-config\",\"set-config\",\"reset-config\"]}"));
}

void SerialConfigService::update() {
    if (!_serial) return;

    while (_serial->available() > 0) {
        int c = _serial->read();
        if (c < 0) break;

        if (c == '\n' || c == '\r') {
            if (_lineLen > 0 && !_lineOverflow) {
                _lineBuf[_lineLen] = '\0';
                _processLine(_lineBuf);
            } else if (_lineOverflow) {
                _emitError(F("line too long"));
            }
            _lineLen = 0;
            _lineOverflow = false;
            continue;
        }

        if (_lineOverflow) continue;
        if (_lineLen >= LINE_BUF_LEN - 1) {
            _lineOverflow = true;
            continue;
        }
        _lineBuf[_lineLen++] = (char)c;
    }
}

void SerialConfigService::_processLine(const char* line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '{') return;  // not a JSON command — ignore (logger output)

    char cmd[24];
    if (!jsonGetString(line, F("cmd"), cmd, sizeof(cmd))) {
        _emitError(F("missing cmd"));
        return;
    }

    if (strcmp_P(cmd, PSTR("get-config")) == 0) {
        _handleGetConfig();
    } else if (strcmp_P(cmd, PSTR("set-config")) == 0) {
        _handleSetConfig(line);
    } else if (strcmp_P(cmd, PSTR("reset-config")) == 0) {
        _handleResetConfig();
    } else {
        _emitError(F("unknown cmd"));
    }
}

void SerialConfigService::_handleGetConfig() {
    NetConfig cfg;
    bool valid = NetworkConfig::load(cfg);
    if (!valid) NetworkConfig::factoryDefaults(cfg);

    char ipStr[16], subnetStr[16], gatewayStr[16], currentStr[16];
    formatIPv4(cfg.ip,      ipStr,      sizeof(ipStr));
    formatIPv4(cfg.subnet,  subnetStr,  sizeof(subnetStr));
    formatIPv4(cfg.gateway, gatewayStr, sizeof(gatewayStr));

    IPAddress current = Ethernet.localIP();
    uint32_t curBE = ((uint32_t)current[0] << 24) | ((uint32_t)current[1] << 16)
                   | ((uint32_t)current[2] << 8)  |  (uint32_t)current[3];
    formatIPv4(curBE, currentStr, sizeof(currentStr));

    char macStr[18];
    snprintf_P(macStr, sizeof(macStr), PSTR("%02X:%02X:%02X:%02X:%02X:%02X"),
               _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);

    char body[200];
    snprintf_P(body, sizeof(body),
               PSTR("{\"mode\":\"%S\",\"ip\":\"%s\",\"subnet\":\"%s\",\"gateway\":\"%s\","
               "\"currentIp\":\"%s\",\"macAddress\":\"%s\"}"),
               cfg.useEepromConfig ? PSTR("static") : PSTR("dhcp"),
               ipStr, subnetStr, gatewayStr, currentStr, macStr);
    _emit(body);
}

void SerialConfigService::_handleSetConfig(const char* json) {
    char mode[12];
    if (!jsonGetString(json, F("mode"), mode, sizeof(mode))) {
        _emitError(F("missing mode"));
        return;
    }

    NetConfig cfg;
    NetworkConfig::factoryDefaults(cfg);

    if (strcmp_P(mode, PSTR("dhcp")) == 0) {
        cfg.useEepromConfig = false;
    } else if (strcmp_P(mode, PSTR("static")) == 0) {
        char ipStr[20], subnetStr[20], gatewayStr[20];
        if (!jsonGetString(json, F("ip"),      ipStr,      sizeof(ipStr))      ||
            !jsonGetString(json, F("subnet"),  subnetStr,  sizeof(subnetStr))  ||
            !jsonGetString(json, F("gateway"), gatewayStr, sizeof(gatewayStr))) {
            _emitError(F("missing ip/subnet/gateway"));
            return;
        }
        if (!parseIPv4(ipStr,      cfg.ip))      { _emitError(F("invalid ip"));      return; }
        if (!parseIPv4(subnetStr,  cfg.subnet))  { _emitError(F("invalid subnet"));  return; }
        if (!parseIPv4(gatewayStr, cfg.gateway)) { _emitError(F("invalid gateway")); return; }
        cfg.useEepromConfig = true;
    } else {
        _emitError(F("invalid mode"));
        return;
    }

    char err[40];
    if (!NetworkConfig::validate(cfg, err, sizeof(err))) {
        _emitError(err);
        return;
    }

    if (!NetworkConfig::save(cfg)) {
        _emitError(F("eeprom write failed"));
        return;
    }

    _emit(F("{\"status\":\"saved\",\"reboot\":true}"));
    _serial->flush();
    NetworkConfig::reboot();
}

void SerialConfigService::_handleResetConfig() {
    NetConfig cfg;
    NetworkConfig::factoryDefaults(cfg);
    NetworkConfig::save(cfg);
    _emit(F("{\"status\":\"saved\",\"reboot\":true}"));
    _serial->flush();
    NetworkConfig::reboot();
}

void SerialConfigService::_emit(const char* json) {
    if (!_serial) return;
    _serial->println(json);
}

void SerialConfigService::_emit(const __FlashStringHelper* json) {
    if (!_serial) return;
    _serial->println(json);
}

void SerialConfigService::_emitError(const char* msg) {
    char body[80];
    snprintf_P(body, sizeof(body), PSTR("{\"error\":\"%s\"}"), msg ? msg : "");
    _emit(body);
}

void SerialConfigService::_emitError(const __FlashStringHelper* msg) {
    char body[80];
    snprintf_P(body, sizeof(body), PSTR("{\"error\":\"%S\"}"), msg);
    _emit(body);
}
