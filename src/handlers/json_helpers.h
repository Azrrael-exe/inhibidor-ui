#pragma once
#include <Arduino.h>
#include <WebServer.h>

/**
 * Extract an integer value from a flat JSON object by key name.
 * Handles optional whitespace around ':'.
 * Returns defaultVal if the key is not found.
 *
 * Example: jsonGetInt("{\"foo\":1,\"bar\":0}", "foo", -1) → 1
 */
inline int jsonGetInt(const char* json, const char* key, int defaultVal) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return defaultVal;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return defaultVal;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    return atoi(p);
}

/**
 * Extract a boolean value from a flat JSON object by key name.
 * Returns 1 (true), 0 (false), or defaultVal if the key is not found.
 *
 * Example: jsonGetBool("{\"active\":true}", "active", -1) → 1
 */
inline int jsonGetBool(const char* json, const char* key, int defaultVal) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return defaultVal;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return defaultVal;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    if (strncmp(p, "true",  4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    return defaultVal;
}

/**
 * Extract a float value from a flat JSON object by key name.
 * Handles optional whitespace around ':'.
 * Returns defaultVal if the key is not found.
 *
 * Example: jsonGetFloat("{\"az\":180.5}", "az", 0.0f) → 180.5
 */
inline float jsonGetFloat(const char* json, const char* key, float defaultVal) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return defaultVal;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return defaultVal;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    return (float)atof(p);
}

/**
 * Returns true if the key exists anywhere in the JSON object.
 *
 * Example: jsonHasKey("{\"az\":180.5}", "az") → true
 */
inline bool jsonHasKey(const char* json, const char* key) {
    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern) != nullptr;
}

/**
 * Extract a string value from a flat JSON object by key name.
 * Copies the raw value (without surrounding quotes) into out, NUL-terminated.
 * Returns true if the key was found and a string value was extracted.
 *
 * Example: jsonGetString("{\"mode\":\"static\"}", "mode", buf, sizeof(buf)) → buf="static"
 */
inline bool jsonGetString(const char* json, const char* key, char* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    out[0] = '\0';

    char pattern[WS_PARAMS_LEN / 2];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* p = strstr(json, pattern);
    if (!p) return false;

    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++; // past opening quote

    size_t i = 0;
    while (*p && *p != '"' && i < outLen - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return *p == '"';
}

/**
 * Parse "a.b.c.d" into a 32-bit value in network byte order
 * (a is the most significant byte, d the least).
 * Returns true on success.
 */
inline bool parseIPv4(const char* s, uint32_t& outBE) {
    if (!s) return false;
    uint32_t result = 0;
    for (uint8_t octet = 0; octet < 4; octet++) {
        if (!*s || *s < '0' || *s > '9') return false;
        uint16_t v = 0;
        uint8_t digits = 0;
        while (*s >= '0' && *s <= '9' && digits < 3) {
            v = v * 10 + (uint16_t)(*s - '0');
            s++;
            digits++;
        }
        if (v > 255) return false;
        result = (result << 8) | (uint32_t)v;
        if (octet < 3) {
            if (*s != '.') return false;
            s++;
        }
    }
    if (*s != '\0') return false;
    outBE = result;
    return true;
}

/** Format a network-byte-order 32-bit IPv4 into "a.b.c.d" (NUL-terminated). */
inline void formatIPv4(uint32_t ipBE, char* out, size_t outLen) {
    snprintf(out, outLen, "%u.%u.%u.%u",
             (unsigned)((ipBE >> 24) & 0xFF),
             (unsigned)((ipBE >> 16) & 0xFF),
             (unsigned)((ipBE >>  8) & 0xFF),
             (unsigned)(ipBE & 0xFF));
}
