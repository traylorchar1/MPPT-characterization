#!/usr/bin/env python3
"""Fixed-corner soak: hold XPF CH2 at V / I for a duration and log everything.

  soak.py V I MINUTES [--tmax C]

CH2 is switched off on exit for any reason, on Photon temperature above
--tmax, on a safety fault lasting > 2 s, or when Photon telemetry stops for
5 s (e.g. the CAN/Ethernet link drops). One summary line per minute.
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
    ap.add_argument('i', type=float)
    ap.add_argument('minutes', type=float)
    ap.add_argument('--tmax', type=float, default=75)
    a = ap.parse_args()
    if a.v > 30 or a.i > 8.5 or a.v * a.i > 205:
        sys.exit('outside bench limits')
    os.makedirs('logs', exist_ok=True)
    stamp = time.strftime('%Y%m%d_%H%M%S')
    path = 'logs/soak_%s.csv' % stamp
    out = csv.writer(open(path, 'w', newline=''))
    out.writerow(['t', 'xpf_v', 'xpf_i', 'ph_vin', 'ph_iin', 'ph_vout', 'ph_temp', 'safety', 'control', 'dac'])
    x = XPF()
    s = Service()
    last = {'tel': None, 't': time.time()}
    lock = threading.Lock()

    def on(fr):
        if fr[1] == 0x600 and len(fr[3]) == 8:
            with lock:
                last['tel'] = telemetry(fr[3])
                last['t'] = fr[0]
    s.b.listeners.append(on)
    abort = None
    t0 = time.time()
    minute = []
    try:
        x.write('OP2 0')
        x.write('V2 %.3f' % a.v)
        x.write('I2 %.3f' % a.i)
        x.write('OP2 1')
        print('soak %.1f V / %.2f A for %.0f min, log %s' % (a.v, a.i, a.minutes, path), flush=True)
        next_poll = 0
        sa = ct = dac = 0
        fault_since = None
        while time.time() - t0 < a.minutes * 60:
            try:
                mv, mi = x.meas(2)
            except ValueError:
                continue
            now = time.time()
            with lock:
                tel, tel_t = last['tel'], last['t']
            if now - tel_t > 5:
                abort = 'no Photon telemetry for %.0f s' % (now - tel_t)
                break
            if now >= next_poll:
                next_poll = now + 1
                try:
                    sa, ct, dac = s.get('safety'), s.get('control'), s.get('c0.dac')
                except (TimeoutError, OSError, RuntimeError):
                    pass
            temp = tel[3] if tel else 0
            out.writerow([round(now, 3), mv, mi] + (list(tel) if tel else [0, 0, 0, 0]) + [sa, ct, dac])
            minute.append((mv, mi, temp, tel[2] if tel else 0, sa, ct))
            if temp > a.tmax:
                abort = 'Photon temperature %.1f C > %.0f C' % (temp, a.tmax)
                break
            if sa:
                fault_since = fault_since or now
                if now - fault_since > 2:
                    abort = 'safety fault %s for > 2 s' % SAFETY[sa]
                    break
            else:
                fault_since = None
            if len(minute) and now - t0 >= 60 * (int((now - t0) / 60)) and int((now - t0) / 60) > getattr(main, 'm', 0):
                main.m = int((now - t0) / 60)
                p = [v * i for v, i, *_ in minute]
                print('%3d min: XPF %6.2f V %5.3f A (max %5.3f) %6.1f W = %5.1f%% | temp %5.1f C (max %5.1f) '
                      '| Vout %6.1f V | control %s | faults %s' % (
                          main.m, st.mean(q[0] for q in minute), st.mean(q[1] for q in minute),
                          max(q[1] for q in minute), st.mean(p), 100 * st.mean(p) / (a.v * a.i),
                          st.mean(q[2] for q in minute), max(q[2] for q in minute), st.mean(q[3] for q in minute),
                          sorted({CONTROL[q[5]] for q in minute}),
                          sorted({SAFETY[q[4]] for q in minute if q[4]}) or 'none'), flush=True)
                minute = []
    finally:
        x.write('OP2 0')
        time.sleep(0.3)
        x.write('I2 2.0')
        s.b.close()
        print(('ABORTED: ' + abort) if abort else 'complete', '| CH2 off | log', path, flush=True)


if __name__ == '__main__':
    main()
