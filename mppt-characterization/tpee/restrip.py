#!/usr/bin/env -S python3 -W ignore
"""Rebuild a manual-read point's strip from its saved photos with different crops (if the camera
moved during a run). Writes readq/NNNN_r.png.

  restrip.py RUN_DIR K [LOAD_CROP XPF_CROP]
"""
import json
import sys
from pathlib import Path

sys.argv, argv = sys.argv[:1], sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import sweep  # noqa: E402

run, k = Path(argv[0]), int(argv[1])
if len(argv) > 3:
    sweep.LOAD_CROP, sweep.XPF_CROP = argv[2], argv[3]
point = json.loads((run / "readq" / f"{k:04d}.json").read_text())
out = run / "readq" / f"{k:04d}_r.png"
sweep.make_strip([run / ph["file"] for ph in point["photos"]], out)
print(out)
