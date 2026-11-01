#!/usr/bin/env python3
"""Log Photon telemetry (both channels) and XPF CH2 readback, print 1 s summaries.

  monitor.py SECONDS [csv] [--on-after=S]   (--on-after switches XPF CH2 on at S seconds)
"""
import os
import csv
import statistics as st
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from photon_can import Bridge, telemetry  # noqa: E402
from xpf import XPF  # noqa: E402


def main():
    args = [x for x in sys.argv[1:] if not x.startswith('--on-after=')]
    on_after = [float(x.split('=')[1]) for x in sys.argv[1:] if x.startswith('--on-after=')]
    T = float(args[0]) if args else 30
    path = args[1] if len(args) > 1 else time.strftime('logs/mon_%Y%m%d_%H%M%S.csv')
    import os
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    rows = []
    lock = threading.Lock()
    b = Bridge()

    def on_frame(fr):
        t, cid, fl, d = fr
        if cid in (0x600, 0x601) and len(d) == 8:
            with lock:
                rows.append((t, 'photon%d' % (cid - 0x600)) + telemetry(d))
    b.listeners.append(on_frame)
    x = XPF()
    t0 = time.time()
    nxt = t0 + 1
    while time.time() - t0 < T:
        if on_after and time.time() - t0 >= on_after[0]:
            x.write('OP2 1')
            print('  CH2 ON', flush=True)
            on_after = []
        try:
            v, a = x.meas(2)
        except ValueError:
            continue
        with lock:
            rows.append((time.time(), 'xpf2', v, a, 0, 0))
        if time.time() >= nxt:
            nxt += 1
            with lock:
                recent = [r for r in rows if r[0] > time.time() - 1]
            line = '%5.1fs' % (time.time() - t0)
            for ch in ('photon0', 'photon1'):
                rr = [r for r in recent if r[1] == ch]
                if rr:
                    vin = [r[2] for r in rr]; iin = [r[3] for r in rr]; vo = [r[4] for r in rr]
                    line += '  %s Vin %6.2f (%5.2f..%5.2f) Iin %5.3f (%5.3f..%5.3f) Vout %6.2f' % (
                        ch[-1:], st.mean(vin), min(vin), max(vin), st.mean(iin), min(iin), max(iin), st.mean(vo))
            xr = [r for r in recent if r[1] == 'xpf2']
            if xr:
                line += '  | XPF2 %6.2f V %5.3f A %6.1f W' % (xr[-1][2], xr[-1][3], xr[-1][2] * xr[-1][3])
            print(line, flush=True)
    b.close()
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['t', 'src', 'v_in', 'i_in', 'v_out', 'temp'])
        w.writerows(rows)
    print('saved', path)


if __name__ == '__main__':
    main()
