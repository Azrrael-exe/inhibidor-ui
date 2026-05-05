#pragma once
#include <Arduino.h>

// ─── Persistent network configuration (EEPROM) ────────────────────────────────
//
// Layout (16 B at EEPROM address 0x00):
//   0x00  magic (0xA5)
//   0x01  version (0x01)
//   0x02  use_eeprom_config (0=DHCP, 1=Static)
//   0x03  reserved
//   0x04  ip       (4 B, network byte order)
//   0x08  subnet   (4 B, network byte order)
//   0x0C  gateway  (4 B, network byte order)
//   0x10  crc8     (over bytes 0x00–0x0F)
//
// IPs are stored in network byte order (big-endian) so a (uint8_t*) cast
// matches IPAddress's internal representation directly.
// ──────────────────────────────────────────────────────────────────────────

struct NetConfig {
    bool     useEepromConfig;
    uint32_t ip;       // network byte order
    uint32_t subnet;   // network byte order
    uint32_t gateway;  // network byte order
};

namespace NetworkConfig {
    /** Read EEPROM, validate magic + crc. Returns false on virgin / corrupt EEPROM. */
    bool load(NetConfig& out);

    /** Write config to EEPROM (uses EEPROM.update to minimize wear). */
    bool save(const NetConfig& cfg);

    /** Fill out with factory defaults: useEepromConfig=false, IPs zeroed. */
    void factoryDefaults(NetConfig& out);

    /**
     * Validate a static-mode config. Returns false and writes a short
     * human-readable reason into errOut on failure. Only called when
     * cfg.useEepromConfig == true.
     */
    bool validate(const NetConfig& cfg, char* errOut, size_t errLen);

    /** Trigger a hardware reset via the watchdog. Does not return. */
    void reboot();
}
