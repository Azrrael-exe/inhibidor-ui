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
