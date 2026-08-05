#!/usr/bin/env python3
"""Stress test read-only del WebServer: reproduce los 502 (SYN drop / RST
cuando no queda socket LISTEN). Solo usa GET /status — nunca POSTs.

Uso: python3 tools/http_stress.py [ip]   (default 192.168.1.93)

Criterio pass (firmware con fix de LISTEN + timeout 500ms):
  - burst: 4/4 respuestas 200 (techo físico: el W5100 tiene 4 sockets;
    más SYNs en el MISMO instante dependen de reintentos TCP del cliente)
  - stall: staller recibe 408 en ~0.5s y las sondas responden en <1s
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
    return ok == n


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

    ok = True
    for tag, delay, r, dt in sorted(results, key=lambda x: x[1]):
        print(f"  {tag:8s} t+{delay:<4} → {r:30s} ({dt:.2f}s)")
        if tag == "STALLER":
            ok &= "408" in r
        else:
            ok &= "200" in r and dt < 1.0
    print(f"  → {'PASS' if ok else 'FAIL'} (staller=408, sondas 200 en <1s)")
    return ok


if __name__ == "__main__":
    b = burst()
    s = stall()
    sys.exit(0 if (b and s) else 1)
