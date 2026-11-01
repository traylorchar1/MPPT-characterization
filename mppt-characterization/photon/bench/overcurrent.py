#!/usr/bin/env python3
"""Step the XPF CH2 current limit at a fixed voltage and check the Photon keeps
working: power drawn (XPF readback), safety/control state, DAC, temperature.

  overcurrent.py V I1 I2 ... [--dwell S] [--tmax C]
Aborts (CH2 off) on temperature above --tmax or a safety fault lasting > 2 s.
"""
import argparse
import csv
import os
import statistics as st
import sys
import threading
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from photon_can import telemetry  # noqa: E402
from svc import CONTROL, Service  # noqa: E402
from xpf import XPF  # noqa: E402

SAFETY = ['NO_ERROR', 'COMP_OV', 'BATT_OV', 'ARR_0_OV', 'ARR_1_OV', 'BATT_OC', 'ARR_0_OC', 'ARR_1_OC',
          'AMB_OT', 'ARR_0_OT', 'ARR_1_OT']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('v', type=float)
    ap.add_argument('currents', type=float, nargs='+')
    ap.add_argument('--dwell', type=float, default=25)
    ap.add_argument('--tmax', type=float, default=75)
    a = ap.parse_args()
    if a.v > 30 or max(a.currents) > 8.5 or a.v * max(a.currents) > 205:
        sys.exit('outside bench limits')
    s = Service()
    tele, lock = [], threading.Lock()

    def on(fr):
        if fr[1] == 0x600 and len(fr[3]) == 8:
            with lock:
                tele.append((fr[0],) + telemetry(fr[3]))
    s.b.listeners.append(on)
    x = XPF()
    os.makedirs('logs', exist_ok=True)
    path = time.strftime('logs/overcurrent_%Y%m%d_%H%M%S.csv')
    out = csv.writer(open(path, 'w', newline=''))
    out.writerow(['i_set', 't', 'xpf_v', 'xpf_i', 'safety', 'control', 'dac', 'i_term', 'temp_c', 'vout'])
    x.write('V2 %.3f' % a.v)
    x.write('I2 %.3f' % a.currents[0])
    x.write('OP2 1')
    abort = None
    for i_set in a.currents:
        x.write('I2 %.3f' % i_set)
        t0 = time.time()
        fault_since = None
        xs, states = [], []
        with lock:
            tele.clear()
        while time.time() - t0 < a.dwell and not abort:
            try:
                mv, mi = x.meas(2)
            except ValueError:
                continue
            sa, ct, dac, it, temp = (s.get('safety'), s.get('control'), s.get('c0.dac'), s.get('c0.i_term'),
                                     s.get('c0.temp') / 100.0)
            with lock:
                vout = tele[-1][3] if tele else 0
            out.writerow([i_set, round(time.time(), 3), mv, mi, sa, ct, dac, it, temp, vout])
            if time.time() - t0 > 8:
                xs.append((mv, mi))
            states.append((sa, ct, dac, temp))
            if temp > a.tmax:
                abort = 'temperature %.1f C' % temp
            if sa:
                fault_since = fault_since or time.time()
                if time.time() - fault_since > 2:
                    abort = 'safety fault %s for >2 s' % SAFETY[sa]
            else:
                fault_since = None
        with lock:
            tl = list(tele)
        faults = sorted({SAFETY[q[0]] for q in states if q[0]})
        ctl = sorted({CONTROL[q[1]] for q in states})
        if xs:
            p = [v * i for v, i in xs]
            print('I_lim %4.1f A: XPF %6.2f V %5.3f A (max %5.3f) %6.1f W = %5.1f%% of %5.1f W | dac %d..%d '
                  '(max_dac %d) | temp %.1f C | Vout %.1f V | control %s | faults %s' % (
                      i_set, st.mean(v for v, _ in xs), st.mean(i for _, i in xs), max(i for _, i in xs),
                      st.mean(p), 100 * st.mean(p) / (a.v * i_set), a.v * i_set,
                      min(q[2] for q in states), max(q[2] for q in states), s.get('state_max_dac'),
                      max(q[3] for q in states), st.mean(t[3] for t in tl) if tl else 0, ctl, faults or 'none'),
                  flush=True)
        if abort:
            break
    x.write('I2 2.0')
    x.write('OP2 0')
    s.b.close()
    print('ABORTED: ' + abort if abort else 'done; CH2 off', '| log', path)


if __name__ == '__main__':
    main()
