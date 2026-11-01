#!/usr/bin/env python3
"""Measure how well the Photon draws the corner power V_set*I_lim of the XPF CH2.

For each (V, I): set CH2, wait SETTLE s, then sample XPF readback and Photon
channel-0 telemetry for MEASURE s. Reports mean/min power as % of the corner,
voltage and current spread, and time spent near the corner.

  corners.py [--settle S] [--measure S] [--tag NAME] V:I [V:I ...]
"""
import argparse
import csv
import os
import statistics as st
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from photon_can import Bridge, telemetry  # noqa: E402
from xpf import XPF  # noqa: E402

V_MAX, I_MAX, P_MAX = 60.0, 4.5, 180.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('points', nargs='+')
    ap.add_argument('--settle', type=float, default=20)
    ap.add_argument('--measure', type=float, default=20)
    ap.add_argument('--tag', default='run')
    a = ap.parse_args()
    pts = [tuple(float(x) for x in p.split(':')) for p in a.points]
    for v, i in pts:
        if v > V_MAX or i > I_MAX or v * i > P_MAX:
            sys.exit('point %s:%s outside bench limits' % (v, i))
    os.makedirs('logs', exist_ok=True)
    stamp = time.strftime('%Y%m%d_%H%M%S')
    raw_path = 'logs/corners_%s_%s_raw.csv' % (a.tag, stamp)
    sum_path = 'logs/corners_%s_%s.csv' % (a.tag, stamp)
    b = Bridge()
    tele = []
    lock = threading.Lock()

    def on_frame(fr):
        if fr[1] == 0x600 and len(fr[3]) == 8:
            with lock:
                tele.append((fr[0],) + telemetry(fr[3]))
    b.listeners.append(on_frame)
    x = XPF()
    raw = []
    summary = []
    for v, i in pts:
        x.write('I2 %.3f' % i)
        x.write('V2 %.3f' % v)
        x.write('OP2 1')
        t_end = time.time() + a.settle
        while time.time() < t_end:
            x.meas(2)
        with lock:
            tele.clear()
        xs = []
        t_end = time.time() + a.measure
        while time.time() < t_end:
            mv, ma = x.meas(2)
            xs.append((time.time(), mv, ma))
        with lock:
            tl = list(tele)
        p = [mv * ma for _, mv, ma in xs]
        corner = v * i
        near = sum(1 for q in p if q >= 0.97 * corner) / len(p)
        row = {
            'v_set': v, 'i_set': i, 'corner_w': round(corner, 2),
            'p_mean_w': round(st.mean(p), 2), 'p_pct': round(100 * st.mean(p) / corner, 1),
            'p_min_pct': round(100 * min(p) / corner, 1), 'near97_frac': round(near, 2),
            'v_mean': round(st.mean(m for _, m, _ in xs), 2), 'v_sd': round(st.pstdev([m for _, m, _ in xs]), 2),
            'i_mean': round(st.mean(c for _, _, c in xs), 3), 'i_sd': round(st.pstdev([c for _, _, c in xs]), 3),
            'ph_vin_sd': round(st.pstdev([t[1] for t in tl]), 2) if tl else None,
            'ph_vout': round(st.mean(t[3] for t in tl), 2) if tl else None,
            'n_xpf': len(xs), 'n_tele': len(tl),
        }
        summary.append(row)
        print('%5.1f V %4.2f A  corner %6.1f W  mean %6.1f W (%5.1f%%)  min %5.1f%%  near %.0f%%  V %.2f+-%.2f  I %.3f+-%.3f  phVin_sd %s' % (
            v, i, corner, row['p_mean_w'], row['p_pct'], row['p_min_pct'], 100 * near,
            row['v_mean'], row['v_sd'], row['i_mean'], row['i_sd'], row['ph_vin_sd']), flush=True)
        raw += [('xpf', v, i, t, mv, ma, '') for t, mv, ma in xs]
        raw += [('photon0', v, i, t[0], t[1], t[2], t[3]) for t in tl]
    x.write('I2 1.0')
    x.write('V2 30')
    b.close()
    with open(sum_path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(summary[0]))
        w.writeheader()
        w.writerows(summary)
    with open(raw_path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['src', 'v_set', 'i_set', 't', 'v', 'i', 'v_out'])
        w.writerows(raw)
    print('saved', sum_path)


if __name__ == '__main__':
    main()
