#!/usr/bin/env -S python3 -W ignore
"""Recompute efficiency for a sweep CSV from the per-photo snapshots saved in its photos_json
column (each photo's load reading paired with the XPF's actual V/I at that instant), using
sweep.combine_pairs. Writes <name>_reprocessed.csv.

  reprocess.py sweep_xxx.csv [--vload 115]
"""
import csv
import json
import sys
from pathlib import Path

sys.argv, argv = sys.argv[:1], sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import sweep  # noqa: E402

src = Path(argv[0])
vload = float(argv[argv.index("--vload") + 1]) if "--vload" in argv else 115.0
rows = list(csv.DictReader(src.open()))
if "photos_json" not in rows[0]:
    sys.exit("this CSV predates per-photo snapshots (no photos_json column)")
for r in rows:
    comb = sweep.combine_pairs(json.loads(r["photos_json"] or "[]"), vload, float(r["P_in_window_avg"]))
    r["load_V"], r["load_A"] = (round(comb["load_V"], 3), round(comb["load_A"], 4)) if comb else ("", "")
    r["P_out_load"] = round(comb["p_out"], 4) if comb else ""
    r["eff_photo_pair"] = round(comb["eff_photo"], 5) if comb and comb["eff_photo"] else ""
    r["eff_bench"] = round(comb["eff"], 5) if comb and comb["eff"] else ""
    r["load_n"] = comb["n"] if comb else 0
    r["eff_spread"] = round(comb["eff_spread"], 4) if comb else ""
out = src.with_name(src.stem + "_reprocessed.csv")
with out.open("w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0]))
    w.writeheader()
    w.writerows(rows)
print(f"{len(rows)} rows -> {out}")
