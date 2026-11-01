#!/usr/bin/env -S python3 -W ignore
"""Read a BK 8502 electronic load's display through the webcam and log it.

Each frame is read by Claude (`claude -p`, using your Claude Code login), which finds
the load's display in the image, so the camera can move. Reads take ~4 s, so several
run in parallel to keep up.

  loadcam.py snap                grab one frame to disp.jpg and print the reading
  loadcam.py log [out.csv]       log load V/A (plus XPF readings) every --period seconds
  options:
    --cam NAME       avfoundation camera name or index (default "A Phone Camera"; names
                     are safer, indices reorder when the phone reconnects)
    --period S       seconds between readings (default 5)
    --crop W:H:X:Y   only send this region of the 1280x720 frame (default: whole frame)
    --model M        Claude model for reading (default sonnet)
    --no-psu         don't read the XPF
"""
import csv
import json
import re
import shutil
import signal
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
FRAME = HERE / "frame.jpg"
SHOTS = HERE / "shots"
PROMPT = (
    "Read {img}. Find the BK Precision 8502 electronic load: the instrument with GREEN "
    "seven-segment digits (voltage on the left of its display, current on the right). "
    "Read ONLY that green display, digit by digit. Never use digits from the red Sorensen "
    "power supply display next to it, even if they look similar. "
    "Voltage: up to 3 digits before the decimal point and at most 2 after (e.g. 115.03 has "
    "three digits before the point). The segmented character right after the voltage digits "
    "is the unit V, even though it looks like a 1 or 1' - it is not a digit. "
    "Current: typically 4 digits with 3 decimals (e.g. 1.492), followed by the letter A. "
    "Seven-segment 4 vs 9: a 4 has NO top segment and no bottom segment; a 9 has both. "
    'Reply with ONLY one line of JSON: {{"V": number or null, "A": number or null}}. Use null '
    "if a value is unreadable. No other text."
)


def opt(name, default):
    if name in sys.argv:
        i = sys.argv.index(name)
        val = sys.argv[i + 1]
        del sys.argv[i:i + 2]
        return val
    return default


def start_camera(cam, fps=1, tries=4):
    """Keep the camera open and overwrite FRAME fps times a second. Retries if ffmpeg exits at
    once (macOS can take a moment to release the camera from a previous process)."""
    for _ in range(tries - 1):
        proc = _spawn_camera(cam, fps)
        time.sleep(1.5)
        if proc.poll() is None:
            return proc
        time.sleep(1.5)
    return _spawn_camera(cam, fps)


def _spawn_camera(cam, fps):
    return subprocess.Popen(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
         "-f", "avfoundation", "-pixel_format", "nv12", "-framerate", "30",
         "-video_size", "1280x720", "-i", str(cam),
         "-vf", f"fps={fps}", "-update", "1", "-atomic_writing", "1", "-q:v", "2",
         str(FRAME)],
        stdin=subprocess.DEVNULL)


def wait_for_frame(cam_proc, after):
    for _ in range(100):
        if FRAME.exists() and FRAME.stat().st_mtime > after:
            return True
        if cam_proc.poll() is not None:
            sys.exit("ffmpeg exited; check --cam index and camera permission")
        time.sleep(0.1)
    return False


def crop(src, dst, region):
    if not region:
        shutil.copy(src, dst)
        return
    w, h, _, _ = region.split(":")
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", str(src),
                    "-vf", f"crop={region},scale={int(w) * 2}:{int(h) * 2}", str(dst)],
                   check=True)


def claude_read(img, model):
    """Ask Claude for the V/A on the display image. Returns ({'V','A'}, raw reply)."""
    r = subprocess.run(
        # Run from the image's folder (reads there are allowed by default); "//" marks an
        # absolute path in permission rules.
        ["claude", "-p", "--model", model, "--allowedTools", f"Read(/{Path(img).resolve()})"],
        input=PROMPT.format(img=img), capture_output=True, text=True, timeout=120,
        cwd=Path(img).resolve().parent)
    raw = r.stdout.strip()
    m = re.search(r"\{.*\}", raw, re.S)
    try:
        vals = json.loads(m.group(0)) if m else {}
    except json.JSONDecodeError:
        vals = {}
    V, A = vals.get("V"), vals.get("A")
    # The load shows at most 2 decimals in volts; a 3rd means the V glyph was read as a digit.
    if isinstance(V, (int, float)) and abs(V * 100 - round(V * 100)) > 1e-6:
        raw += f"  [rejected V={V}: more than 2 decimals]"
        V = None
    return {"V": V, "A": A}, raw


def psu_reader():
    sys.path.insert(0, str(Path.home()))
    from xpf import XPF
    p = XPF()

    def read():
        (v1, a1), (v2, a2) = p.meas(1), p.meas(2)
        return {"psu1_V": v1, "psu1_A": a1, "psu2_V": v2, "psu2_A": a2}
    return read


def main():
    cam = opt("--cam", "A Phone Camera")
    period = float(opt("--period", "5"))
    region = opt("--crop", "")
    model = opt("--model", "sonnet")
    use_psu = "--no-psu" not in sys.argv
    args = [a for a in sys.argv[1:] if a != "--no-psu"]
    cmd = args[0] if args else "snap"

    # Treat kill/TERM like Ctrl-C so the camera and pending reads get cleaned up.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))
    t0 = time.time()
    proc = start_camera(cam)
    pool = None
    try:
        wait_for_frame(proc, t0 + 1.5)  # let exposure settle
        if cmd == "snap":
            disp = HERE / "disp.jpg"
            crop(FRAME, disp, region)
            proc.terminate()
            vals, raw = claude_read(disp, model)
            print("reply:", raw)
            print("parsed:", vals)
            print("crop:", disp)
            return

        SHOTS.mkdir(exist_ok=True)
        out = Path(args[1] if len(args) > 1 else HERE / f"log_{datetime.now():%Y%m%d_%H%M%S}.csv")
        read_psu = psu_reader() if use_psu else None
        # Enough workers that ~4 s reads keep up with the requested period.
        pool = ThreadPoolExecutor(max_workers=max(1, int(8 / period) + 1))
        pending = []
        with out.open("w", newline="") as f:
            w = None
            print(f"logging to {out} every {period:g} s with {model}  (Ctrl-C to stop)")
            next_t = time.time()
            while True:
                # Snapshot the frame and PSU at the same moment, then read the display async.
                now = datetime.now()
                shot = SHOTS / f"{now:%Y%m%d_%H%M%S_%f}.jpg"
                crop(FRAME, shot, region)
                row = {"time": now.isoformat(timespec="milliseconds")}
                if read_psu:
                    row |= read_psu()
                pending.append((row, shot, pool.submit(claude_read, shot, model)))

                # Write finished rows in order.
                while pending and pending[0][2].done():
                    row, shot, fut = pending.pop(0)
                    try:
                        vals, raw = fut.result()
                    except Exception as e:
                        vals, raw = {"V": None, "A": None}, f"error: {e}"
                    V, A = vals["V"], vals["A"]
                    row = {"time": row["time"], "load_V": V, "load_A": A,
                           "load_W": round(V * A, 4) if V is not None and A is not None else None,
                           **{k: v for k, v in row.items() if k != "time"},
                           "shot": shot.name, "reply": raw}
                    if w is None:
                        w = csv.DictWriter(f, fieldnames=list(row))
                        w.writeheader()
                    w.writerow(row)
                    f.flush()
                    print(f"{row['time'][11:]}  load {V} V  {A} A"
                          + (f"   | psu1 {row['psu1_V']:.3f} V {row['psu1_A']:.3f} A"
                             if read_psu else ""))

                next_t += period
                time.sleep(max(0, next_t - time.time()))
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()
        if pool:
            pool.shutdown(wait=False, cancel_futures=True)


if __name__ == "__main__":
    main()
