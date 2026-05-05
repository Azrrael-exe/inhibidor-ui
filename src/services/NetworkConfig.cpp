#include "NetworkConfig.h"
#include <EEPROM.h>
#include <avr/wdt.h>
#include <string.h>

namespace {
    constexpr uint16_t EEPROM_BASE = 0x0000;
    constexpr uint8_t  MAGIC       = 0xA5;
    constexpr uint8_t  VERSION     = 0x01;

    constexpr uint16_t OFF_MAGIC   = EEPROM_BASE + 0x00;
    constexpr uint16_t OFF_VERSION = EEPROM_BASE + 0x01;
    constexpr uint16_t OFF_FLAG    = EEPROM_BASE + 0x02;
    constexpr uint16_t OFF_RSVD    = EEPROM_BASE + 0x03;
    constexpr uint16_t OFF_IP      = EEPROM_BASE + 0x04;
    constexpr uint16_t OFF_SUBNET  = EEPROM_BASE + 0x08;
    constexpr uint16_t OFF_GATEWAY = EEPROM_BASE + 0x0C;
    constexpr uint16_t OFF_CRC     = EEPROM_BASE + 0x10;
    constexpr uint16_t BLOCK_LEN   = 0x10; // bytes covered by CRC (0x00–0x0F)

    // CRC-8 (poly 0x07, init 0x00) — small footprint, sufficient for 16 B integrity.
    uint8_t crc8(const uint8_t* data, size_t len) {
        uint8_t crc = 0;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t b = 0; b < 8; b++) {
                crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
            }
        }
        return crc;
    }

    void writeBE32(uint16_t addr, uint32_t v) {
        EEPROM.update(addr,     (uint8_t)((v >> 24) & 0xFF));
        EEPROM.update(addr + 1, (uint8_t)((v >> 16) & 0xFF));
        EEPROM.update(addr + 2, (uint8_t)((v >>  8) & 0xFF));
        EEPROM.update(addr + 3, (uint8_t)(v & 0xFF));
    }

    uint32_t readBE32(uint16_t addr) {
        return ((uint32_t)EEPROM.read(addr)     << 24)
             | ((uint32_t)EEPROM.read(addr + 1) << 16)
             | ((uint32_t)EEPROM.read(addr + 2) <<  8)
             | ((uint32_t)EEPROM.read(addr + 3));
    }

    bool isContiguousMask(uint32_t mask) {
        // A valid IPv4 netmask is N ones followed by (32-N) zeros.
        // Equivalent to: (~mask + 1) & ~mask == 0, with edge cases for 0 and 0xFFFFFFFF.
        if (mask == 0xFFFFFFFFUL) return true;
        uint32_t inv = ~mask;
        return (inv & (inv + 1)) == 0;
    }
}

bool NetworkConfig::load(NetConfig& out) {
    uint8_t buf[BLOCK_LEN];
    for (uint16_t i = 0; i < BLOCK_LEN; i++) {
        buf[i] = EEPROM.read(EEPROM_BASE + i);
    }
    uint8_t storedCrc = EEPROM.read(OFF_CRC);

    if (buf[0] != MAGIC)        return false;
    if (buf[1] != VERSION)      return false;
    if (crc8(buf, BLOCK_LEN) != storedCrc) return false;

    out.useEepromConfig = (buf[2] != 0);
    out.ip      = readBE32(OFF_IP);
    out.subnet  = readBE32(OFF_SUBNET);
    out.gateway = readBE32(OFF_GATEWAY);
    return true;
}

bool NetworkConfig::save(const NetConfig& cfg) {
    EEPROM.update(OFF_MAGIC,   MAGIC);
    EEPROM.update(OFF_VERSION, VERSION);
    EEPROM.update(OFF_FLAG,    cfg.useEepromConfig ? 1 : 0);
    EEPROM.update(OFF_RSVD,    0);
    writeBE32(OFF_IP,      cfg.ip);
    writeBE32(OFF_SUBNET,  cfg.subnet);
    writeBE32(OFF_GATEWAY, cfg.gateway);

    uint8_t buf[BLOCK_LEN];
    for (uint16_t i = 0; i < BLOCK_LEN; i++) {
        buf[i] = EEPROM.read(EEPROM_BASE + i);
    }
    EEPROM.update(OFF_CRC, crc8(buf, BLOCK_LEN));
    return true;
}

void NetworkConfig::factoryDefaults(NetConfig& out) {
    out.useEepromConfig = false;
    out.ip = 0;
    out.subnet = 0;
    out.gateway = 0;
}

bool NetworkConfig::validate(const NetConfig& cfg, char* errOut, size_t errLen) {
    auto fail = [&](const char* msg) {
        if (errOut && errLen) {
            strncpy(errOut, msg, errLen - 1);
            errOut[errLen - 1] = '\0';
        }
        return false;
    };

    if (!cfg.useEepromConfig) return true; // DHCP mode → nothing to validate

    if (cfg.ip == 0 || cfg.ip == 0xFFFFFFFFUL)         return fail("invalid ip");
    if (cfg.gateway == 0 || cfg.gateway == 0xFFFFFFFFUL) return fail("invalid gateway");
    if (cfg.subnet == 0 || cfg.subnet == 0xFFFFFFFFUL) return fail("invalid subnet");
    if (!isContiguousMask(cfg.subnet))                 return fail("non-contiguous subnet");
    if ((cfg.ip & cfg.subnet) != (cfg.gateway & cfg.subnet)) return fail("gateway not in subnet");

    return true;
}

void NetworkConfig::reboot() {
    wdt_enable(WDTO_15MS);
    while (true) { /* spin until watchdog fires */ }
}
