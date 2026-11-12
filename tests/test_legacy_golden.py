"""Legacy algorithm golden samples (refactor P0).

Runs the *current* firmware `data_process()` inside Unicorn over deterministic
input vectors and either records the bit-exact outputs (`--record`) or checks
the build against the recorded file. The refactored `legacy` profile must later
reproduce exactly these outputs (docs/REFACTOR_DESIGN.md §17.4, §17.9).

Usage:
  cmake -B firmware/build_emu -S firmware -G Ninja -DCMAKE_BUILD_TYPE=Debug -DFW_EMU_SP_FPU=ON
  cmake --build firmware/build_emu
  set FIRMWARE_BUILD_DIR=firmware/build_emu
  python tests/test_legacy_golden.py            # check
  python tests/test_legacy_golden.py --record   # (re)record, only on the baseline commit

The pipeline depends on the build's sensor type through MEASURE_INTERVAL
(predictive compensation), so the golden file is per sensor type.
"""
import json
import math
import random
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_ak09974c_firmware as fw  # noqa: E402  (reuses the Unicorn firmware loader)

GOLDEN = Path(__file__).resolve().parent / "golden" / "legacy_pipeline_ak09974c.json"
N = 100                      # samples per scenario
FLAG_SETS = [0x00, 0x01, 0x09, 0x0B, 0x0F, 0x1F, 0x3F, 0x7F, 0x45]
#  0x01 calib | 0x09 +predictive | 0x0B +deadband | 0x0F +sliding | 0x1F +mapping
#  0x3F +smooth | 0x7F +z filter | 0x45 calib+sliding+z filter

PAIR_SIZE = 36               # SensorPair_Data: actual(16) reference(16) valid(1)+pad
CAL = {                      # SensorPair_Calibration contents used for every run
    "actual_offset": (-48.0, -40.0, 48.0, 0.0),
    "reference_offset": (-47.0, -32.0, -24.0, 0.0),
    "actual_std": (1.2, 1.1, 1.5, 0.0),
    "reference_std": (1.0, 1.3, 1.1, 0.0),
}
ALGO = (8.5, 20.0, 0.1)      # g_algo: dead_value, spike_multiplier, stability_threshold
MAPS = (1.0, 1.0, 1.0)       # X/Y/Z_MAPPING_VALUE (overridden per run below)


def f32(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def scenarios():
    """Deterministic input vectors: list of (actual xyzt, reference xyzt, valid)."""
    out = {}

    def gen(name, fn, seed):
        rnd = random.Random(seed)
        rows = []
        for i in range(N):
            a, r, valid = fn(i, rnd)
            rows.append(([f32(v) for v in a], [f32(v) for v in r], valid))
        out[name] = rows

    base_a, base_r = CAL["actual_offset"][:3], CAL["reference_offset"][:3]

    def noisy(base, sig, rnd):
        return [b + rnd.gauss(0, s) for b, s in zip(base, sig)]

    gen("static_noise", lambda i, r: (noisy(base_a, (1.1, 1.1, 1.1), r) + [25.0],
                                      noisy(base_r, (1.1, 1.1, 1.1), r) + [25.0], 1), 1)
    gen("step", lambda i, r: (noisy([b + (30.0 if i >= 40 else 0.0) for b in base_a], (1, 1, 1), r) + [25.0],
                              noisy(base_r, (1, 1, 1), r) + [25.0], 1), 2)
    gen("ramp", lambda i, r: (noisy([b + 0.4 * i for b in base_a], (1, 1, 1), r) + [25.0],
                              noisy(base_r, (1, 1, 1), r) + [25.0], 1), 3)
    gen("spike", lambda i, r: (noisy([b + (80.0 if i in (30, 31, 60) else 0.0) for b in base_a], (1, 1, 1), r) + [25.0],
                               noisy(base_r, (1, 1, 1), r) + [25.0], 1), 4)
    gen("common_mode", lambda i, r: (noisy([b + 15 * math.sin(i / 8) for b in base_a], (1, 1, 1), r) + [25.0],
                                     noisy([b + 15 * math.sin(i / 8) for b in base_r], (1, 1, 1), r) + [25.0], 1), 5)
    gen("z_sign", lambda i, r: (noisy([base_a[0], base_a[1], base_a[2] + 60 * math.sin(i / 10)], (1, 1, 1), r) + [25.0],
                                noisy(base_r, (1, 1, 1), r) + [25.0], 1), 6)
    gen("invalid", lambda i, r: (noisy(base_a, (1, 1, 1), r) + [25.0],
                                 noisy(base_r, (1, 1, 1), r) + [25.0], 0 if i % 17 == 5 else 1), 7)
    return out


def bits(v):
    return struct.unpack("<I", struct.pack("<f", v))[0]


def run(rows, flags, calibrated):
    f = fw.Firmware()
    u, s = f.u, f.sym
    u.mem_write(s["FLAG_RES"], bytes([flags]))
    u.mem_write(s["g_algo"], struct.pack("<3f", *ALGO))
    for name, v in zip(("X_MAPPING_VALUE", "Y_MAPPING_VALUE", "Z_MAPPING_VALUE"), (1.5, 0.8, 2.0)):
        u.mem_write(s[name], struct.pack("<f", v))
    cal = b"".join(struct.pack("<4f", *CAL[k]) for k in
                   ("actual_offset", "reference_offset", "actual_std", "reference_std"))
    u.mem_write(s["sensor_calibration"], cal + bytes([calibrated]))
    out = []
    for a, r, valid in rows:
        u.mem_write(s["sensor_data"], struct.pack("<4f4fB", *a, *r, valid))
        f.call("data_process")
        p = bytes(u.mem_read(s["processed_sensor_data"], PAIR_SIZE))
        x, y, z = struct.unpack_from("<3f", p, 0)
        out.append([p[32], bits(x), bits(y), bits(z)])
    return out


def runs():
    for flags in FLAG_SETS:
        yield flags, 1
    yield 0x7F, 0     # not calibrated: most stages must be skipped


def record():
    sc = scenarios()
    data = {
        "meta": {
            "sensor_type": "AK09974C",
            "note": "outputs of legacy data_process(); float32 values stored as uint32 bit patterns",
            "calibration": CAL, "algo": ALGO, "maps": (1.5, 0.8, 2.0), "samples": N,
        },
        "inputs": {k: [[[bits(v) for v in a], [bits(v) for v in r], valid] for a, r, valid in rows]
                   for k, rows in sc.items()},
        "outputs": {},
    }
    for name, rows in sc.items():
        for flags, calibrated in runs():
            data["outputs"][f"{name}/0x{flags:02X}/cal{calibrated}"] = run(rows, flags, calibrated)
    GOLDEN.parent.mkdir(exist_ok=True)
    GOLDEN.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    print(f"recorded {len(data['outputs'])} runs -> {GOLDEN}")


def unbits(u32):
    return struct.unpack("<f", struct.pack("<I", u32))[0]


class LegacyGolden(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(GOLDEN.read_text(encoding="utf-8"))

    def test_bit_exact(self):
        d = self.data
        for key, expected in d["outputs"].items():
            name, flags, cal = key.split("/")
            rows = [([unbits(v) for v in a], [unbits(v) for v in r], valid) for a, r, valid in d["inputs"][name]]
            with self.subTest(run=key):
                got = run(rows, int(flags, 16), int(cal[3:]))
                for i, (g, e) in enumerate(zip(got, expected)):
                    self.assertEqual(g, e, f"{key} sample {i}: got {g} expected {e}")


if __name__ == "__main__":
    if "--record" in sys.argv:
        record()
    else:
        unittest.main(argv=[sys.argv[0], "-v"])
