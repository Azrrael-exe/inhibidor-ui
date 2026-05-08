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

/**
 * Extract a value from a flat URL query string ("key=value&key=value").
 * Match is anchored at start of string or right after '&' to avoid substring matches.
 * Value is copied (without percent-decoding) into out, NUL-terminated.
 * Returns true if the key was found (even if its value is empty); false if key absent.
 *
 * Example: queryGetString("foo=1&request_id=abc", "request_id", buf, sizeof(buf)) → buf="abc"
 */
inline bool queryGetString(const char* query, const char* key, char* out, size_t outLen) {
    if (!query || !key || !out || outLen == 0) return false;
    out[0] = '\0';

    size_t keyLen = strlen(key);
    const char* p = query;

    while (*p) {
        bool atBoundary = (p == query) || (*(p - 1) == '&');
        if (atBoundary && strncmp(p, key, keyLen) == 0 && p[keyLen] == '=') {
            p += keyLen + 1;
            size_t i = 0;
            while (*p && *p != '&' && i < outLen - 1) {
                out[i++] = *p++;
            }
            out[i] = '\0';
            return true;
        }
        p++;
    }
    return false;
}

/**
 * Validates that `id` matches [A-Za-z0-9_-]{1,36}.
 * Returns false if id is null, empty, longer than 36, or contains an invalid char.
 */
inline bool isValidRequestId(const char* id) {
    if (!id) return false;
    size_t len = 0;
    for (const char* p = id; *p; p++) {
        char c = *p;
        bool ok = (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  c == '_' || c == '-';
        if (!ok) return false;
        if (++len > 36) return false;
    }
    return len > 0;
}

/**
 * Extracts and validates `request_id` from a request source.
 * `isQueryString` selects the parser: true → queryGetString, false → jsonGetString.
 *
 * Return values:
 *    1 = present and valid (out is filled with the id)
 *    0 = absent (out is "")
 *   -1 = present but invalid format (caller should respond HTTP 400)
 */
inline int extractRequestId(const char* source, bool isQueryString,
                            char* out, size_t outLen) {
    if (!out || outLen == 0) return 0;
    out[0] = '\0';

    bool found = isQueryString
        ? queryGetString(source, "request_id", out, outLen)
        : jsonGetString(source, "request_id", out, outLen);

    if (!found) {
        // jsonGetString returns false even when the key is present but value isn't a string
        // (e.g. "request_id":123). Treat that as a malformed presence → 400.
        if (!isQueryString && jsonHasKey(source, "request_id")) {
            return -1;
        }
        return 0;
    }
    // Both parsers may yield an empty string when the user sent the key with no value.
    // isValidRequestId rejects empty strings, so this collapses to -1 as expected.
    return isValidRequestId(out) ? 1 : -1;
}
