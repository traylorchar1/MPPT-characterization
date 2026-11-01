#!/usr/bin/env -S python3 -W ignore
"""Live dashboard for sweep.py. Serves http://localhost:8765

  dashboard.py [port]

Reads what sweep.py publishes (live.json + the run's CSV) and the latest camera frame.
It never opens the instruments itself, so it can't interfere with a sweep.
"""
import csv
import json
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIVE = HERE / "live.json"
FRAME = Path.home() / "loadcam" / "frame.jpg"


def num(v):
    if v in ("", None):
        return None
    try:
        return float(v)
    except ValueError:
        return v


def run_rows():
    try:
        live = json.loads(LIVE.read_text())
        path = Path(live.get("csv") or "")
    except (OSError, ValueError):
        path = Path()
    if not path.is_file():
        runs = sorted(HERE.glob("sweep_*.csv"))
        if not runs:
            return {"csv": None, "rows": []}
        path = runs[-1]
    with path.open() as f:
        rows = [{k: num(v) for k, v in r.items() if k != "reply"} for r in csv.DictReader(f)]
    return {"csv": path.name, "rows": rows}


class Handler(SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, body, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        route = self.path.split("?")[0]
        try:
            if route in ("/", "/index.html"):
                self.send((HERE / "dashboard.html").read_bytes(), "text/html; charset=utf-8")
            elif route == "/compare":
                self.send((HERE / "compare.html").read_bytes(), "text/html; charset=utf-8")
            elif route == "/compare.json":
                with (HERE / "results" / "photon_vs_tpee_comparison.csv").open() as f:
                    rows = [{k: num(v) for k, v in r.items()} for r in csv.DictReader(f)]
                self.send(json.dumps(rows).encode(), "application/json")
            elif route == "/tpee.json":
                with (HERE / "results" / "tpee_sec-b175-7a_summary.csv").open() as f:
                    rows = [{k: num(v) for k, v in r.items()} for r in csv.DictReader(f)]
                self.send(json.dumps(rows).encode(), "application/json")
            elif route == "/live.json":
                self.send(LIVE.read_bytes() if LIVE.exists() else b"{}", "application/json")
            elif route == "/data.json":
                self.send(json.dumps(run_rows()).encode(), "application/json")
            elif route == "/frame.jpg":
                self.send(FRAME.read_bytes(), "image/jpeg")
            else:
                self.send_error(404)
        except (OSError, BrokenPipeError):
            self.send_error(503)


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
    print(f"dashboard at http://localhost:{port}")
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
