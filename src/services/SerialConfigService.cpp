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

    _emit("{\"info\":\"config-channel\",\"commands\":[\"get-config\",\"set-config\",\"reset-config\"]}");
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
                _emitError("line too long");
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
    if (!jsonGetString(line, "cmd", cmd, sizeof(cmd))) {
        _emitError("missing cmd");
        return;
    }

    if (strcmp(cmd, "get-config") == 0) {
        _handleGetConfig();
    } else if (strcmp(cmd, "set-config") == 0) {
        _handleSetConfig(line);
    } else if (strcmp(cmd, "reset-config") == 0) {
        _handleResetConfig();
    } else {
        _emitError("unknown cmd");
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
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             _mac[0], _mac[1], _mac[2], _mac[3], _mac[4], _mac[5]);

    char body[200];
    snprintf(body, sizeof(body),
             "{\"mode\":\"%s\",\"ip\":\"%s\",\"subnet\":\"%s\",\"gateway\":\"%s\","
             "\"currentIp\":\"%s\",\"macAddress\":\"%s\"}",
             cfg.useEepromConfig ? "static" : "dhcp",
             ipStr, subnetStr, gatewayStr, currentStr, macStr);
    _emit(body);
}

void SerialConfigService::_handleSetConfig(const char* json) {
    char mode[12];
    if (!jsonGetString(json, "mode", mode, sizeof(mode))) {
        _emitError("missing mode");
        return;
    }

    NetConfig cfg;
    NetworkConfig::factoryDefaults(cfg);

    if (strcmp(mode, "dhcp") == 0) {
        cfg.useEepromConfig = false;
    } else if (strcmp(mode, "static") == 0) {
        char ipStr[20], subnetStr[20], gatewayStr[20];
        if (!jsonGetString(json, "ip",      ipStr,      sizeof(ipStr))      ||
            !jsonGetString(json, "subnet",  subnetStr,  sizeof(subnetStr))  ||
            !jsonGetString(json, "gateway", gatewayStr, sizeof(gatewayStr))) {
            _emitError("missing ip/subnet/gateway");
            return;
        }
        if (!parseIPv4(ipStr,      cfg.ip))      { _emitError("invalid ip");      return; }
        if (!parseIPv4(subnetStr,  cfg.subnet))  { _emitError("invalid subnet");  return; }
        if (!parseIPv4(gatewayStr, cfg.gateway)) { _emitError("invalid gateway"); return; }
        cfg.useEepromConfig = true;
    } else {
        _emitError("invalid mode");
        return;
    }

    char err[40];
    if (!NetworkConfig::validate(cfg, err, sizeof(err))) {
        _emitError(err);
        return;
    }

    if (!NetworkConfig::save(cfg)) {
        _emitError("eeprom write failed");
        return;
    }

    _emit("{\"status\":\"saved\",\"reboot\":true}");
    _serial->flush();
    NetworkConfig::reboot();
}

void SerialConfigService::_handleResetConfig() {
    NetConfig cfg;
    NetworkConfig::factoryDefaults(cfg);
    NetworkConfig::save(cfg);
    _emit("{\"status\":\"saved\",\"reboot\":true}");
    _serial->flush();
    NetworkConfig::reboot();
}

void SerialConfigService::_emit(const char* json) {
    if (!_serial) return;
    _serial->println(json);
}

void SerialConfigService::_emitError(const char* msg) {
    char body[80];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", msg ? msg : "error");
    _emit(body);
}
