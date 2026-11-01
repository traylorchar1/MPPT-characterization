#!/usr/bin/env -S python3 -W ignore
"""Control a Rigol DP832 over USB.

  psu status                 show all channels
  psu set CH V [A]           set voltage (and current limit)
  psu on CH | psu off CH     toggle output (CH = 1, 2, 3, or all)
  psu ovp CH V | psu ocp CH A    set and enable protection
  psu raw 'SCPI'             send a raw command (queries end in ?)
"""
import sys
import pyvisa

RES = "USB0::6833::3601::DP8C274M00225::0::INSTR"


def chans(arg):
    return [1, 2, 3] if arg == "all" else [int(arg)]


def status(d):
    for ch in (1, 2, 3):
        _, v, a = d.query(f":APPL? CH{ch}").strip().split(",")
        mv, ma, mw = d.query(f":MEAS:ALL? CH{ch}").strip().split(",")
        out = d.query(f":OUTP? CH{ch}").strip()
        print(f"CH{ch} {out:>3}  set {float(v):6.3f} V {float(a):5.3f} A   "
              f"meas {float(mv):6.3f} V {float(ma):5.3f} A {float(mw):6.3f} W")


def main(argv):
    d = pyvisa.ResourceManager("@py").open_resource(RES, timeout=3000)
    cmd, args = (argv[0], argv[1:]) if argv else ("status", [])
    if cmd == "status":
        pass
    elif cmd == "set":
        ch, v = int(args[0]), float(args[1])
        d.write(f":APPL CH{ch},{v}" + (f",{float(args[2])}" if len(args) > 2 else ""))
    elif cmd in ("on", "off"):
        for ch in chans(args[0]):
            d.write(f":OUTP CH{ch},{cmd.upper()}")
    elif cmd in ("ovp", "ocp"):
        ch = int(args[0])
        d.write(f":OUTP:{cmd.upper()}:VAL CH{ch},{float(args[1])}")
        d.write(f":OUTP:{cmd.upper()} CH{ch},ON")
    elif cmd == "raw":
        q = args[0]
        print(d.query(q).strip() if q.rstrip().endswith("?") else d.write(q) and "ok")
        return
    else:
        sys.exit(__doc__)
    status(d)


if __name__ == "__main__":
    main(sys.argv[1:])
