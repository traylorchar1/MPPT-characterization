#!/usr/bin/env -S python3 -W ignore
"""Efficiency sweep of a TPEE SEC-B175-7A fed by the XPF (both channels in series) into a
BK 8502 load in CV mode, read through the phone camera.

Per point: set the XPF, wait until the MPPT and XPF readings settle, average them, grab a
camera frame, and move on. Claude reads the load display from that frame in the background,
so camera latency doesn't slow the sweep.

  sweep.py check                  preflight only (instruments, MPPT limits, camera)
  sweep.py monitor                read-only live readings for the dashboard (changes nothing)
  sweep.py run [out.csv]          full sweep
  Watch either one live with dashboard.py (http://localhost:8765).
  options:
    --vmin/--vmax/--vstep V       input voltage range (default 10..80 step 5, run high to low)
    --istep A                     current step (default 0.1)
    --imax A                      current cap (default 7.0, the MPPT rating)
    --pmax W                      input power cap (default 290, under the load's 300 W)
    --vload V                     load CV setpoint the MPPT output should sit at (default 130)
    --channel N                   drive only XPF channel N (1 or 2), other left off; max 60 V.
                                  Without it, both channels in series (max 120 V).
    --dry-run                     go through the motions without touching the XPF
    --dwell S                     seconds to hold each point before measuring (default 10)
    --env-v V                     cap currents for operation at V (not the setting), e.g. a converter
                                  that holds its own input voltage below the XPF setting
    --no-mppt                     converter has no data link (e.g. Dilithium Photon): no telemetry,
                                  no forced sweeps; start-up = converter pulls the XPF into current
                                  limit; efficiency from the XPF readback and the load photos only
    --hold                        MPPT holds its input 1 V below the XPF setting (firmware
                                  SetInputVoltage) instead of tracking: the XPF sits steadily in
                                  current limit, so readings don't swing. Tracking resumes at the end.
    --manual-read                 no background reads: each point's photos become one strip in
                                  <run>/readq/ that Claude reads in-session; merge.py builds the CSV
    --load-crop / --xpf-crop W:H:X:Y   override the strip crops (XPF crop follows --channel)
    --cam NAME                    camera (default "FaceTime HD Camera")
    --start-v V                   energize at V (if above the first point) so the MPPT can start,
                                  then step to the first point (needed for vmax below ~35 V)
    --aux-v V                     preflight: the other XPF channel must be ON at V (MPPT supply)
    --no-cam                      skip the camera; output power from MPPT telemetry only
                                  (eff_psu_mppt), so the load isn't independently measured
"""
import csv
import fcntl
import subprocess
import math
import re
import json
import os
import sys
import time
from concurrent.futures import Future, ThreadPoolExecutor
from datetime import datetime
from pathlib import Path
from statistics import mean, median

import numpy as np

HOME = Path.home()
_REPO = Path(__file__).resolve().parent.parent
sys.path[:0] = [str(_REPO / "tools"), str(_REPO / "tools" / "loadcam"), str(_REPO / "tpee")]
import loadcam  # noqa: E402
from tpee import TPEE  # noqa: E402
from xpf import XPF  # noqa: E402

HERE = Path(__file__).resolve().parent
CAM = "FaceTime HD Camera"   # override with --cam
CAM_FPS = 10            # frames at most 0.1 s old when a photo is taken
DISPLAY_LAG = 0.4       # s after settling before a frame counts (load display update + exposure)
DWELL = 10.0            # s to hold each point before measuring (override with --dwell)
AVG_WINDOW = 1.0        # s of samples averaged at the end of the dwell
MAX_EXTRA = 10.0        # s of extra 1 s windows allowed when the point isn't stable yet
MEAS_WINDOW = 2.4       # s over which instruments are averaged and photos taken
N_PHOTOS = 6            # photos of the load per point, read and averaged
READ_MODEL = "opus"
# Manual-read mode: crops (w:h:x:y in the 1280x720 frame) of the load display and the XPF's
# Output 1 display, stacked one row per photo into a strip that Claude reads in-session.
HOLD_MARGIN = 1.0       # --hold: MPPT input held this far below the XPF voltage (lead drop)
LOAD_CROP = "390:110:200:455"
XPF_CROP = "150:110:628:400"
XPF_CROP_CH2 = "220:110:855:405"   # XPF Output 2 display (used with --channel 2)
STRIP_SCALE = 2         # enlarge crops (laptop camera is farther from the displays)


def make_strip(photos, out):
    """One image per point: for each photo, the load display crop beside the XPF crop, top to
    bottom in photo order."""
    args = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
    for ph in photos:
        args += ["-i", str(ph)]
    parts = []
    for i in range(len(photos)):
        parts.append(f"[{i}]split[s{i}a][s{i}b];[s{i}a]crop={LOAD_CROP},scale=iw*{STRIP_SCALE}:-1[a{i}];"
                     f"[s{i}b]crop={XPF_CROP},scale=iw*{STRIP_SCALE}:-1[b{i}];[a{i}][b{i}]hstack=inputs=2[r{i}]")
    rows = "".join(f"[r{i}]" for i in range(len(photos)))
    graph = ";".join(parts) + (f";{rows}vstack=inputs={len(photos)}[out]" if len(photos) > 1
                               else ";[r0]copy[out]")
    subprocess.run(args + ["-filter_complex", graph, "-map", "[out]", str(out)], check=True)
OVP_PER_CH_MARGIN = 3   # V above the per-channel setpoint at vmax
XPF_VMAX = 60           # per channel


def opt(name, default, cast=float):
    if name in sys.argv:
        i = sys.argv.index(name)
        val = sys.argv[i + 1]
        del sys.argv[i:i + 2]
        return cast(val)
    return default


def xpf_envelope(v_ch):
    """Per-channel current limit of the XPF 60-10DP power envelope."""
    return float(np.interp(v_ch, [0, 16, 35, 60], [10, 10, 5, 3]))


def grid(vmin, vmax, vstep, istep, imax, pmax, nch=2, env_v=None):
    pts = []
    for v in np.arange(vmax, vmin - 1e-9, -vstep):
        vo = env_v or v   # operating voltage for the envelope/power cap (converter may pull the XPF below its setting)
        cap = min(imax, xpf_envelope(vo / nch), pmax / vo)
        n = int(np.floor(cap / istep + 1e-6))
        pts += [(round(float(v), 3), round(k * istep, 3)) for k in range(1, n + 1)]
    return pts


class Abort(Exception):
    pass


class Live:
    """Publishes the current state to live.json for dashboard.py (atomic, throttled)."""

    def __init__(self, path=HERE / "live.json"):
        self.path, self.state, self.t = path, {}, 0.0

    def update(self, force=False, **kw):
        self.state |= kw
        self.state["updated"] = time.time()
        if force or time.time() - self.t > 0.2:
            tmp = self.path.with_suffix(".tmp")
            tmp.write_text(json.dumps(self.state))
            os.replace(tmp, self.path)
            self.t = time.time()

    def sample(self, m, p):
        self.update(mppt=m, psu=p)


BOTH_PROMPT = (
    "Read {img}. It shows two instruments side by side. "
    "(1) The BK Precision 8502 electronic load: GREEN seven-segment digits, voltage on the left "
    "of its display, current on the right. Voltage has up to 3 digits before the decimal point "
    "and at most 2 after (e.g. 115.03). The segmented character right after the voltage digits "
    "is the unit V, even though it looks like a 1 or 1' - it is not a digit. Current is "
    "typically 4 digits with 3 decimals (e.g. 1.492) followed by A. "
    "(2) The Sorensen XPF power supply: RED seven-segment digits. Use its LEFT output (Output 1): "
    "the upper number is volts (e.g. 59.85), the lower is amps (e.g. 2.62). "
    "Read each display digit by digit and never mix digits between the two instruments. "
    "Seven-segment 4 vs 9: a 4 has NO top segment and no bottom segment; a 9 has both. "
    'Reply with ONLY one line of JSON: {{"load_V": n, "load_A": n, "psu_V": n, "psu_A": n}}, '
    "using null for anything unreadable. No other text."
)


def read_both(shot):
    """Read the load (output) and the XPF Output 1 (input) from the same photo."""
    r = subprocess.run(
        ["claude", "-p", "--model", READ_MODEL, "--allowedTools", f"Read(/{shot.resolve()})"],
        input=BOTH_PROMPT.format(img=shot.resolve()), capture_output=True, text=True,
        timeout=120, cwd=shot.resolve().parent)
    raw = r.stdout.strip()
    m = re.search(r"\{.*\}", raw, re.S)
    try:
        d = json.loads(m.group(0)) if m else {}
    except ValueError:
        d = {}
    v = {k: d.get(k) for k in ("load_V", "load_A", "psu_V", "psu_A")}
    # the load shows at most 2 decimals in volts; a 3rd means the V glyph was read as a digit
    if isinstance(v["load_V"], (int, float)) and abs(v["load_V"] * 100 - round(v["load_V"] * 100)) > 1e-6:
        v["load_V"] = None
    return v, raw


def combine_pairs(snaps, vload, p_in_window):
    """Combine a point's per-photo readings.

    Each photo gives the load's V/A and the XPF display's V/A from the same image. Both displays
    are smoothed readings, so their ratio is a consistent per-photo efficiency; it's used to drop
    implausible photos and, when >= 60% agree within 1 point of the median, transient ones.
    Primary efficiency = mean load power over the kept photos / the XPF's USB-average input power
    over the same measurement window (both time averages; the USB average has 1 mA resolution).
    The USB value sampled at the photo instant is NOT used: it's instantaneous and carries the
    MPPT's tracking ripple (seen 1.66-2.37 A at a 2.0 A limit)."""
    ok = []
    for sn in snaps:
        lv, la = sn.get("load_V"), sn.get("load_A")
        if lv is None or la is None or abs(lv - vload) > 5 or la < 0:
            continue
        pv, pa = sn.get("psu_photo_V"), sn.get("psu_photo_A")
        pin_photo = pv * pa if pv is not None and pa is not None and pv * pa > 0 else None
        ref = pin_photo or p_in_window
        if ref <= 0:
            continue
        eff_i = lv * la / ref
        if 0.5 < eff_i < 1.1:
            ok.append(sn | {"p_out": lv * la, "p_in_photo": pin_photo, "eff_i": eff_i})
    if not ok:
        return None
    med = median(x["eff_i"] for x in ok)
    close = [x for x in ok if abs(x["eff_i"] - med) <= 0.01]
    used = close if len(close) >= math.ceil(0.6 * len(ok)) else ok
    p_out = mean(x["p_out"] for x in used)
    photo_in = [x["p_in_photo"] for x in used if x["p_in_photo"]]
    return {
        "load_V": mean(x["load_V"] for x in used), "load_A": mean(x["load_A"] for x in used),
        "p_out": p_out, "p_in": p_in_window,
        "eff": p_out / p_in_window if p_in_window > 0 else None,
        "eff_photo": p_out / mean(photo_in) if photo_in else None,
        "psu_photo_V": mean(x["psu_photo_V"] for x in used if x["psu_photo_V"] is not None) if photo_in else None,
        "psu_photo_A": mean(x["psu_photo_A"] for x in used if x["psu_photo_A"] is not None) if photo_in else None,
        "n": len(used), "n_ok": len(ok),
        "eff_spread": max(x["eff_i"] for x in ok) - min(x["eff_i"] for x in ok),
        "A_spread": max(x["load_A"] for x in ok) - min(x["load_A"] for x in ok),
    }


def compute_row(row, snaps, vload, raw=""):
    """Turn a point's instrument averages plus its per-photo snapshots (each with the load and
    XPF readings from that photo) into the final CSV row. Shared by the sweep and merge.py."""
    row = dict(row)
    p_in_window = row["psu_V"] * row["psu_I"]   # XPF USB average over the measurement window
    comb = combine_pairs(snaps, vload, p_in_window) if snaps else None
    V, A = (comb["load_V"], comb["load_A"]) if comb else (None, None)
    p_in = p_in_window
    p_out = comb["p_out"] if comb else None
    mp_in, mp_out = row["mppt_V_in"] * row["mppt_I_in"], row["mppt_V_out"] * row["mppt_I_out"]
    row |= {
        "load_V": V, "load_A": A,
        "P_in_psu": round(p_in, 4), "P_out_load": round(p_out, 4) if p_out else None,
        # load power from the photos (robust mean) / XPF USB-average input power, same window
        "eff_bench": round(comb["eff"], 5) if comb and comb["eff"] else None,
        # load / the XPF display read from the same photos (snapshot of both instruments)
        "eff_photo_pair": round(comb["eff_photo"], 5) if comb and comb["eff_photo"] else None,
        # actual measured input (XPF USB average over the measurement window), not the setpoint
        "in_V_actual": round(row["psu_V"], 3),
        "in_I_actual": round(row["psu_I"], 4),
        "in_V_photo": round(comb["psu_photo_V"], 2) if comb and comb["psu_photo_V"] else None,
        "in_I_photo": round(comb["psu_photo_A"], 3) if comb and comb["psu_photo_A"] else None,
        "P_in_window_avg": round(row["psu_V"] * row["psu_I"], 4),
        "P_in_mppt": round(mp_in, 4), "P_out_mppt": round(mp_out, 4),
        "eff_mppt_telemetry": round(mp_out / mp_in, 5) if mp_in > 0 else None,
        # independent input (XPF), MPPT-reported output: usable when there's no camera
        "eff_psu_mppt": round(mp_out / p_in, 5) if p_in > 0 else None,
        # cross-reference: MPPT telemetry minus bench instruments
        "dVin": round(row["mppt_V_in"] - row["psu_V"], 4),
        "dIin": round(row["mppt_I_in"] - row["psu_I"], 4),
        "dVout": round(row["mppt_V_out"] - V, 4) if V is not None else None,
        "dIout": round(row["mppt_I_out"] - A, 4) if A is not None else None,
        "load_n": comb["n"] if comb else 0,
        "load_n_ok": comb["n_ok"] if comb else 0,
        "load_A_spread": round(comb["A_spread"], 3) if comb else None,
        "eff_spread": round(comb["eff_spread"], 4) if comb else None,
    }
    row["photos_json"] = json.dumps(snaps)
    row["shot"] = snaps[0]["file"] if snaps else ""
    row["reply"] = raw
    return row


class NoMPPT:
    """Stand-in for a converter with no data link (e.g. Dilithium Photon). Telemetry fields are
    NaN so nothing downstream mistakes them for measurements; comparisons with NaN are False,
    so telemetry-based safety checks are inert and the XPF-based ones carry the load."""
    NAN = float("nan")

    def values(self):
        n = self.NAN
        return {"I_ind": n, "I_out": n, "I_in": n, "V_in": n, "V_out": n, "T_heatsink": n,
                "T_ambient": n, "eff": n, "mode": "n/a", "fault": "OK", "P_in": n, "P_out": n}


class Bench:
    def __init__(self, dry, vload, channel=None, no_mppt=False):
        # One process at a time on the instruments: two readers on one serial port interleave.
        self.lock = open(HERE / ".bench.lock", "w")
        try:
            fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            sys.exit("another sweep.py (run or monitor) is using the instruments; stop it first")
        self.no_mppt = no_mppt
        self.psu, self.mppt, self.dry, self.vload = XPF(), (NoMPPT() if no_mppt else TPEE()), dry, vload
        self.set_v = self.set_i = 0.0
        self.live = Live()
        self.channel = channel      # None = both channels in series
        self.nch = 1 if channel else 2
        self.neg_since = None
        self.high_since = None
        self.spike_since = None
        self.iin_since = None
        self.starting = False
        self.dwell = DWELL
        self.hold = False       # --hold: MPPT holds a fixed input voltage instead of tracking
        self.hold_v = None

    def psu_read(self):
        (v1, a1), (v2, a2) = self.psu.meas(1), self.psu.meas(2)
        r = {"psu_V1": v1, "psu_V2": v2, "psu_I1": a1, "psu_I2": a2}
        # psu_V / psu_I: what the MPPT input actually sees from the active configuration
        if self.channel:
            r["psu_V"], r["psu_I"] = r[f"psu_V{self.channel}"], r[f"psu_I{self.channel}"]
        else:
            r["psu_V"], r["psu_I"] = v1 + v2, (a1 + a2) / 2
        return r

    def set_point(self, v, i):
        """Both channels at v/2 with the same current limit. Lower the current first when
        it is dropping so no intermediate state exceeds the power cap."""
        if self.dry:
            self.set_v, self.set_i = v, i
            return
        if self.channel:
            c = self.channel
            if i < self.set_i:
                self.psu.write(f"I{c} {i}"); self.psu.write(f"V{c} {v}")
            else:
                self.psu.write(f"V{c} {v}"); self.psu.write(f"I{c} {i}")
            self.set_v, self.set_i = v, i
            # not during start-up: the firmware only enables the power stage on its way into P&O
            if self.hold and not self.starting and self.hold_v != v - HOLD_MARGIN:
                self.hold_v = v - HOLD_MARGIN
                self.mppt.hold_input_voltage(self.hold_v)
            return
        if i < self.set_i:
            self.psu.write(f"I1 {i}"); self.psu.write(f"I2 {i}")
            self.psu.write(f"V1 {v / 2}"); self.psu.write(f"V2 {v / 2}")
        else:
            self.psu.write(f"V1 {v / 2}"); self.psu.write(f"V2 {v / 2}")
            self.psu.write(f"I1 {i}"); self.psu.write(f"I2 {i}")
        self.set_v, self.set_i = v, i

    def outputs_on(self):
        if not self.dry:
            if self.channel:
                self.psu.write(f"OP{self.channel} 1")
            else:
                self.psu.write("OPALL 1")  # both at once: a lagging channel would conduct in reverse

    def start_up(self, v, i, pmax, timeout=15):
        """Energize at the first point and wait for the MPPT to boost its output up to the
        load's CV setpoint. The output first charges from the input through the bypass
        diode, which needs to exceed 30 V for the MPPT to turn on."""
        # Energize at 0.1 A: until the load is absorbing at its CV voltage the MPPT may park on
        # the current-limited side of the supply's curve, where one series channel conducts
        # in reverse; at 0.1 A that diode current is negligible (rated 3 A).
        # Series: 0.1 A so a reversed channel's diode current stays negligible. Single channel:
        # nothing can reverse, and at 0.1 A the MPPT's input-current offset (~-0.06 A) swamps
        # its power signal so it can't track; start at 1 A (within the channel envelope).
        start_i = min(i, 0.1) if not self.channel else min(max(i, 1.0), xpf_envelope(v), 7.0)
        self.starting = True
        self.set_point(v, start_i)
        self.outputs_on()
        if self.dry:
            return
        self.starting = True
        t0 = time.time()
        if self.no_mppt:
            timeout = max(timeout, 30)
            while time.time() - t0 < timeout:
                m, p = self.mppt.values(), self.psu_read()
                self.live.sample(m, p)
                self.safety(m, p, pmax)
                if p["psu_I"] > 0.9 * start_i:
                    print(f"converter drawing {p['psu_I']:.2f} A at {p['psu_V']:.2f} V after "
                          f"{time.time() - t0:.1f} s (no telemetry: output not verified)")
                    self.starting = False
                    return
                time.sleep(0.1)
            raise Abort(f"converter drew only {p['psu_I']:.3f} A within {timeout} s: is its CAN/"
                        f"aux supply on and the load in CV mode at {self.vload:g} V with input on?")
        while time.time() - t0 < timeout:
            m, p = self.mppt.values(), self.psu_read()
            self.live.sample(m, p)
            self.safety(m, p, pmax)
            if abs(m["V_out"] - self.vload) < 3:
                print(f"MPPT up: output {m['V_out']:.1f} V after {time.time() - t0:.1f} s")
                self.starting = False
                if self.hold:
                    self.hold_v = v - HOLD_MARGIN
                    self.mppt.hold_input_voltage(self.hold_v)
                    print(f"MPPT holding input at {self.hold_v:g} V (tracking off)")
                return
            time.sleep(0.1)
        raise Abort(f"MPPT output reached {m['V_out']:.1f} V, not {self.vload:g} V, within "
                    f"{timeout} s: is the load in CV mode at {self.vload:g} V with its input on?")

    def outputs_off(self):
        if not self.dry:
            if self.channel:
                # only the sweep channel: the other one may be powering the MPPT (CAN 12 V)
                self.psu.write(f"OP{self.channel} 0")
            else:
                self.psu.write("OPALL 0")
                self.psu.write("OP1 0"); self.psu.write("OP2 0")

    def safety(self, m, p, pmax):
        """Real-time checks on every sample; raise Abort to shut the XPF off."""
        if p["psu_V"] > self.set_v + 2:
            raise Abort(f"XPF reads {p['psu_V']:.2f} V, above its {self.set_v:g} V setting: "
                        "something is back-feeding the input")
        if m["fault"] != "OK":
            raise Abort(f"MPPT fault {m['fault']}")
        # A series channel reading negative means it hit its current limit and the string
        # current flows through its reverse diode. That current can't exceed the set limit, and
        # the diode is rated 3 A continuous (transients higher). Abort only if it persists for
        # over 1 s with the limit above 2.5 A; otherwise the point is flagged in bypass_frac.
        if not self.channel and min(p["psu_V1"], p["psu_V2"]) < -0.3:
            now = time.time()
            self.neg_since = self.neg_since or now
            dur = now - self.neg_since
            # At or below 2.5 A the diode is inside its 3 A continuous rating: flag, don't stop.
            if self.set_i > 2.5 and dur > 1.0:
                raise Abort(f"XPF channel negative for {dur:.1f} s at {self.set_i:g} A (CH1 "
                            f"{p['psu_V1']:.2f} V, CH2 {p['psu_V2']:.2f} V): string current "
                            "is flowing through a channel's reverse diode")
        else:
            self.neg_since = None
        # The load's CV loop lets the output overshoot a few volts on steps (seen: +7 V), which
        # is harmless (MPPT caps itself at 147 V). A sustained rise means the load dropped out.
        if m["V_out"] > 145:
            raise Abort(f"MPPT output {m['V_out']:.1f} V, at its own limit: load dropped out?")
        if m["V_out"] > self.vload + 20:
            self.spike_since = self.spike_since or time.time()
            if time.time() - self.spike_since > 0.3:
                raise Abort(f"MPPT output {m['V_out']:.1f} V, {m['V_out'] - self.vload:.0f} V "
                            "above load CV for over 0.3 s: load dropped out?")
        else:
            self.spike_since = None
        if m["V_out"] > self.vload + 5:
            self.high_since = self.high_since or time.time()
            if time.time() - self.high_since > 1.0:
                raise Abort(f"MPPT output held at {m['V_out']:.1f} V, above load CV for over 1 s: "
                            "load dropped out?")
        else:
            self.high_since = None
        if m["V_in"] > 85:
            raise Abort(f"MPPT input {m['V_in']:.1f} V over 85 V")
        # The XPF's current limit caps the continuous input current; single samples spike (seen
        # 7.56 A with the XPF limited to 3 A: input caps / sensor), so require it to persist.
        if m["I_in"] > 7.3:
            self.iin_since = self.iin_since or time.time()
            if time.time() - self.iin_since > 0.3:
                raise Abort(f"MPPT input current {m['I_in']:.2f} A over 7.3 A for over 0.3 s")
        else:
            self.iin_since = None
        p_in = p["psu_V"] * (p["psu_I"] if self.channel else max(p["psu_I1"], p["psu_I2"]))
        if p_in > pmax + 15:
            raise Abort(f"input power {p_in:.0f} W over cap")
        if m["T_heatsink"] > 70:
            raise Abort(f"heatsink {m['T_heatsink']:.0f} C: add airflow")

    def window_stable(self, ms, ps):
        i_spread = max(p["psu_I"] for p in ps) - min(p["psu_I"] for p in ps)
        if self.no_mppt:
            return i_spread <= max(0.05, 0.05 * self.set_i)
        vo_spread = max(m["V_out"] for m in ms) - min(m["V_out"] for m in ms)
        return i_spread <= max(0.05, 0.05 * self.set_i) and vo_spread <= 1.5

    def at_mpp(self, p):
        """Supply at its corner (tracking mode) or, in hold mode, current-limited at set_i with the
        MPPT holding the input below the XPF voltage."""
        if self.hold:
            return (p["psu_I"] > self.set_i - max(0.03, 0.03 * self.set_i)
                    and p["psu_V"] < self.set_v - 0.3)
        """Supply at its corner: near full set voltage AND at its current limit. Left of the
        corner the MPPT has pulled the supply into current limit (voltage sags); right of it
        the MPPT isn't drawing the available current."""
        return (p["psu_V"] > 0.95 * self.set_v and
                p["psu_I"] > self.set_i - max(0.05, 0.1 * self.set_i))

    def force_sweep(self, pmax):
        """Make the firmware sweep its input setpoint around the target and park at the best
        point it finds, then resume P&O from there."""
        lo, hi = max(6.0, 0.8 * self.set_v), self.set_v + 2.0   # 128 steps: ~0.1 V resolution
        self.live.update(force=True, message=f"MPPT off its maximum power point: forcing a "
                                             f"sweep {lo:.0f}-{hi:.0f} V")
        # 128 steps at the MPPT's Sweep_timestep, plus its start delay and margin
        timeout = 128 * self.mppt.config()["sweep_timestep"] / 1000 + 3.0
        best = self.mppt.sweep_and_wait(
            lo, hi, timeout=timeout, on_sample=lambda m: self.safety(m, self.psu_read(), pmax))
        self.live.update(force=True, message="" if best else "sweep did not report back")
        return best

    def settle_and_sample(self, pmax, max_sweeps=3, snap=None):
        """Hold the point for self.dwell s, then average the last AVG_WINDOW s. From 3 s in, if
        the MPPT sits off the supply's corner for 2 s straight, force a firmware sweep and
        restart the hold (at most max_sweeps times). Safety-checked throughout."""
        t_point = t_first = time.time()
        ms, ps, bad_since, n_sweeps, sweep_best = [], [], None, 0, None
        while time.time() - t_point < self.dwell:
            m, p = self.mppt.values(), self.psu_read()
            self.live.sample(m, p)
            self.safety(m, p, pmax)
            now = time.time()
            if not self.hold and not self.no_mppt and now - t_point > 3.0 and not self.at_mpp(p) and n_sweeps < max_sweeps:
                bad_since = bad_since or now
                if now - bad_since > 2.0:
                    best = self.force_sweep(pmax)
                    n_sweeps += 1
                    sweep_best = best or sweep_best
                    t_point, bad_since, ms, ps = time.time(), None, [], []
                    continue
            else:
                bad_since = None
            if now - t_point >= self.dwell - AVG_WINDOW:
                ms.append(m); ps.append(p)
            time.sleep(0.02)
        # Stability gate: the MPPT and the load's CV loop sometimes oscillate (seen: input
        # current swinging ~0.9 A, output spiking to ~120 V). Keep taking fresh 1 s windows,
        # up to MAX_EXTRA s, until one is quiet.
        stable, t_extra = self.window_stable(ms, ps), time.time()
        while not stable and time.time() - t_extra < MAX_EXTRA:
            ms, ps, t_w = [], [], time.time()
            while time.time() - t_w < AVG_WINDOW:
                m, p = self.mppt.values(), self.psu_read()
                self.live.sample(m, p)
                self.safety(m, p, pmax)
                ms.append(m); ps.append(p)
                time.sleep(0.02)
            stable = self.window_stable(ms, ps)
        settle = time.time() - t_first
        # Measurement window: the load's current swings with the MPPT/CV-loop interplay (seen
        # 0.94 vs 1.51 A in photos 0.3 s apart), so one photo against a 1 s instrument average
        # doesn't line up. Sample everything over the same MEAS_WINDOW and take N_PHOTOS photos
        # spread across it; the load readings get averaged like the instrument readings.
        ms, ps, shots, t_m = [], [], [], time.time()
        while (el := time.time() - t_m) < MEAS_WINDOW:
            m, p = self.mppt.values(), self.psu_read()
            self.live.sample(m, p)
            self.safety(m, p, pmax)
            ms.append(m); ps.append(p)
            if snap and len(shots) < N_PHOTOS and el >= (len(shots) + 0.5) * MEAS_WINDOW / N_PHOTOS:
                shots.append({"file": snap(len(shots)), "t": time.time(),
                              "psu_V": p["psu_V"], "psu_I": p["psu_I"],
                              "mppt_V_out": m["V_out"], "mppt_I_out": m["I_out"]})
            time.sleep(0.02)
        row = {k: mean(p[k] for p in ps) for k in ps[0]}
        row["_shots"] = shots
        for k in ("V_in", "I_in", "V_out", "I_out", "I_ind", "T_heatsink", "eff"):
            row[f"mppt_{k}"] = mean(m[k] for m in ms)
        row["mppt_mode"] = ms[-1]["mode"]
        # Did the MPPT sit at the supply's corner (full set voltage at the current limit)? If it
        # pulled the supply down into current limit, this point is a different operating point.
        row["at_mpp"] = int(self.at_mpp(row))
        # fraction of averaged samples with a channel in reverse (MPPT sitting on the wrong step)
        row["bypass_frac"] = (0.0 if self.channel else
                              round(mean(min(p["psu_V1"], p["psu_V2"]) < -0.3 for p in ps), 2))
        row["settle_s"] = round(settle, 2)
        row["n_sweeps"] = n_sweeps
        row["stable"] = int(stable)
        row["sweep_best_V"] = round(sweep_best[0], 2) if sweep_best else None
        row["sweep_best_A"] = round(sweep_best[1], 3) if sweep_best else None
        # spread over the averaging window: how settled the point really was
        row["vin_spread"] = round(max(m["V_in"] for m in ms) - min(m["V_in"] for m in ms), 3)
        row["iin_spread"] = round(max(p["psu_I"] for p in ps) - min(p["psu_I"] for p in ps), 3)
        return row


def fresh_frame(after, cam_proc):
    """Copy the first camera frame written after `after` to a per-point file."""
    loadcam.wait_for_frame(cam_proc, after)
    return loadcam.FRAME


def preflight(b, vmax, vload):
    """Hard stops before anything is energized."""
    if b.no_mppt:
        problems = []
        print("no MPPT link: converter limits can't be checked; output protection relies on "
              "the converter's own output limit and the load")
        ovp_need = vmax / b.nch + OVP_PER_CH_MARGIN
        for ch in ([b.channel] if b.channel else (1, 2)):
            ovp = b.psu.num(f"OVP{ch}?")
            if ovp > ovp_need + 0.5:
                problems.append(f"XPF CH{ch} OVP {ovp:.1f} V; set it to {ovp_need:.1f} V "
                                f"(`~/xpf.py ovp {ch} {ovp_need:.1f}`)")
        return problems
    fw = b.mppt.fw_version()
    cfg = b.mppt.config()
    val = b.mppt.values()
    print(f"MPPT {fw['hw']} fw {fw['fw']}: Vout_limit {cfg['Vout_limit']:.0f} V, "
          f"Vin_limit {cfg['Vin_limit']:.0f} V, Iin_max {cfg['Iin_max']:.1f} A, fault {val['fault']}")
    problems = []
    if not vload + 5 <= cfg["Vout_limit"] <= 170:
        problems.append(f"MPPT Vout_limit {cfg['Vout_limit']:.0f} V should be {vload + 5:.0f}-170 V")
    if cfg["Vin_limit"] < vmax:
        problems.append(f"MPPT Vin_limit {cfg['Vin_limit']:.0f} V is below vmax {vmax:.0f} V")
    if vmax > 0.97 * vload:
        problems.append(f"vmax {vmax:.0f} V exceeds 0.97 x load CV ({0.97 * vload:.0f} V)")
    if val["fault"] != "OK":
        problems.append(f"MPPT fault {val['fault']}")
    if not cfg["output_enable"]:
        problems.append("MPPT output is disabled; enable it with `~/tpee/tpee.py enable on` "
                        "after the XPF is at a safe, matched setting")
    if vmax / b.nch > XPF_VMAX:
        problems.append(f"vmax {vmax:g} V is above what {b.nch} XPF channel(s) can supply")
    ovp_need = vmax / b.nch + OVP_PER_CH_MARGIN
    for ch in ([b.channel] if b.channel else (1, 2)):
        ovp = b.psu.num(f"OVP{ch}?")
        if ovp > ovp_need + 0.5:
            problems.append(f"XPF CH{ch} OVP {ovp:.1f} V; set it to {ovp_need:.1f} V "
                            f"(`~/xpf.py ovp {ch} {ovp_need:.1f}`)")
    return problems


def monitor(b, cam_period=3.0):
    """Read-only: poll the MPPT and XPF, read the load through the camera every few
    seconds, and publish it all for the dashboard. Sends nothing that changes state."""
    b.live.update(force=True, status="MONITOR", message="read-only monitoring", csv=None,
                  mppt_config=b.mppt.config(), vload=b.vload)
    cam = loadcam.start_camera(CAM, fps=2)
    pool = ThreadPoolExecutor(max_workers=1)
    fut, t_last = None, 0.0
    print("monitoring (read-only); Ctrl-C to stop")
    try:
        while True:
            m, p = b.mppt.values(), b.psu_read()
            b.live.sample(m, p)
            if fut and fut.done():
                try:
                    vals, _ = fut.result()
                    b.live.update(load={"V": vals["V"], "A": vals["A"], "t": t_last})
                except Exception:
                    pass
                fut = None
            if fut is None and time.time() - t_last > cam_period and loadcam.FRAME.exists():
                shot = HERE / "monitor_frame.jpg"
                shot.write_bytes(loadcam.FRAME.read_bytes())
                t_last = time.time()
                fut = pool.submit(loadcam.claude_read, shot, "sonnet")
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        cam.terminate()
        pool.shutdown(wait=False, cancel_futures=True)
        b.live.update(force=True, status="IDLE", message="monitor stopped")


def main():
    # SIGTERM (kill, pkill) behaves like Ctrl-C: the except blocks turn the XPF off. Background
    # jobs ignore SIGINT, so TERM is how a detached sweep gets stopped.
    import signal
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))
    vmin, vmax, vstep = opt("--vmin", 10.0), opt("--vmax", None), opt("--vstep", 5.0)
    istep, imax, pmax = opt("--istep", 0.1), opt("--imax", 7.0), opt("--pmax", 290.0)
    vload = opt("--vload", 130.0)
    channel = opt("--channel", None, int)
    globals()["LOAD_CROP"] = opt("--load-crop", LOAD_CROP, str)
    globals()["XPF_CROP"] = opt("--xpf-crop", XPF_CROP_CH2 if channel == 2 else XPF_CROP, str)
    globals()["CAM"] = opt("--cam", CAM, str)
    aux = opt("--aux-v", None)
    start_v = opt("--start-v", 0.0)             # other XPF channel must be ON at this voltage (MPPT CAN)
    dwell = opt("--dwell", DWELL)
    if channel not in (None, 1, 2):
        sys.exit("--channel must be 1 or 2")
    if vmax is None:
        vmax = float(XPF_VMAX) if channel else 80.0
    dry = "--dry-run" in sys.argv
    no_cam = "--no-cam" in sys.argv
    manual = "--manual-read" in sys.argv   # Claude reads each point's strip in-session
    hold = "--hold" in sys.argv            # MPPT holds input 1 V below the XPF instead of tracking
    no_mppt = "--no-mppt" in sys.argv      # converter with no data link (Photon): XPF + camera only
    args = [a for a in sys.argv[1:] if a not in ("--dry-run", "--no-cam", "--manual-read", "--hold", "--no-mppt")]
    if hold and no_mppt:
        sys.exit("--hold needs an MPPT link; it can't be used with --no-mppt")
    if manual:
        globals()["N_PHOTOS"] = 4
    cmd = args[0] if args else "check"

    env_v = opt("--env-v", None)
    pts = grid(vmin, vmax, vstep, istep, imax, pmax, 1 if channel else 2, env_v)
    print(f"{len(pts)} points, {vmax:g}->{vmin:g} V, up to {imax:g} A / {pmax:g} W"
          + ("  [DRY RUN: XPF untouched]" if dry else ""))
    b = Bench(dry, vload, channel, no_mppt)
    b.dwell = dwell
    b.hold = hold
    if cmd == "monitor":
        return monitor(b)
    problems = preflight(b, max(vmax, start_v), vload)
    if aux is not None and channel:
        o = 3 - channel
        on, v_aux = b.psu.query(f"OP{o}?") == "1", b.psu.meas(o)[0]
        print(f"XPF CH{o} (MPPT supply): {'ON' if on else 'OFF'} {v_aux:.2f} V")
        if not on or abs(v_aux - aux) > 0.5:
            problems.append(f"XPF CH{o} should be ON at {aux:g} V to power the MPPT")
    for p in problems:
        print("  PREFLIGHT:", p)
    if cmd == "check" or (problems and not dry):
        sys.exit(1 if problems else 0)

    out = Path(args[1] if len(args) > 1 else HERE / f"sweep_{datetime.now():%Y%m%d_%H%M%S}.csv")
    shots = out.with_suffix("")
    shots.mkdir(exist_ok=True)
    live = b.live
    live.update(force=True, status="STARTING", message="", csv=str(out.resolve()), n=len(pts),
                k=0, done=0, t_start=time.time(), dry=dry, vload=vload,
                grid={"vmin": vmin, "vmax": vmax, "vstep": vstep, "istep": istep,
                      "imax": imax, "pmax": pmax, "nch": 1 if channel else 2}, channel=channel)
    live.update(force=True, no_cam=no_cam, manual=manual, hold=hold)
    readq = shots / "readq"
    if manual:
        readq.mkdir(exist_ok=True)
        (readq / "meta.json").write_text(json.dumps({"csv": str(out.resolve()), "vload": vload}))
    cam = None if no_cam else loadcam.start_camera(CAM, fps=CAM_FPS)
    pool = ThreadPoolExecutor(max_workers=18)  # 6 photos/point, ~3 points of reads in flight
    pending, written, t_start = [], 0, time.time()

    def finish(row, photos, futs, f, w):
        snaps, raws = [], []
        for ph, fu in zip(photos, futs):
            try:
                v, raw = fu.result()
            except Exception as e:  # a failed read shouldn't kill the sweep
                v, raw = {"load_V": None, "load_A": None, "psu_V": None, "psu_A": None}, f"error: {e}"
            raws.append(f"{ph['file'].name}: {raw}")
            snaps.append({"file": ph["file"].name, "psu_V": ph["psu_V"], "psu_I": ph["psu_I"],
                          "mppt_V_out": ph["mppt_V_out"], "mppt_I_out": ph["mppt_I_out"],
                          "load_V": v["load_V"], "load_A": v["load_A"],
                          "psu_photo_V": v["psu_V"], "psu_photo_A": v["psu_A"]})
        raw = " | ".join(raws) if raws else "no camera"
        row = compute_row(row, snaps, vload, raw)
        V, A = row["load_V"], row["load_A"]
        p_in, p_out = row["P_in_psu"], row["P_out_load"]
        if w[0] is None:
            w[0] = csv.DictWriter(f, fieldnames=list(row))
            w[0].writeheader()
        w[0].writerow(row)
        f.flush()
        live.update(force=True, done=live.state.get("done", 0) + 1,
                    load={"V": V, "A": A, "t": time.time(), "point": [row["set_V"], row["set_I"]]})
        e = row["eff_bench"] or row["eff_psu_mppt"]
        print(f"  {row['set_V']:5.1f} V {row['set_I']:4.1f} A  settle {row['settle_s']:.2f}s  "
              f"in {p_in:6.1f} W  out {p_out or 0:6.1f} W  eff {e if e else '-'}  "
              f"telemetry eff {row['eff_mppt_telemetry']}  {row['mppt_mode']}", flush=True)

    try:
        with out.open("w", newline="") as f:
            w = [None]
            print(f"logging to {out}")
            if cam:
                loadcam.wait_for_frame(cam, time.time() + 0.5)
            # The MPPT only switches on with >30 V on its output; at low input voltages the
            # bypass diode can't get it there, so start up at start_v and step down.
            b.start_up(max(pts[0][0], start_v), pts[0][1], pmax)
            live.update(force=True, status="RUNNING")
            for k, (v, i) in enumerate(pts):
                b.set_point(v, i)
                live.update(k=k, set={"V": v, "I": i})
                row = {"time": datetime.now().isoformat(timespec="milliseconds"),
                       "set_V": v, "set_I": i}
                shot_base = shots / f"{k:04d}_{v:g}V_{i:g}A"

                def snap(j, base=shot_base):
                    ph = base.with_name(f"{base.name}_{j}.jpg")
                    ph.write_bytes(loadcam.FRAME.read_bytes())
                    return ph
                row |= b.settle_and_sample(pmax, snap=snap if cam else None)
                photos = row.pop("_shots")
                if manual:
                    make_strip([ph["file"] for ph in photos], readq / f"{k:04d}.png")
                    (readq / f"{k:04d}.json").write_text(json.dumps({
                        "k": k, "row": row,
                        "photos": [{"file": ph["file"].name, "psu_V": ph["psu_V"], "psu_I": ph["psu_I"],
                                    "mppt_V_out": ph["mppt_V_out"], "mppt_I_out": ph["mppt_I_out"]}
                                   for ph in photos]}))
                    live.update(force=True, queued=k + 1,
                                done=len(list(readq.glob("*.reading.json"))))
                    print(f"  {v:5.1f} V {i:4.1f} A  settle {row['settle_s']:.2f}s  queued strip "
                          f"{k:04d}.png  in {row['psu_V'] * row['psu_I']:6.1f} W  "
                          f"stable {row['stable']} mpp {row['at_mpp']} sweeps {row['n_sweeps']}", flush=True)
                    continue
                futs = [pool.submit(read_both, ph["file"]) for ph in photos]
                pending.append((row, photos, futs))
                while pending and all(fu.done() for fu in pending[0][2]):
                    finish(*pending.pop(0), f, w)
                    written += 1
            b.outputs_off()
            print("sweep done, XPF outputs off; waiting for remaining camera reads")
            live.update(force=True, status="FINISHING", k=len(pts),
                        message="XPF outputs off; finishing camera reads")
            for item in pending:
                finish(*item, f, w)
            live.update(force=True, status="DONE", message="sweep complete, XPF outputs off")
    except Abort as e:
        b.outputs_off()
        print(f"\nABORT: {e}\nXPF outputs turned OFF.")
        live.update(force=True, status="ABORTED", message=f"{e}. XPF outputs turned off.")
    except KeyboardInterrupt:
        b.outputs_off()
        print("\nstopped by user; XPF outputs turned OFF.")
        live.update(force=True, status="STOPPED", message="stopped by user, XPF outputs off")
    except BaseException as e:
        # Any other failure (a bug, a lost serial port) must never leave the supply on.
        try:
            b.outputs_off()
            off = "XPF outputs turned OFF."
        except Exception as e2:
            off = f"COULD NOT TURN XPF OFF ({e2}): switch it off by hand!"
        print(f"\nCRASH: {type(e).__name__}: {e}\n{off}")
        live.update(force=True, status="ABORTED", message=f"crash: {type(e).__name__}: {e}. {off}")
        raise
    finally:
        if hold and not dry:
            try:
                b.mppt.resume_tracking()
            except Exception as e:
                print(f"could not resume MPPT tracking: {e}")
        if cam:
            cam.terminate()
        pool.shutdown(wait=False, cancel_futures=True)
        print(f"{time.time() - t_start:.0f} s elapsed")


if __name__ == "__main__":
    main()
