#!/usr/bin/env -S python3 -W ignore
"""Control a Sorensen XPF 60-10DP over its USB serial port.

  xpf status                 show both channels
  xpf set CH V [A]           set voltage (and current limit)
  xpf on CH | xpf off CH     toggle output (CH = 1, 2, or all)
  xpf ovp CH V | xpf ocp CH A    set protection trip points
  xpf raw 'CMD'              send a raw command (queries end in ?)
"""
import glob
import sys
import time

import serial

PORT_GLOB = "/dev/cu.usbmodemJ00412501*"


class XPF:
    def __init__(self, port=None):
        port = port or glob.glob(PORT_GLOB)[0]
        self.s = serial.Serial(port, 9600, timeout=2)

    def write(self, cmd):
        self.s.write((cmd + "\n").encode())
        time.sleep(0.05)

    def query(self, cmd):
        self.s.reset_input_buffer()
        self.s.write((cmd + "\n").encode())
        return self.s.readline().decode(errors="replace").strip()

    def num(self, cmd):
        """Query and strip the echoed prefix/units, e.g. 'V1 16.16' or '16.147V' -> float."""
        return float(self.query(cmd).split()[-1].rstrip("VA"))

    def meas(self, ch):
        return self.num(f"V{ch}O?"), self.num(f"I{ch}O?")

    def status(self):
        for ch in (1, 2):
            v, a = self.meas(ch)
            out = "ON" if self.query(f"OP{ch}?") == "1" else "OFF"
            print(f"CH{ch} {out:>3}  set {self.num(f'V{ch}?'):6.2f} V {self.num(f'I{ch}?'):5.2f} A   "
                  f"meas {v:7.3f} V {a:6.3f} A   OVP {self.num(f'OVP{ch}?'):.1f} V  "
                  f"OCP {self.num(f'OCP{ch}?'):.2f} A")


def chans(arg):
    return [1, 2] if arg == "all" else [int(arg)]


def main(argv):
    p = XPF()
    cmd, args = (argv[0], argv[1:]) if argv else ("status", [])
    if cmd == "status":
        pass
    elif cmd == "set":
        ch = int(args[0])
        p.write(f"V{ch} {float(args[1])}")
        if len(args) > 2:
            p.write(f"I{ch} {float(args[2])}")
    elif cmd in ("on", "off"):
        for ch in chans(args[0]):
            p.write(f"OP{ch} {1 if cmd == 'on' else 0}")
    elif cmd in ("ovp", "ocp"):
        p.write(f"{cmd.upper()}{int(args[0])} {float(args[1])}")
    elif cmd == "raw":
        q = args[0]
        print(p.query(q) if q.rstrip().endswith("?") else (p.write(q) or "ok"))
        return
    else:
        sys.exit(__doc__)
    time.sleep(0.5)
    p.status()


if __name__ == "__main__":
    main(sys.argv[1:])
