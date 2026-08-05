#!/usr/bin/env python3
"""Stress test read-only del WebServer: reproduce los 502 (SYN drop / RST
cuando no queda socket LISTEN). Solo usa GET /status — nunca POSTs.

Uso: python3 tools/http_stress.py [ip]   (default 192.168.1.93)

Criterio pass (firmware con fix de LISTEN + timeout 500ms):
  - burst: 4/4 respuestas 200 (techo físico: el W5100 tiene 4 sockets;
    más SYNs en el MISMO instante dependen de reintentos TCP del cliente)
  - stall: staller recibe 408 en ~0.5s y las sondas responden en <1s

Al final imprime un análisis detallado que interpreta cada resultado.
"""
import socket
import sys
import threading
import time

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.1.93"
REQUEST = b"GET /status HTTP/1.1\r\nHost: d\r\nConnection: close\r\n\r\n"


def fetch(timeout=5.0):
    t0 = time.time()
    try:
        s = socket.create_connection((HOST, 80), timeout=timeout)
        s.sendall(REQUEST)
        s.settimeout(timeout)
        data = b""
        while chunk := s.recv(512):
            data += chunk
        s.close()
        status = data.split(b"\r\n", 1)[0].decode(errors="replace") if data else "EMPTY-CLOSE"
        return status, time.time() - t0
    except Exception as e:
        return f"{type(e).__name__}", time.time() - t0


def burst(n=4):
    print(f"== burst: {n} GET concurrentes ==")
    out = [None] * n
    threads = [threading.Thread(target=lambda i=i: out.__setitem__(i, fetch())) for i in range(n)]
    for t in threads: t.start()
    for t in threads: t.join()
    ok = sum(1 for r, _ in out if "200" in r)
    for r, dt in out:
        print(f"  {r:30s} ({dt:.2f}s)")
    print(f"  → {ok}/{n} OK")
    return out


def stall():
    print("== stall: request incompleto + sondas durante la ventana ==")
    results = []

    def staller():
        t0 = time.time()
        s = socket.create_connection((HOST, 80), timeout=5)
        s.sendall(b"GET /status HTTP/1.1\r\n")  # sin blank line: incompleto
        s.settimeout(8)
        data = b""
        try:
            while chunk := s.recv(512):
                data += chunk
        except socket.timeout:
            pass
        s.close()
        status = data.split(b"\r\n", 1)[0].decode(errors="replace") if data else "EMPTY-CLOSE"
        results.append(("STALLER", 0.0, status, time.time() - t0))

    def probe(tag, delay):
        time.sleep(delay)
        r, dt = fetch()
        results.append((tag, delay, r, dt))

    threads = [threading.Thread(target=staller)] + [
        threading.Thread(target=probe, args=(f"probe{i+1}", d))
        for i, d in enumerate([0.2, 0.4, 0.6, 0.8])
    ]
    for t in threads: t.start()
    for t in threads: t.join()

    results.sort(key=lambda x: x[1])
    for tag, delay, r, dt in results:
        print(f"  {tag:8s} t+{delay:<4} → {r:30s} ({dt:.2f}s)")
    return results


def analyze(burst_results, stall_results):
    """Interpreta los resultados crudos y explica qué significa cada uno."""
    W = print
    W()
    W("=" * 68)
    W("ANÁLISIS DETALLADO")
    W("=" * 68)

    # ── Burst ────────────────────────────────────────────────────────────
    W()
    W("[1] BURST — conexiones simultáneas contra los 4 sockets del W5100")
    W("    Qué mide: capacidad de aceptar SYNs que llegan en el mismo")
    W("    instante. El chip W5100 solo tiene 4 sockets de hardware: 1 en")
    W("    servicio + 1 LISTEN + 2 en cola. El firmware re-abre el LISTEN")
    W("    en cada loop() (~1 ms), así que ninguna conexión debe perderse.")
    W()
    direct = retried = failed = 0
    for r, dt in burst_results:
        if "200" not in r:
            failed += 1
        elif dt < 0.9:
            direct += 1
        else:
            retried += 1
    W(f"    · {direct} atendidas de forma directa (<0.9s): entraron a un")
    W("      socket libre y fueron servidas sin espera.")
    if retried:
        W(f"    · {retried} atendidas tras reintento TCP (~1s, ~2s, ~4s): su SYN")
        W("      llegó con los 4 sockets ocupados y el sistema operativo del")
        W("      cliente reintentó. NORMAL por encima de 4 simultáneas — no")
        W("      es pérdida, es encolado por hardware + reintento estándar.")
    if failed:
        W(f"    · {failed} FALLARON. Posibles causas, según el síntoma:")
        W("      - 'timeout' con connect lento  → sockets agotados sostenidamente")
        W("        (¿clientes reales compitiendo durante el test?)")
        W("      - 'ConnectionRefusedError'     → RST: nadie escucha en el puerto")
        W("        (firmware sin el fix de re-listen, o equipo recién booteado)")
        W("      - 'EMPTY-CLOSE'                → el equipo cerró sin responder:")
        W("        sospecha de crash/reboot (ver canario de watchdog volátil)")

    # ── Stall ────────────────────────────────────────────────────────────
    W()
    W("[2] STALL — cliente atascado a mitad de request + sondas concurrentes")
    W("    Qué mide: el peor caso histórico de los 502. Un cliente envía un")
    W("    request incompleto y se queda callado; el firmware debe (a)")
    W("    cortarlo con 408 al vencer WS_CLIENT_TIMEOUT_MS (500 ms) y (b)")
    W("    seguir aceptando y sirviendo a los demás mientras tanto.")
    W()
    staller = next((x for x in stall_results if x[0] == "STALLER"), None)
    probes = [x for x in stall_results if x[0] != "STALLER"]
    if staller:
        _, _, r, dt = staller
        if "408" in r and dt < 1.0:
            W(f"    · STALLER: 408 en {dt:.2f}s ✓ — el firmware detectó el atasco y")
            W("      respondió ANTES de cerrar (un cierre mudo es lo que el proxy")
            W("      traduce a 502). El valor ~0.5s confirma el timeout nuevo.")
        elif "408" in r:
            W(f"    · STALLER: 408 en {dt:.2f}s ⚠ — responde, pero el timeout es el")
            W("      viejo (3s): este binario NO tiene el fix de 500 ms. Cada")
            W("      atasco bloquea la cola 6× más de lo esperado.")
        else:
            W(f"    · STALLER: '{r}' ✗ — no hubo 408: cierre mudo o cuelgue.")
            W("      Firmware anterior al fix, o el equipo crasheó (502 seguro")
            W("      detrás de un proxy).")
    for tag, delay, r, dt in probes:
        if "200" in r and dt < 1.0:
            verdict, expl = "✓", "servida DURANTE el atasco — concurrencia sana"
        elif "200" in r and dt < 3.5:
            verdict, expl = "⚠", "servida recién al liberarse el staller — cola mono-cliente (fix de LISTEN ausente o degradado)"
        elif "200" in r:
            verdict, expl = "⚠", "servida solo tras reintentos SYN — sin LISTEN durante el atasco"
        else:
            verdict, expl = "✗", "nunca servida: socket perdido (conexión fantasma) o equipo caído"
        W(f"    · {tag} (t+{delay}s): {r} en {dt:.2f}s {verdict} — {expl}")

    # ── Veredicto ────────────────────────────────────────────────────────
    W()
    W("[3] VEREDICTO")
    burst_ok = all("200" in r for r, _ in burst_results)
    stall_ok = staller and "408" in staller[2] and staller[3] < 1.0 \
        and all("200" in r and dt < 1.0 for _, _, r, dt in probes)
    if burst_ok and stall_ok:
        W("    PASS — el servidor acepta conexiones durante todo el ciclo de")
        W("    vida de un request. Las condiciones que el proxy traducía a 502")
        W("    (SYN descartado, cierre mudo, cola bloqueada) no se reproducen.")
    else:
        W("    FAIL — hay una regresión en el manejo de concurrencia o de")
        W("    timeouts. Diagnóstico fino: GET /debug/sockets devuelve")
        W("    [SnSR, puerto, bytes RX] de los 4 sockets del W5100")
        W("    (SnSR: 0=CLOSED 20=LISTEN 23=ESTABLISHED 28=CLOSE_WAIT).")
        W("    Si además /status falla con otros endpoints sanos, sospechá")
        W("    memoria: canario = setear rf_timeout_seconds a un valor no-")
        W("    default, martillar /status y verificar que el valor sobrevive.")
    return burst_ok and stall_ok


if __name__ == "__main__":
    b = burst()
    s = stall()
    ok = analyze(b, s)
    sys.exit(0 if ok else 1)
