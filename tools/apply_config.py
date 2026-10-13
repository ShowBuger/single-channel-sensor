#!/usr/bin/env python3
"""
apply_config.py — Send a sensor_config.json to the STM32 firmware over UART.

Usage:
    python apply_config.py COM3 sensor_config.json
    python apply_config.py /dev/ttyUSB0 sensor_config.json [--baud 115200]
"""
import json
import sys
import time
import argparse


def apply_config(port: str, cfg_path: str, baud: int = 115200) -> None:
    try:
        import serial
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial")
        sys.exit(1)

    with open(cfg_path, "r") as f:
        cfg = json.load(f)

    commands = []

    s = cfg["sensor"]
    commands += [
        f"ML SET sensor.gain {s['gain_sel']}",
        f"ML SET sensor.osr {s['osr']}",
        f"ML SET sensor.filt {s['dig_filt']}",
        f"ML SET sensor.resx {s['res_x']}",
        f"ML SET sensor.resy {s['res_y']}",
        f"ML SET sensor.resz {s['res_z']}",
        f"ML SET sensor.tcmp {s['tcmp_en']}",
    ]

    a = cfg["algorithm"]
    flag = a["flag_res"]
    if isinstance(flag, str):
        flag = int(flag, 16)
    commands += [
        f"ML SET algo.flag {flag}",
        f"ML SET algo.xmap {a['x_map']:.4f}",
        f"ML SET algo.ymap {a['y_map']:.4f}",
        f"ML SET algo.zmap {a['z_map']:.4f}",
        f"ML SET algo.dead {a['dead_value']:.4f}",
        f"ML SET algo.spike {a['spike_multiplier']:.4f}",
        f"ML SET algo.stab {a['stability']:.4f}",
        "ML SAVECFG",
        "ML APPLYCFG",
    ]

    with serial.Serial(port, baud, timeout=1) as ser:
        time.sleep(0.1)
        for cmd in commands:
            line = (cmd + "\r\n").encode()
            ser.write(line)
            time.sleep(0.05)
            resp = ser.readline().decode("utf-8", errors="replace").strip()
            status = "OK" if resp and "FAIL" not in resp.upper() else "!!"
            print(f"  [{status}] {cmd:<40s} -> {resp}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Apply sensor_config.json to firmware")
    parser.add_argument("port", help="Serial port, e.g. COM3 or /dev/ttyUSB0")
    parser.add_argument("config", help="Path to sensor_config.json")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default 115200)")
    args = parser.parse_args()

    print(f"Applying {args.config} -> {args.port} @ {args.baud}")
    apply_config(args.port, args.config, args.baud)
    print("Done.")


if __name__ == "__main__":
    main()
