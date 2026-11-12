#!/usr/bin/env python3
"""Hardware-in-the-loop serial test tool for the single-channel sensor firmware.

Talks to the board over its serial port (ST-Link VCP by default), records the
raw byte stream with host timestamps, and runs an automated test suite.

  python firmware/tools/hwtest.py ports
  python firmware/tools/hwtest.py monitor  [--seconds 10] [--raw out.bin]
  python firmware/tools/hwtest.py cmd "ML GETCFG" [--expect CFG_END] [--timeout 3]
  python firmware/tools/hwtest.py suite    [--seconds 10] [--expect-rate 10] [--log-impact]

Common options: --port COMx (default: auto-detect STLink/STMicro VCP), --baud 921600.

The suite never changes persistent settings unless --log-impact is given
(ML LOGLEVEL <n> writes Flash; the original level is restored afterwards).
It never sends ML RESET / ML RESTORE / ML SET / ML SAVECFG / ML IMPORT_*.

Exit code: 0 = all passed, 1 = a test failed, 2 = could not talk to the device.
Requires: pip install pyserial
"""
import argparse
import json
import math
import re
import statistics
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    sys.exit("pyserial is required: python -m pip install pyserial")

ROOT = Path(__file__).resolve().parents[2]
REPORT_DIR = ROOT / "tests" / "hw_reports"
NUM = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
SAMPLE_RE = re.compile(rf"^sensor(\d+)\s*:\s*({NUM})\s*,\s*({NUM})\s*,\s*({NUM})\s*$")
LOG_PREFIX_RE = re.compile(r"^\[[EWID]\]\[[^\]]+\]\s*")


# --------------------------------------------------------------------------- port
def find_port(explicit=None):
    if explicit:
        return explicit
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        text = f"{p.description} {p.manufacturer or ''}".lower()
        if "stlink" in text or "st-link" in text or "stmicro" in text:
            return p.device
    if len(ports) == 1:
        return ports[0].device
    raise SystemExit("cannot auto-detect the board port; use --port. Available: "
                     + ", ".join(f"{p.device} ({p.description})" for p in ports))


# ------------------------------------------------------------------------- device
class Device:
    """Background reader: keeps raw chunks and complete lines with host timestamps."""

    def __init__(self, port, baud, settle=0.4):
        self.ser = serial.Serial(port, baud, timeout=0.05)
        # ST-Link VCP flushes a stale, partly dropped buffer for ~100-300 ms after the
        # port opens (measured: truncated heads and spliced lines). Discard that window
        # before talking to the device, otherwise the first reply can be glued to garbage.
        self.settle_bytes = 0
        deadline = time.perf_counter() + settle
        while time.perf_counter() < deadline:
            self.settle_bytes += len(self.ser.read(4096))
        self.ser.reset_input_buffer()
        self.raw = bytearray()          # every byte received
        self.chunks = []                # (t, offset, len)
        self.lines = []                 # (t, text)  text without CR/LF
        self._buf = bytearray()
        self._lock = threading.Lock()
        self._cv = threading.Condition(self._lock)
        self._alive = True
        self.t0 = time.perf_counter()
        self._thr = threading.Thread(target=self._reader, daemon=True)
        self._thr.start()

    def now(self):
        return time.perf_counter() - self.t0

    def _reader(self):
        while self._alive:
            try:
                data = self.ser.read(4096)
            except serial.SerialException:
                break
            if not data:
                continue
            t = self.now()
            with self._cv:
                self.chunks.append((t, len(self.raw), len(data)))
                self.raw += data
                for b in data:
                    if b in (10, 13):
                        if self._buf:
                            self.lines.append((t, self._buf.decode("utf-8", "replace")))
                            self._buf.clear()
                    else:
                        self._buf.append(b)
                self._cv.notify_all()

    def close(self):
        self._alive = False
        self._thr.join(timeout=1)
        self.ser.close()

    def send(self, cmd):
        self.ser.write((cmd + "\n").encode())
        self.ser.flush()

    def wait_line(self, pattern, start_index, timeout):
        """Return (index, t, text) of the first line at/after start_index matching pattern."""
        rx = re.compile(pattern)
        deadline = time.perf_counter() + timeout
        i = start_index
        with self._cv:
            while True:
                while i < len(self.lines):
                    t, text = self.lines[i]
                    if rx.search(LOG_PREFIX_RE.sub("", text)):
                        return i, t, text
                    i += 1
                left = deadline - time.perf_counter()
                if left <= 0:
                    return None
                self._cv.wait(left)

    def request(self, cmd, pattern, timeout=3.0):
        """Send cmd and wait for a reply line. Result includes latency and integrity info."""
        with self._lock:
            start, raw_start = len(self.lines), len(self.raw)
        t_send = self.now()
        self.send(cmd)
        hit = self.wait_line(pattern, start, timeout)
        res = {"cmd": cmd, "ok": hit is not None}
        if hit:
            idx, t, text = hit
            clean = LOG_PREFIX_RE.sub("", text)
            res.update(index=idx, latency_ms=round((t - t_send) * 1000, 2), line=text,
                       exact=bool(re.fullmatch(pattern, clean)))
        else:
            # Was the reply received but glued to another line? Look at the raw bytes.
            token = re.sub(r"[\^\$\\]", "", pattern.split("|")[0])[:24]
            with self._lock:
                tail = bytes(self.raw[raw_start:])
            pos = tail.find(token.encode()) if token else -1
            res.update(latency_ms=None, line=None, exact=False,
                       raw_found=pos >= 0,
                       raw_context=tail[max(0, pos - 48):pos + 48].hex(" ") if pos >= 0 else None)
        return res

    def lines_between(self, t_from, t_to):
        with self._lock:
            return [(t, s) for t, s in self.lines if t_from <= t <= t_to]


def samples_from(lines):
    out = []
    for t, s in lines:
        m = SAMPLE_RE.match(s.strip())
        if m:
            out.append((t, int(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))))
    return out


# -------------------------------------------------------------------------- stats
def interval_stats(ts):
    if len(ts) < 3:
        return None
    d = [b - a for a, b in zip(ts, ts[1:])]
    d_sorted = sorted(d)
    med = statistics.median(d)
    return {
        "count": len(ts),
        "rate_hz": round((len(ts) - 1) / (ts[-1] - ts[0]), 3),
        "median_ms": round(med * 1000, 2),
        "p95_ms": round(d_sorted[int(0.95 * (len(d) - 1))] * 1000, 2),
        "max_ms": round(max(d) * 1000, 2),
        "std_ms": round(statistics.pstdev(d) * 1000, 2),
        "gaps_over_2x": sum(1 for x in d if x > 2.0 * med),
    }


def axis_stats(vals):
    uniq = sorted(set(vals))
    steps = [round(b - a, 6) for a, b in zip(uniq, uniq[1:]) if b - a > 1e-9]
    return {
        "mean": round(statistics.fmean(vals), 4),
        "std": round(statistics.pstdev(vals), 4),
        "min": min(vals), "max": max(vals),
        "distinct": len(uniq),
        "min_step": min(steps) if steps else None,
    }


# -------------------------------------------------------------------------- suite
class Suite:
    def __init__(self, dev, args):
        self.d, self.a = dev, args
        self.results, self.info = [], {}

    def record(self, name, status, **data):
        self.results.append({"test": name, "status": status, **data})
        mark = {"PASS": "+", "FAIL": "x", "SKIP": "-", "WARN": "!"}[status]
        detail = "  ".join(f"{k}={v}" for k, v in data.items() if not isinstance(v, (dict, list)))
        print(f"  [{mark}] {name:<28} {status:<4} {detail}")

    def run(self):
        steps = [self.t_sync, self.t_loglevel, self.t_getcfg, self.t_stop, self.t_stream,
                 self.t_cmd_under_stream]
        if self.a.log_impact:
            steps.append(self.t_log_impact)
        for step in steps:
            try:
                step()
            except Exception as e:  # keep going, report the crash as a failure
                self.record(step.__name__[2:], "FAIL", error=repr(e))
        self.finish()

    # T1 -----------------------------------------------------------------------
    def t_sync(self):
        r = self.d.request("ML IMPORT_CANCEL", r"^(IMPORT_CANCELLED|Unknown.*)$", 5)
        if r["ok"] and r["exact"]:
            self.record("sync_import_cancel", "PASS", latency_ms=r["latency_ms"])
        elif r["ok"]:
            self.record("sync_import_cancel", "FAIL", reason="reply not on its own line", line=r["line"])
        else:
            self.record("sync_import_cancel", "FAIL", reason="no reply line",
                        raw_found=r.get("raw_found"), raw_context=r.get("raw_context"))

    # T2 -----------------------------------------------------------------------
    def t_loglevel(self):
        r = self.d.request("ML LOGLEVEL", r"^LogLevel=([0-4]).*$", 3)
        if r["ok"]:
            self.info["log_level"] = int(re.search(r"LogLevel=(\d)", r["line"]).group(1))
            self.record("loglevel_query", "PASS", level=self.info["log_level"], latency_ms=r["latency_ms"])
        else:
            self.record("loglevel_query", "FAIL", reason="no LogLevel reply")

    # T3 -----------------------------------------------------------------------
    def t_getcfg(self):
        b = self.d.request("ML GETCFG", r"^CFG_BEGIN$", 3)
        if not b["ok"]:
            self.record("getcfg", "FAIL", reason="no CFG_BEGIN")
            return
        e = self.d.wait_line(r"^CFG_END$", b["index"], 3)
        if not e:
            self.record("getcfg", "FAIL", reason="no CFG_END")
            return
        body = [s for _, s in self.d.lines[b["index"] + 1:e[0]] if s.strip().startswith("{")]
        try:
            cfg = json.loads(body[-1])
        except Exception as ex:
            self.record("getcfg", "FAIL", reason=f"bad JSON: {ex}")
            return
        self.info["config"] = cfg
        self.record("getcfg", "PASS", product=cfg.get("product"), flag=cfg.get("algo", {}).get("flag"))

    # T4 -----------------------------------------------------------------------
    def t_stop(self):
        r = self.d.request("ML STOP", r"^(Stopped|Stop failed)$", 3)
        if not r["ok"] or r["line"].endswith("failed"):
            self.record("stop", "FAIL", line=r.get("line"))
            return
        time.sleep(0.3)                               # let in-flight lines drain
        t1 = self.d.now()
        time.sleep(1.0)
        n = len(samples_from(self.d.lines_between(t1, self.d.now())))
        self.record("stop", "PASS" if n == 0 else "FAIL", samples_after_stop=n)

    # T5 -----------------------------------------------------------------------
    def t_stream(self):
        r = self.d.request("ML START", r"^(Sensor started|Failed to start)$", 5)
        if not r["ok"] or r["line"].startswith("Failed"):
            self.record("start", "FAIL", line=r.get("line"))
            return
        self.record("start", "PASS", latency_ms=r["latency_ms"])
        time.sleep(0.5)                               # skip warm-up samples
        t1 = self.d.now()
        time.sleep(self.a.seconds)
        t2 = self.d.now()
        lines = self.d.lines_between(t1, t2)
        smp = [s for s in samples_from(lines) if s[1] == 0]
        st = interval_stats([s[0] for s in smp])
        if not st:
            self.record("sample_rate", "FAIL", reason="fewer than 3 samples", samples=len(smp))
            return
        self.info["stream"] = st
        self.info["lines_per_sample"] = round(len(lines) / max(1, len(smp)), 1)
        self.info["bytes_per_s"] = round(sum(len(s) + 2 for _, s in lines) / (t2 - t1))
        exp = self.a.expect_rate
        ok = abs(st["rate_hz"] - exp) <= 0.2 * exp
        self.record("sample_rate", "PASS" if ok else "FAIL", expected_hz=exp, **{k: st[k] for k in
                    ("rate_hz", "median_ms", "p95_ms", "max_ms", "std_ms", "gaps_over_2x")})
        self.record("serial_load", "PASS", lines_per_sample=self.info["lines_per_sample"],
                    bytes_per_s=self.info["bytes_per_s"],
                    link_use_pct=round(self.info["bytes_per_s"] * 10 / self.a.baud * 100, 1))
        axes = {n: axis_stats([s[i] for s in smp]) for n, i in (("x", 2), ("y", 3), ("z", 4))}
        self.info["noise"] = axes
        flag = self.info.get("config", {}).get("algo", {}).get("flag", "?")
        all_flat = all(v["std"] == 0 for v in axes.values())
        self.record("noise_baseline", "WARN" if all_flat or flag not in ("0x00", "?") else "PASS",
                    flag=flag, note=("output is post-algorithm; raw noise needs flag 0x00"
                                     if flag not in ("0x00", "?") else ""),
                    **{f"{n}_std": v["std"] for n, v in axes.items()},
                    **{f"{n}_step": v["min_step"] for n, v in axes.items()})
        bad = [s for _, s in lines if "�" in s]
        self.record("line_integrity", "PASS" if not bad else "WARN", undecodable_lines=len(bad))

    # T6 -----------------------------------------------------------------------
    def t_cmd_under_stream(self):
        lat, glued, missing = [], [], 0
        for i in range(self.a.probes):
            time.sleep(0.05 + (i * 0.037) % 0.1)      # vary the phase against the sample loop
            r = self.d.request("ML LOGLEVEL", r"^LogLevel=[0-4].*$", 3)
            if not r["ok"]:
                missing += 1
                if r.get("raw_found"):
                    glued.append(r.get("raw_context"))
            else:
                lat.append(r["latency_ms"])
                if not r["exact"]:
                    glued.append(r["line"])
        data = {"probes": self.a.probes, "missing": missing, "glued": len(glued)}
        if lat:
            data.update(lat_median_ms=round(statistics.median(lat), 1), lat_max_ms=round(max(lat), 1))
        status = "PASS" if missing == 0 and not glued else "FAIL"
        self.record("cmd_under_stream", status, **data)
        if glued:
            self.info["glued_examples"] = glued[:5]

    # T7 (optional, writes Flash) ---------------------------------------------------
    def t_log_impact(self):
        orig = self.info.get("log_level")
        if orig is None:
            self.record("log_impact", "SKIP", reason="unknown original level")
            return
        rates = {}
        try:
            for lvl in sorted({orig, 2}):
                r = self.d.request(f"ML LOGLEVEL {lvl}", rf"^LogLevel={lvl}$", 6)
                if not r["ok"]:
                    self.record("log_impact", "FAIL", reason=f"cannot set level {lvl}")
                    return
                time.sleep(0.5)
                t1 = self.d.now()
                time.sleep(min(5, self.a.seconds))
                st = interval_stats([s[0] for s in samples_from(self.d.lines_between(t1, self.d.now()))])
                rates[lvl] = st and st["rate_hz"]
        finally:
            self.d.request(f"ML LOGLEVEL {orig}", rf"^LogLevel={orig}$", 6)
        self.record("log_impact", "PASS", **{f"rate_at_level_{k}": v for k, v in rates.items()},
                    restored_level=orig)

    # ------------------------------------------------------------------------
    def finish(self):
        if self.a.leave_running:
            r = self.d.request("ML START", r"^(Sensor started|Failed to start)$", 5)
        else:
            r = self.d.request("ML STOP", r"^(Stopped|Stop failed)$", 3)
        failed = [x for x in self.results if x["status"] == "FAIL"]
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        REPORT_DIR.mkdir(parents=True, exist_ok=True)
        report = {
            "time": datetime.now().isoformat(timespec="seconds"),
            "port": self.a.port, "baud": self.a.baud, "seconds": self.a.seconds,
            "summary": {"total": len(self.results), "failed": len(failed)},
            "results": self.results, "info": self.info,
            "stopped_at_end": bool(r["ok"]) and not self.a.leave_running,
        }
        path = REPORT_DIR / f"hwtest-{stamp}.json"
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        (REPORT_DIR / f"hwtest-{stamp}.raw").write_bytes(bytes(self.d.raw))
        print(f"\n  {len(self.results) - len(failed)}/{len(self.results)} passed   report: {path}")
        self.exit_code = 1 if failed else 0


# --------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("action", choices=["ports", "monitor", "cmd", "suite"])
    ap.add_argument("command", nargs="?", help="command text for 'cmd'")
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--seconds", type=float, default=10)
    ap.add_argument("--timeout", type=float, default=3)
    ap.add_argument("--expect", default=r".+", help="regex for 'cmd' reply")
    ap.add_argument("--expect-rate", type=float, default=10.0, help="expected sample rate (Hz)")
    ap.add_argument("--probes", type=int, default=20, help="commands sent during streaming")
    ap.add_argument("--log-impact", action="store_true", help="measure log level impact (writes Flash)")
    ap.add_argument("--leave-running", action="store_true", help="restart sampling at the end")
    ap.add_argument("--raw", help="monitor: save raw bytes to this file")
    a = ap.parse_args()

    if a.action == "ports":
        for p in serial.tools.list_ports.comports():
            print(f"{p.device:8} {p.description}")
        return 0

    a.port = find_port(a.port)
    try:
        dev = Device(a.port, a.baud)
    except serial.SerialException as e:
        print(f"cannot open {a.port}: {e}  (is the GUI or another terminal holding the port?)")
        return 2
    try:
        if a.action == "monitor":
            print(f"monitor {a.port} @ {a.baud} for {a.seconds}s")
            time.sleep(a.seconds)
            for t, s in dev.lines:
                print(f"{t:9.3f}  {s}")
            if a.raw:
                Path(a.raw).write_bytes(bytes(dev.raw))
            st = interval_stats([s[0] for s in samples_from(dev.lines)])
            print(f"\n{len(dev.lines)} lines, {len(dev.raw)} bytes, samples: {st}")
            return 0
        if a.action == "cmd":
            if not a.command:
                ap.error("cmd needs the command text")
            r = dev.request(a.command, a.expect, a.timeout)
            time.sleep(0.2)
            print(json.dumps(r, ensure_ascii=False, indent=2))
            return 0 if r["ok"] else 1
        print(f"hwtest suite on {a.port} @ {a.baud}  (discarded {dev.settle_bytes} bytes while settling)\n")
        s = Suite(dev, a)
        s.run()
        return s.exit_code
    finally:
        dev.close()


if __name__ == "__main__":
    sys.exit(main())
