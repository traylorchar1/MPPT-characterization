#!/usr/bin/env python3
"""Force a retrack and measure how long input power stays below 95 % of the
pre-retrack level (XPF CH2 readback).  retrack_test.py [repeats]"""
import os
import statistics as st
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from svc import Service  # noqa: E402
from xpf import XPF  # noqa: E402


def power(x):
    while True:
        try:
            v, i = x.meas(2)
            return time.time(), v * i
        except ValueError:
            pass


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    s, x = Service(), XPF()
    for k in range(n):
        t_end = time.time() + 4
        base = [power(x)[1] for _ in range(30)]
        p0 = st.median(base)
        s.retrack()
        t0 = time.time()
        low_since, recovered = None, None
        trace = []
        while time.time() - t0 < 12:
            t, p = power(x)
            trace.append((t - t0, p))
        below = [t for t, p in trace if p < 0.95 * p0]
        dur = (max(below) - min(below)) if below else 0
        energy_lost = sum(max(0, p0 - p) * dt for (t, p), (t2, _) in zip(trace, trace[1:]) for dt in [t2 - t])
        print('retrack %d: before %.1f W, below 95%% for %.2f s (first %.2f s), energy lost %.1f J' % (
            k + 1, p0, dur, min(below) if below else 0, energy_lost), flush=True)
        time.sleep(4)
    s.b.close()


if __name__ == '__main__':
    main()
