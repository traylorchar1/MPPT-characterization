#!/usr/bin/env -S python3 -W ignore
"""Build a manual-read sweep's CSV from its queued points and Claude's readings.

  merge.py RUN_DIR                  rebuild RUN_DIR's CSV from readq/*.json + *.reading.json
  merge.py RUN_DIR K READINGS       save readings for point K, then rebuild
      READINGS: JSON list, one entry per photo in strip order (top to bottom):
                [load_V, load_A, psu_V, psu_A], null for anything unreadable
  merge.py RUN_DIR --pending        list points still waiting for a reading
"""
import csv
import json
import sys
from pathlib import Path

sys.argv, argv = sys.argv[:1], sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import sweep  # noqa: E402

run = Path(argv[0])
readq = run / "readq"
meta = json.loads((readq / "meta.json").read_text())

if len(argv) > 1 and argv[1] == "--pending":
    todo = [q.stem for q in sorted(readq.glob("[0-9][0-9][0-9][0-9].json"))
            if not (readq / f"{q.stem}.reading.json").exists()]
    print(" ".join(todo) if todo else "none pending")
    sys.exit()
if len(argv) > 2:
    k, vals = int(argv[1]), json.loads(argv[2])
    (readq / f"{k:04d}.reading.json").write_text(json.dumps(vals))

rows = []
for q in sorted(readq.glob("[0-9][0-9][0-9][0-9].json")):
    rd = readq / f"{q.stem}.reading.json"
    if not rd.exists():
        continue
    point, reads = json.loads(q.read_text()), json.loads(rd.read_text())
    snaps = []
    for ph, r in zip(point["photos"], reads):
        r = (r or []) + [None] * (4 - len(r or []))
        snaps.append(ph | {"load_V": r[0], "load_A": r[1], "psu_photo_V": r[2], "psu_photo_A": r[3]})
    raw = "read in-session by Claude: " + json.dumps(reads)
    rows.append(sweep.compute_row(point["row"], snaps, meta["vload"], raw))
if rows:
    keys = list(dict.fromkeys(k for r in rows for k in r))
    out = Path(meta["csv"])
    tmp = out.with_suffix(".tmp")
    with tmp.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys)
        w.writeheader()
        w.writerows(rows)
    tmp.replace(out)
    last = rows[-1]
    print(f"{len(rows)} points merged -> {out.name} | last {last['set_V']} V {last['set_I']} A: "
          f"in {last['in_V_actual']} V {last['in_I_actual']} A, eff {last['eff_bench']}, "
          f"photo-pair eff {last['eff_photo_pair']}, used {last['load_n']}/{last['load_n_ok']}")
else:
    print("no readings yet")
