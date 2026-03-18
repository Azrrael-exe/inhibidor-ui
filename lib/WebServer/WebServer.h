#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <Ethernet.h>

// ─── Buffer size constants ────────────────────────────────────────────────────

/** Maximum number of registered routes. Each route costs ~6 bytes of SRAM. */
#define WS_MAX_ROUTES       10

/** Maximum URL path length including null terminator. */
#define WS_PATH_LEN         64

/**
 * Maximum length for request params / body including null terminator.
 * Used for GET query strings and POST JSON bodies.
 */
#define WS_PARAMS_LEN       256

/**
 * Line buffer for parsing request line and headers one line at a time.
 * Must fit the request line: "POST /path?query HTTP/1.1" (typically < 220 bytes).
 * Headers that exceed this are silently skipped (we only need Content-Length).
 */
#define WS_LINE_BUF_LEN     256

// ─── HttpMethod ───────────────────────────────────────────────────────────────

enum HttpMethod : uint8_t {
    HTTP_GET     = 0,
    HTTP_POST    = 1,
    HTTP_UNKNOWN = 0xFF
};

// ─── HttpRequest ─────────────────────────────────────────────────────────────

/**
 * @brief Immutable view of an incoming HTTP request passed to route handlers.
 *
 * For GET requests:  params holds the URL query string (without leading '?').
 *                    e.g. "band=2&state=on"
 * For POST requests: params holds the raw JSON body.
 *                    e.g. '{"band":3}'
 *
 * Stack-allocated inside WebServer::update() during dispatch.
 * Memory: 1 + 64 + 256 = 321 bytes.
 */
struct HttpRequest {
    HttpMethod method;
    char       path[WS_PATH_LEN];
    char       params[WS_PARAMS_LEN];
};

// ─── HttpResponse ─────────────────────────────────────────────────────────────

/**
 * @brief JSON-only response builder bound to an active EthernetClient.
 *
 * All responses use Content-Type: application/json (including errors).
 * Writes directly to the EthernetClient socket — no intermediate buffer.
 * Uses the F() macro for header string literals to keep them in Flash.
 *
 * Typical usage in a handler:
 *   res.json(200, "{\"status\":\"ok\"}");
 *   res.badRequest("band out of range");  // → {"error":"band out of range"}
 *
 * Stack-allocated inside WebServer::update() during dispatch.
 * Memory: 2 bytes (pointer) + 1 byte (flag) = 3 bytes.
 */
class HttpResponse {
public:
    explicit HttpResponse(EthernetClient* client);

    /** Send a JSON response body with the given status code. */
    void json(uint16_t statusCode, const char* body);

    /** 404 {"error":"Not Found"} */
    void notFound();

    /** 400 {"error":"<msg>"} */
    void badRequest(const char* msg = "Bad Request");

    /** 500 {"error":"<msg>"} */
    void serverError(const char* msg = "Internal Server Error");

    /** 405 {"error":"Method Not Allowed"} */
    void methodNotAllowed();

    /** Returns true if a response has already been sent. */
    bool sent() const;

private:
    EthernetClient* _client;
    bool            _sent;

    void _send(uint16_t statusCode, const char* body);
    void _sendError(uint16_t statusCode, const char* msg);
    static const char* _phrase(uint16_t code);
};

// ─── Route handler type ───────────────────────────────────────────────────────

/**
 * @brief FastAPI-inspired handler signature.
 *
 * Example:
 *   void handlePostBand(const HttpRequest& req, HttpResponse& res) {
 *       res.json(200, "{\"accepted\":true}");
 *   }
 */
typedef void (*RouteHandler)(const HttpRequest& req, HttpResponse& res);

// ─── Internal route entry ─────────────────────────────────────────────────────

struct Route {
    const char*  path;      ///< Path literal (static lifetime required)
    RouteHandler handler;
    HttpMethod   method;
};

// ─── Parser state machine ─────────────────────────────────────────────────────

/**
 * Line-by-line HTTP parser states.
 *
 * Transitions:
 *   IDLE → REQUEST_LINE → HEADERS → BODY → DONE
 *                                 ↘ DONE  (GET)
 *   Any → ERROR on malformed request line
 */
enum ParseState : uint8_t {
    PS_IDLE,
    PS_REQUEST_LINE,
    PS_HEADERS,
    PS_BODY,
    PS_DONE,
    PS_ERROR
};

// ─── WebServer ────────────────────────────────────────────────────────────────

/**
 * @brief Non-blocking HTTP/1.1 server for ATmega2560 / W5100.
 *
 * Parses requests line-by-line: only the request line and Content-Length
 * header are retained; all other browser headers are discarded without
 * buffering, so there is no per-request buffer size limit on headers.
 *
 * All responses are JSON (Content-Type: application/json).
 *
 * Static SRAM footprint: ~466 bytes.
 * Peak stack during dispatch: ~324 bytes (HttpRequest + HttpResponse).
 *
 * Usage:
 *   WebServer server(80);
 *
 *   void setup() {
 *     byte mac[] = { 0xDE,0xAD,0xBE,0xEF,0xFE,0xED };
 *     Ethernet.begin(mac);
 *     delay(1000);
 *     server.begin();
 *     server.on("/api/status", HTTP_GET,  handleGetStatus);
 *     server.on("/api/band",   HTTP_POST, handlePostBand);
 *   }
 *
 *   void loop() {
 *     server.update();
 *   }
 */
class WebServer {
public:
    explicit WebServer(uint16_t port = 80);

    /** Start listening. Call after Ethernet.begin() in setup(). */
    void begin();

    /**
     * Register a route handler.
     * @param path   URL path (exact match, must have static lifetime).
     * @param method HTTP_GET or HTTP_POST.
     * @param handler Callback invoked on match.
     * @return true on success; false if route table is full.
     */
    bool on(const char* path, HttpMethod method, RouteHandler handler);

    /**
     * Service one iteration. Call every loop().
     * Non-blocking; processes one complete request per call.
     */
    void update();

private:
    // ── Route table ──────────────────────────────────────────────────────────
    uint16_t       _port;
    Route          _routes[WS_MAX_ROUTES];
    uint8_t        _routeCount;

    // ── Network ──────────────────────────────────────────────────────────────
    EthernetServer _server;
    EthernetClient _client;

    // ── Parser state ─────────────────────────────────────────────────────────
    ParseState _parseState;

    // Partially parsed request (built incrementally during parsing)
    HttpMethod _method;
    char       _path[WS_PATH_LEN];
    char       _params[WS_PARAMS_LEN];
    uint16_t   _contentLength;
    uint16_t   _bodyReceived;

    // Single-line accumulator (reused for request line and each header)
    char     _lineBuf[WS_LINE_BUF_LEN];
    uint16_t _lineLen;
    bool     _lineOverflow; ///< true when current line exceeded WS_LINE_BUF_LEN

    // ── Internal helpers ──────────────────────────────────────────────────────
    void _resetParser();
    void _readClient();
    bool _parseRequestLine();
    void _processHeader();
    void _dispatch(const HttpRequest& req);
    static bool _strEqCI(const char* a, const char* b, uint8_t len);
};

#endif // WEBSERVER_H
