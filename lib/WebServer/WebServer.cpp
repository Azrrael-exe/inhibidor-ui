#include "WebServer.h"
#include <string.h>

// ─── HttpResponse ─────────────────────────────────────────────────────────────

HttpResponse::HttpResponse(EthernetClient* client)
    : _client(client), _sent(false)
{}

bool HttpResponse::sent() const { return _sent; }

void HttpResponse::json(uint16_t statusCode, const char* body) {
    if (_sent) return;
    _send(statusCode, body);
}

void HttpResponse::notFound()         { _sendError(404, "Not Found"); }
void HttpResponse::methodNotAllowed() { _sendError(405, "Method Not Allowed"); }

void HttpResponse::badRequest(const char* msg)  { _sendError(400, msg); }
void HttpResponse::serverError(const char* msg) { _sendError(500, msg); }

void HttpResponse::_send(uint16_t code, const char* body) {
    if (_sent || !_client) return;
    _sent = true;

    uint16_t bodyLen = body ? (uint16_t)strlen(body) : 0;

    _client->print(F("HTTP/1.1 "));
    _client->print(code);
    _client->print(' ');
    _client->print(_phrase(code));
    _client->print(F("\r\nContent-Type: application/json\r\nContent-Length: "));
    _client->print(bodyLen);
    _client->print(F("\r\nConnection: close\r\n\r\n"));

    if (body && bodyLen > 0) {
        _client->print(body);
    }
}

void HttpResponse::_sendError(uint16_t code, const char* msg) {
    if (_sent) return;
    // Build: {"error":"<msg>"}  (max msg 128 chars to fit 160-byte stack buffer)
    char buf[160];
    buf[0] = '\0';
    strncat(buf, "{\"error\":\"", sizeof(buf) - 1);
    uint8_t room = (uint8_t)(sizeof(buf) - strlen(buf) - 3); // reserve `"}\0`
    strncat(buf, msg, room);
    strncat(buf, "\"}", sizeof(buf) - strlen(buf) - 1);
    _send(code, buf);
}

const char* HttpResponse::_phrase(uint16_t code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

// ─── WebServer ────────────────────────────────────────────────────────────────

WebServer::WebServer(uint16_t port)
    : _port(port),
      _routeCount(0),
      _server(port),
      _parseState(PS_IDLE),
      _method(HTTP_UNKNOWN),
      _contentLength(0),
      _bodyReceived(0),
      _lineLen(0),
      _lineOverflow(false)
{
    memset(_routes,  0, sizeof(_routes));
    memset(_path,    0, sizeof(_path));
    memset(_params,  0, sizeof(_params));
    memset(_lineBuf, 0, sizeof(_lineBuf));
}

void WebServer::begin() {
    _server.begin();
    _resetParser();
}

bool WebServer::on(const char* path, HttpMethod method, RouteHandler handler) {
    if (_routeCount >= WS_MAX_ROUTES) return false;
    _routes[_routeCount].path    = path;
    _routes[_routeCount].method  = method;
    _routes[_routeCount].handler = handler;
    _routeCount++;
    return true;
}

// ─── update() ────────────────────────────────────────────────────────────────

void WebServer::update() {
    // Accept a new client if idle
    if (_parseState == PS_IDLE) {
        _client = _server.available();
        if (_client) {
            _resetParser();
            _parseState = PS_REQUEST_LINE;
        }
    }

    // Read bytes while parsing is in progress
    if (_parseState == PS_REQUEST_LINE ||
        _parseState == PS_HEADERS      ||
        _parseState == PS_BODY) {
        _readClient();
    }

    // Full request received: dispatch
    if (_parseState == PS_DONE) {
        HttpRequest req;
        req.method = _method;
        strncpy(req.path,   _path,   WS_PATH_LEN   - 1); req.path[WS_PATH_LEN   - 1] = '\0';
        strncpy(req.params, _params, WS_PARAMS_LEN - 1); req.params[WS_PARAMS_LEN - 1] = '\0';

        _dispatch(req);
        _client.stop();
        _resetParser();
    }

    // Malformed request line
    if (_parseState == PS_ERROR) {
        HttpResponse res(&_client);
        res.badRequest("Malformed request");
        _client.stop();
        _resetParser();
    }
}

// ─── _resetParser() ──────────────────────────────────────────────────────────

void WebServer::_resetParser() {
    _parseState    = PS_IDLE;
    _method        = HTTP_UNKNOWN;
    _path[0]       = '\0';
    _params[0]     = '\0';
    _contentLength = 0;
    _bodyReceived  = 0;
    _lineLen       = 0;
    _lineOverflow  = false;
}

// ─── _readClient() ───────────────────────────────────────────────────────────

/**
 * Character-driven, line-by-line HTTP parser.
 *
 * - '\r' is silently discarded.
 * - '\n' marks end of line; the current _lineBuf is processed, then cleared.
 * - Body bytes (PS_BODY) are written directly into _params without line buffering.
 * - Header lines that exceed WS_LINE_BUF_LEN are marked as overflow and skipped
 *   — we only need Content-Length, which is always short.
 */
void WebServer::_readClient() {
    while (_client.available()) {
        char c = (char)_client.read();

        // ── Body phase: read raw bytes ────────────────────────────────────────
        if (_parseState == PS_BODY) {
            if (_bodyReceived < WS_PARAMS_LEN - 1) {
                _params[_bodyReceived] = c;
            }
            _bodyReceived++;

            if (_bodyReceived >= _contentLength) {
                uint16_t len = (_contentLength < WS_PARAMS_LEN)
                               ? _contentLength
                               : WS_PARAMS_LEN - 1;
                _params[len] = '\0';
                _parseState = PS_DONE;
                return;
            }
            continue;
        }

        // ── Line-based phases ─────────────────────────────────────────────────
        if (c == '\r') continue;  // strip CR; LF triggers line processing

        if (c != '\n') {
            // Accumulate character
            if (_lineLen < WS_LINE_BUF_LEN - 1) {
                _lineBuf[_lineLen++] = c;
            } else {
                _lineOverflow = true;
            }
            continue;
        }

        // ── End of line (LF received) ─────────────────────────────────────────
        _lineBuf[_lineLen] = '\0';

        if (_parseState == PS_REQUEST_LINE) {
            if (!_parseRequestLine()) {
                _parseState = PS_ERROR;
                return;
            }
            _parseState   = PS_HEADERS;
            _lineLen      = 0;
            _lineOverflow = false;

        } else if (_parseState == PS_HEADERS) {
            if (_lineLen == 0 && !_lineOverflow) {
                // Blank line = end of headers
                if (_method == HTTP_POST && _contentLength > 0) {
                    _bodyReceived = 0;
                    _parseState   = PS_BODY;
                    // Stay in loop to read body bytes in the same call
                } else {
                    _parseState = PS_DONE;
                    return;
                }
            } else {
                if (!_lineOverflow) {
                    _processHeader();
                }
                _lineLen      = 0;
                _lineOverflow = false;
            }
        }
    }
}

// ─── _parseRequestLine() ─────────────────────────────────────────────────────

bool WebServer::_parseRequestLine() {
    // Expected format: "GET /path?query HTTP/1.1"
    char* p = _lineBuf;

    if (strncmp(p, "GET ", 4) == 0) {
        _method = HTTP_GET;
        p += 4;
    } else if (strncmp(p, "POST ", 5) == 0) {
        _method = HTTP_POST;
        p += 5;
    } else {
        _method = HTTP_UNKNOWN;
        return false;
    }

    // Skip extra spaces
    while (*p == ' ') p++;

    // Scan URL token (ends at space or end-of-string)
    const char* urlStart   = p;
    const char* queryStart = nullptr;
    bool        hasQuery   = false;

    while (*p && *p != ' ') {
        if (*p == '?' && !hasQuery) {
            hasQuery = true;
            // Save path up to '?'
            uint8_t pathLen = (uint8_t)(p - urlStart);
            if (pathLen >= WS_PATH_LEN) pathLen = WS_PATH_LEN - 1;
            strncpy(_path, urlStart, pathLen);
            _path[pathLen] = '\0';
            queryStart = p + 1;
        }
        p++;
    }

    if (!hasQuery) {
        uint8_t pathLen = (uint8_t)(p - urlStart);
        if (pathLen >= WS_PATH_LEN) pathLen = WS_PATH_LEN - 1;
        strncpy(_path, urlStart, pathLen);
        _path[pathLen] = '\0';
        _params[0] = '\0';
    } else if (_method == HTTP_GET && queryStart) {
        uint8_t qLen = (uint8_t)(p - queryStart);
        if (qLen >= WS_PARAMS_LEN) qLen = WS_PARAMS_LEN - 1;
        strncpy(_params, queryStart, qLen);
        _params[qLen] = '\0';
    }

    return true;
}

// ─── _processHeader() ────────────────────────────────────────────────────────

void WebServer::_processHeader() {
    // Only Content-Length is needed; all other headers are discarded
    if (_strEqCI(_lineBuf, "Content-Length:", 15)) {
        const char* p = _lineBuf + 15;
        while (*p == ' ') p++;
        _contentLength = (uint16_t)atoi(p);
    }
}

// ─── _dispatch() ─────────────────────────────────────────────────────────────

void WebServer::_dispatch(const HttpRequest& req) {
    bool pathFound = false;

    for (uint8_t i = 0; i < _routeCount; i++) {
        if (strcmp(req.path, _routes[i].path) == 0) {
            pathFound = true;
            if (_routes[i].method == req.method) {
                HttpResponse res(&_client);
                _routes[i].handler(req, res);
                if (!res.sent()) {
                    res.json(204, "{}");
                }
                return;
            }
        }
    }

    HttpResponse res(&_client);
    if (pathFound) {
        res.methodNotAllowed();
    } else {
        res.notFound();
    }
}

// ─── _strEqCI() ──────────────────────────────────────────────────────────────

bool WebServer::_strEqCI(const char* a, const char* b, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (tolower((uint8_t)a[i]) != tolower((uint8_t)b[i])) return false;
    }
    return true;
}
