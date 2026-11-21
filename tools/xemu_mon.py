#!/usr/bin/env python3
"""tools/xemu_mon.py — drive xemu's QEMU monitor: poll CPU state and dump
guest physical memory. Evidence is a file dump (brief rule 5).

usage: xemu_mon.py [--port 44556] [--eips N] [--dump PA LEN OUT]...
"""
import argparse, re, socket, sys, time

CTRL = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\r|\x08")

def connect(port, tries=40):
    for _ in range(tries):
        try:
            s = socket.create_connection(("127.0.0.1", port), 2)
            return s
        except OSError:
            time.sleep(0.5)
    sys.exit("monitor never came up")

def cmd(s, c, wait=0.6):
    s.sendall((c + "\n").encode())
    time.sleep(wait)
    s.settimeout(1.5)
    buf = b""
    try:
        while True:
            d = s.recv(65536)
            if not d:
                break
            buf += d
    except socket.timeout:
        pass
    return CTRL.sub("", buf.decode(errors="replace"))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=44556)
    ap.add_argument("--eips", type=int, default=3, help="sample EIP N times, 2 s apart")
    ap.add_argument("--dump", nargs="*", default=[],
                    help="triples of: guest-PA length outfile")
    a = ap.parse_args()
    s = connect(a.port)
    cmd(s, "info status")
    for i in range(a.eips):
        r = cmd(s, "info registers")
        m = re.search(r"EIP=([0-9a-f]+)", r)
        h = re.search(r"HLT=([01])", r)
        print(f"EIP@{i}: {m.group(1) if m else '?'} HLT={h.group(1) if h else '?'}")
        time.sleep(2)
    d = a.dump
    for i in range(0, len(d), 3):
        pa, ln, out = d[i], int(d[i + 1]), d[i + 2]
        # pmemsave writes guest physical memory to a file host-side
        r = cmd(s, f"pmemsave {pa} {ln} {out}", wait=2.0)
        print(f"pmemsave {pa} {ln} -> {out}: {r.strip().splitlines()[-1] if r.strip() else 'ok'}")

if __name__ == "__main__":
    main()
