#!/usr/bin/env -S python3 -W ignore
"""Read-only client for a TPEE SEC-B175-7A running Open-SEC firmware (V1.12).

Protocol is from ReboostTool (packet.cpp, commands.cpp) and the firmware's src/commands.c:
VESC-style frames  0x02 <len> <payload> <crc16 hi> <crc16 lo> 0x03,  115200 8N1,
floats are big-endian IEEE float32.

Only commands in READ_ONLY can be sent. In particular COMM_GET_MCCONF_DEFAULT (16) is
excluded: despite the name it loads factory defaults into the active config.

  tpee.py info          firmware version and config (limits, MPPT settings)
  tpee.py values        one telemetry sample
  tpee.py stream [Hz]   print telemetry continuously (what Reboost's "RT data" does)
  tpee.py enable on|off set Output Enable (config write; RAM only, verified)
  tpee.py set NAME=VAL ...  set tracking fields (PO_stepsize, PO_timestep_ms, PO_gain,
                        sweep_timestep, output_enable); RAM only, verified
  tpee.py hold V        hold the MPPT input at V volts (stops tracking); tpee.py track resumes
  tpee.py sweep LO HI   make the MPPT sweep its input voltage from LO to HI volts (128 steps),
                        park at the best point and resume tracking; prints what it found
"""
import glob
import struct
import sys
import time

import serial

COMM_FW_VERSION, COMM_GET_VALUES, COMM_SET_MCCONF, COMM_GET_MCCONF = 0, 4, 13, 14
COMM_MPPT_SWEEP = 100   # firmware's CONM_MPPT_GET_SWEEP: request = start a sweep, reply = its data
COMM_MPPT_SET_VIN = 105 # firmware's CONM_MPPT_SetInputVoltage: hold the input at a fixed voltage
READ_ONLY = {COMM_FW_VERSION, COMM_GET_VALUES, COMM_GET_MCCONF}
CONF_FMT = ">9fbbHfHffHbHHbHbHfH"   # V1.12 config layout, 73 bytes
CONF_ENABLE_FIELD = 9                # outputEnable (byte 36)
# Fields set_config may change (index into CONF_FMT). Limits, CAN and startup stay untouched.
WRITABLE = {"output_enable": 9, "PO_stepsize": 12, "PO_timestep_ms": 13, "PO_gain": 14,
            "sweep_timestep": 19}

MODES = ["CIV (input voltage ctrl)", "CIC (input current ctrl)", "MinInputCurrent",
         "COV (output voltage limit)", "COC (output current limit)", "TD (temp derating)",
         "Fault"]
FAULTS = ["OK", "ConfigError", "InputOverVolt", "OutputOverVolt", "OutputOverCurrent",
          "InputOverCurrent", "InputUnderCurrent", "PhaseOverCurrent", "Fault"]


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


class Reader:
    """Pops big-endian fields off a payload."""

    def __init__(self, b):
        self.b, self.i = b, 0

    def _take(self, fmt):
        v = struct.unpack_from(">" + fmt, self.b, self.i)[0]
        self.i += struct.calcsize(fmt)
        return v

    def f32(self): return self._take("f")
    def u8(self): return self._take("B")
    def i8(self): return self._take("b")
    def u16(self): return self._take("H")

    def cstr(self):
        end = self.b.index(0, self.i)
        s = self.b[self.i:end].decode(errors="replace")
        self.i = end + 1
        return s


class TPEE:
    def __init__(self, port=None, baud=115200):
        ports = [port] if port else glob.glob("/dev/cu.usbserial-*")
        if not ports:
            raise SystemExit("MPPT not found: no /dev/cu.usbserial-* port. Is its USB cable plugged in?")
        port = ports[0]
        self.s = serial.Serial(port, baud, timeout=0.5)
        self.rx = b""           # bytes carried between reads so unsolicited packets aren't lost
        self.last_sweep = None  # (time, [(V, A), ...]) from the most recent sweep report

    def request(self, cmd, timeout=1.0):
        if cmd not in READ_ONLY:
            raise ValueError(f"command {cmd} is not in the read-only whitelist")
        self._send(bytes([cmd]))
        return self._read_packet(cmd, timeout)

    def _send(self, payload):
        c = crc16(payload)
        self.s.write(bytes([2, len(payload)]) + payload + bytes([c >> 8, c & 0xFF, 3]))

    def _read_packet(self, want, timeout):
        buf = self.rx
        end = time.time() + timeout
        while time.time() < end:
            buf += self.s.read(self.s.in_waiting or 1)
            while len(buf) >= 5:
                if buf[0] == 2:
                    n, hdr = buf[1], 2
                elif buf[0] == 3:
                    n, hdr = (buf[1] << 8) | buf[2], 3
                else:
                    buf = buf[1:]
                    continue
                if len(buf) < hdr + n + 3:
                    break
                payload = buf[hdr:hdr + n]
                crc = (buf[hdr + n] << 8) | buf[hdr + n + 1]
                ok = buf[hdr + n + 2] == 3 and crc == crc16(payload)
                buf = buf[hdr + n + 3:] if ok else buf[1:]
                if ok and payload and payload[0] == COMM_MPPT_SWEEP:
                    self._record_sweep(payload[1:])
                if ok and payload and payload[0] == want:
                    self.rx = buf
                    return payload[1:]
        self.rx = buf
        raise TimeoutError(f"no reply to command {want}")

    def _record_sweep(self, d):
        n = d[0]
        pts = [(struct.unpack_from(">h", d, 1 + 4 * k)[0] / 1e2,
                struct.unpack_from(">h", d, 3 + 4 * k)[0] / 1e3) for k in range(n)]
        self.last_sweep = (time.time(), pts)

    def start_sweep(self, v_lo, v_hi):
        """Ask the firmware to sweep its input-voltage setpoint from v_lo to v_hi (128 steps,
        Sweep_timestep ms each), then park at the highest-power point and resume P&O. Only
        runs while the MPPT is tracking. No config change. Completion arrives as a sweep report
        (see last_sweep)."""
        self._send(bytes([COMM_MPPT_SWEEP]) + struct.pack(">hh", round(v_lo * 100), round(v_hi * 100)))

    def hold_input_voltage(self, v):
        """Switch the MPPT from P&O tracking to holding its input at v volts (firmware
        CONM_MPPT_SetInputVoltage). In V1.12 that handler has no `break` and falls through into
        COMM_MPPT_SCOPE_RUN, which parses scope settings from the rest of the packet; so a
        well-formed, minimal scope config is appended (16 samples, no pretrigger, divider 1,
        sources 0/0, no fault trigger, no auto trigger). That only arms a RAM capture."""
        payload = (bytes([COMM_MPPT_SET_VIN]) + struct.pack(">h", round(v * 100))
                   + struct.pack(">HHBBBbb", 16, 0, 1, 0, 0, 0, 0))
        self._send(payload)
        try:
            self._read_packet(COMM_MPPT_SET_VIN, 1.0)   # the fall-through's reply
        except TimeoutError:
            pass

    def resume_tracking(self):
        """Back to P&O: any config write re-inits the MPPT (init -> delay -> P&O). Rewrites the
        current Output Enable value, so nothing changes but the mode."""
        self.set_config(output_enable=self.config()["output_enable"])

    def sweep_and_wait(self, v_lo, v_hi, timeout=5.0, on_sample=None):
        """Start a sweep and poll telemetry until the report arrives. on_sample(values) is called
        for every poll (use it for safety checks). Returns the best (V, A) found, or None."""
        t0 = time.time()
        self.start_sweep(v_lo, v_hi)
        while time.time() - t0 < timeout:
            v = self.values()
            if on_sample:
                on_sample(v)
            if self.last_sweep and self.last_sweep[0] > t0:
                return max(self.last_sweep[1], key=lambda p: p[0] * p[1])
            time.sleep(0.02)
        return None

    def fw_version(self):
        r = Reader(self.request(COMM_FW_VERSION))
        major, minor, hw = r.u8(), r.u8(), r.cstr()
        return {"fw": f"{major}.{minor}", "hw": hw}

    def values(self):
        r = Reader(self.request(COMM_GET_VALUES))
        v = {"I_ind": r.f32(), "I_out": r.f32(), "I_in": r.f32(), "V_in": r.f32(),
             "V_out": r.f32(), "T_heatsink": r.f32(), "T_ambient": r.f32(), "eff": r.f32()}
        mode, fault = r.u8(), r.u8()
        v["mode"] = MODES[mode] if mode < len(MODES) else str(mode)
        v["fault"] = FAULTS[fault] if fault < len(FAULTS) else str(fault)
        v["P_in"], v["P_out"] = v["V_in"] * v["I_in"], v["V_out"] * v["I_out"]
        return v

    def set_config(self, **changes):
        """Guarded config write (same as editing fields in Reboost): read the active config,
        change only whitelisted fields, write it back with COMM_SET_MCCONF, and verify every
        other field came back unchanged. V1.12's set/get layouts are mirror images (73 bytes).
        RAM only: not stored to flash, so a power cycle restores the saved settings. Note the
        firmware halts and re-inits the converter on every write."""
        before = self.request(COMM_GET_MCCONF)
        if len(before) != 73:
            raise RuntimeError(f"unexpected config length {len(before)}; not writing")
        vals = list(struct.unpack(CONF_FMT, before))
        idx = {}
        for name, v in changes.items():
            if name not in WRITABLE:
                raise ValueError(f"{name} is not writable here")
            i = WRITABLE[name]
            vals[i] = type(vals[i])(v)
            idx[i] = vals[i]
        self._send(bytes([COMM_SET_MCCONF]) + struct.pack(CONF_FMT, *vals))
        self._read_packet(COMM_SET_MCCONF, 2.0)  # firmware acks with the command id
        # Compare decoded fields: the firmware stores volts/amps as mV/mA, so a float can come
        # back with its last bit flipped (e.g. 90.0 V). Allow that, nothing more.
        b = struct.unpack(CONF_FMT, before)
        a = struct.unpack(CONF_FMT, self.request(COMM_GET_MCCONF))
        close = lambda x, y: abs(x - y) <= 1e-4 * max(1.0, abs(x)) if isinstance(x, float) else x == y
        bad = [i for i in range(len(a)) if not close(idx.get(i, b[i]), a[i])]
        if bad:
            raise RuntimeError(f"verify failed on fields {bad}")
        return {n: a[WRITABLE[n]] for n in changes}

    def set_output_enable(self, on):
        return bool(self.set_config(output_enable=1 if on else 0)["output_enable"])

    def config(self):
        r = Reader(self.request(COMM_GET_MCCONF))
        c = {}
        for k in ("meter_filter", "Vout_limit", "Vin_limit", "Iout_limit", "Iin_max",
                  "phase_enable_I", "Iin_min", "temp_limit_start", "temp_limit_end"):
            c[k] = r.f32()
        c["output_enable"], c["output_enable_on_startup"], c["startup_delay_ms"] = \
            r.i8(), r.i8(), r.u16()
        c["PO_stepsize"], c["PO_timestep_ms"], c["PO_gain"] = r.f32(), r.u16(), r.f32()
        c["jump_P_threshold"], c["jump_rate"] = r.f32(), r.u16()
        c["sweep_enable"], c["sweep_interval"], c["sweep_timestep"] = r.i8(), r.u16(), r.u16()
        c["sweep_direction"], c["sweep_points"], c["sweep_on_can"] = r.i8(), r.u16(), r.i8()
        c["can_baud"], c["can_samplepoint"], c["can_id"] = r.u16(), r.f32(), r.u16()
        return c


def fmt_values(v):
    return (f"in {v['V_in']:7.3f} V {v['I_in']:6.3f} A {v['P_in']:7.2f} W | "
            f"out {v['V_out']:7.3f} V {v['I_out']:6.3f} A {v['P_out']:7.2f} W | "
            f"eff {v['eff']:.4g}  Tsink {v['T_heatsink']:.1f} C  {v['mode']}  fault {v['fault']}")


def main(argv):
    m = TPEE()
    cmd = argv[0] if argv else "values"
    if cmd == "info":
        print(m.fw_version())
        for k, v in m.config().items():
            print(f"  {k:26s} {v:.4g}" if isinstance(v, float) else f"  {k:26s} {v}")
    elif cmd == "values":
        print(fmt_values(m.values()))
    elif cmd == "enable":
        print("output_enable =", m.set_output_enable(argv[1] == "on"))
        time.sleep(1.0)
        print(fmt_values(m.values()))
    elif cmd == "set":
        kv = dict(a.split("=", 1) for a in argv[1:])
        print(m.set_config(**{k: float(v) for k, v in kv.items()}))
    elif cmd == "hold":
        m.hold_input_voltage(float(argv[1]))
        time.sleep(1.0)
        print(fmt_values(m.values()))
    elif cmd == "track":
        m.resume_tracking()
        time.sleep(1.0)
        print(fmt_values(m.values()))
    elif cmd == "sweep":
        best = m.sweep_and_wait(float(argv[1]), float(argv[2]))
        print("no sweep report (is the MPPT tracking?)" if best is None
              else f"sweep best: {best[0]:.2f} V {best[1]:.3f} A ({best[0] * best[1]:.1f} W)")
        time.sleep(0.5)
        print(fmt_values(m.values()))
    elif cmd == "stream":
        period = 1 / float(argv[1]) if len(argv) > 1 else 0.5
        try:
            while True:
                print(time.strftime("%H:%M:%S"), fmt_values(m.values()), flush=True)
                time.sleep(period)
        except KeyboardInterrupt:
            pass
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
