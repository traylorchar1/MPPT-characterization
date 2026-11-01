#!/usr/bin/env python3
"""Trace tracker internals (debug stream base+0xE1) with XPF CH2 readback.

  trace.py SECONDS [--v V --i A] [--on-after S] [--csv path]
Prints 0.2 s rows: XPF V/I/P, channel-0 goal V, DAC, P&O mean V, alg/dir/clamp/control, step.
"""
import argparse
import csv
import os
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from photon_can import Bridge, telemetry  # noqa: E402
from svc import Service  # noqa: E402
from xpf import XPF  # noqa: E402

VS = 0.020391516


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('seconds', type=float)
    ap.add_argument('--v', type=float)
    ap.add_argument('--i', type=float)
    ap.add_argument('--on-after', type=float)
    ap.add_argument('--csv')
    ap.add_argument('--every', type=float, default=0.2)
    a = ap.parse_args()
    b = Bridge()
    s = Service(b)
    s.set('stream', 1)
    rows, lock = [], threading.Lock()
    last = {}

    def on_frame(fr):
        t, cid, fl, d = fr
        if cid == 0x6E1 and len(d) == 8:
            g, dac, va = struct.unpack('<HHH', d[:6])
            with lock:
                last['dbg'] = (g, dac, va, d[6], d[7])
                rows.append((t, 'dbg', g, dac, va, d[6], d[7]))
        elif cid == 0x600 and len(d) == 8:
            with lock:
                last['tel'] = telemetry(d)
                rows.append((t, 'tel') + telemetry(d))
    b.listeners.append(on_frame)
    x = XPF()
    if a.v is not None:
        x.write('V2 %.3f' % a.v)
    if a.i is not None:
        x.write('I2 %.3f' % a.i)
    t0 = time.time()
    nxt = t0
    on_after = a.on_after
    while time.time() - t0 < a.seconds:
        if on_after is not None and time.time() - t0 >= on_after:
            x.write('OP2 1')
            on_after = None
        try:
            v, i = x.meas(2)
        except ValueError:
            continue
        rows.append((time.time(), 'xpf', v, i))
        if time.time() >= nxt:
            nxt += a.every
            with lock:
                dbg = last.get('dbg')
                tel = last.get('tel')
            line = '%6.2fs XPF %6.2fV %5.3fA %6.1fW' % (time.time() - t0, v, i, v * i)
            if dbg:
                g, dac, va, fl, st = dbg
                line += ' | goal %6.2fV dac %4d avg %6.2fV alg %d dir %s clamp %d ctl %2d step %3d' % (
                    g * VS, dac, va * VS, fl & 3, 'D' if fl & 4 else 'U', (fl >> 3) & 1, fl >> 4, st)
            if tel:
                line += ' | ph %6.2fV %5.3fA' % (tel[0], tel[1])
            print(line, flush=True)
    s.set('stream', 0)
    b.close()
    path = a.csv or time.strftime('logs/trace_%Y%m%d_%H%M%S.csv')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='') as f:
        csv.writer(f).writerows(rows)
    print('saved', path)


if __name__ == '__main__':
    main()
