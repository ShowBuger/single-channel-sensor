"""Run MA03 (AK09974C) regressions against the built ARM Debug binary using Unicorn.

Build a Debug image with CURRENT_SENSOR_TYPE = SENSOR_TYPE_AK09974C, install `unicorn`,
then run this file. Point FIRMWARE_BUILD_DIR at that build (default firmware/build).
Only the soft-I2C primitives, delays, tick, watchdog and logging are stubbed; the driver,
config layer and sensor manager run as compiled ARM code against a register-level chip model.

The self-test/threshold API test needs those functions linked in; --gc-sections drops them
when unused, so configure with
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--undefined=AK09974C_SelfTest -Wl,--undefined=AK09974C_SetThreshold
   -Wl,--undefined=AK09974C_SetSwitch -Wl,--undefined=AK09974C_SetLinear"
or that test is skipped.
"""
import json
import os
import struct
import subprocess
import sys
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import (UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,
                               UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("FIRMWARE_BUILD_DIR", ROOT / "firmware/build"))
ELF = BUILD / "single_channel_sensor.elf"


class FakeAK:
    """AK09974C register model following datasheet 250800026-E-00."""
    def __init__(self, bus, field=(0, 0, 0)):
        self.bus, self.field = bus, list(field)
        self.selftest_field = (3, -2, 50)
        self.vio = 0
        self.reset()
        self.writes, self.violations = [], []

    def reset(self):
        self.mset, self.mcyc, self.run, self.lset = 0x02, 0, 0, 0
        self.swset = [0, 0]
        self.thr = {r: [0, 0, 0, 0] for r in range(0x34, 0x38)}
        self.drdy, self.err, self.data = 0, 0, (0, 0, 0)
        self.single_due = self.next_cycle = None

    def tick(self):
        now = self.bus.now
        if self.single_due is not None and now >= self.single_due:
            self.single_due, self.run, self.drdy = None, 0, 1
            self.data = self.selftest_field if self.mset & 0x08 else tuple(self.field)
        while self.next_cycle is not None and now >= self.next_cycle:
            self.drdy, self.data = 1, tuple(self.field)
            self.next_cycle += self.mcyc * 0.5

    def write(self, reg, data):
        self.tick()
        self.writes.append((reg, list(data)))
        if reg != 0x20 and not self.vio:
            self.violations.append(f"write {reg:#x} before VIO")
        if reg == 0x20:
            self.vio = data[0] if data[0] in (1, 2) else 0
        elif reg == 0x30:
            if data[0] & 1:
                self.reset()
        elif 0x31 <= reg <= 0x3B and self.run and self.mcyc:
            self.violations.append(f"write {reg:#x} while running")
        elif reg in self.thr:
            self.thr[reg] = list(data)
        elif reg == 0x38:
            self.swset = list(data)
        elif reg == 0x39:
            self.lset = data[0]
        elif reg == 0x3A:
            self.mset = data[0]
        elif reg == 0x3B:
            self.mcyc = data[0]
        if reg == 0x3C:
            if len(data) != 1:
                self.violations.append("RUN not written individually")
            self.run = data[0] & 1
            if self.run and self.mcyc == 0:
                self.single_due = self.bus.now + 1
            elif self.run:
                self.next_cycle = self.bus.now + self.mcyc * 0.5
            else:
                self.next_cycle = self.single_due = None
        if 0x31 <= reg <= 0x3C:
            self.drdy = 0

    def st(self):
        return 0x80 | self.drdy | (0x20 if self.err else 0)

    def stream(self, reg):
        self.tick()
        if reg != 0x20 and not self.vio:
            self.violations.append(f"read {reg:#x} before VIO")
        be = lambda v: [(v >> 8) & 0xFF, v & 0xFF]
        if reg == 0x00:
            return [0x48, 0xC2, 0, 0]
        if reg == 0x10:
            return [self.st()]
        if reg == 0x11:
            return [self.st()] + be(self.data[0]) + be(self.data[1]) + be(self.data[2])
        if reg in self.thr:
            return self.thr[reg]
        return {0x38: self.swset, 0x39: [self.lset], 0x3A: [self.mset], 0x3B: [self.mcyc],
                0x3C: [self.run], 0x20: [self.vio]}.get(reg, [0] * 9)

    def read_done(self, reg, n):
        if (reg == 0x11 and n >= 7) or 0x31 <= reg <= 0x3C:
            self.drdy = 0


class Bus:
    def __init__(self):
        self.now = 0.0
        self.devs = {}
        self.dev = None
        self.phase = None

    def start(self):
        if self.phase == "data" and self.wbuf:
            self.dev.write(self.ptr, self.wbuf)
        self.phase, self.wbuf = "addr", []

    def write_byte(self, b):
        if self.phase == "addr":
            self.dev = self.devs.get(b >> 1)
            self.rw = b & 1
            self.phase, self.nread = ("read", 0) if self.rw else ("reg", 0)
        elif self.phase == "reg":
            self.ptr, self.phase, self.wbuf = b, "data", []
        else:
            self.wbuf.append(b)

    def ack(self):
        return 0 if self.dev else 1

    def read_byte(self):
        s = self.dev.stream(self.ptr)
        v = s[self.nread] if self.nread < len(s) else 0
        self.nread += 1
        return v

    def stop(self):
        if self.dev and self.phase == "data" and self.wbuf:
            self.dev.write(self.ptr, self.wbuf)
        if self.dev and self.phase == "read":
            self.dev.read_done(self.ptr, self.nread)
        self.phase = None


class Firmware:
    def __init__(self):
        out = subprocess.check_output(["arm-none-eabi-nm", "--defined-only", str(ELF)], text=True)
        self.sym = {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
        binary = (BUILD / "single_channel_sensor.bin").read_bytes()
        u = self.u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        u.mem_map(0x08000000, 0x40000)
        u.mem_write(0x08000000, binary)
        u.mem_map(0x20000000, 0x10000)
        u.mem_map(0x40000000, 0x30000)
        u.mem_map(0x48000000, 0x2000)
        u.mem_map(0xE0000000, 0x100000)
        s = self.sym
        u.mem_write(s['_sdata'], binary[s['_sidata'] - 0x08000000:s['_sidata'] - 0x08000000 + s['_edata'] - s['_sdata']])
        self.stubs, self.output = {}, []
        self.bus = Bus()
        u.hook_add(UC_HOOK_CODE, self._hook)
        b = self.bus
        self.stub('Log_Print', lambda: 0)
        self.stub('IWDG_Feed', lambda: 0)
        self.stub('Serial_SendString', lambda: self.output.append(self.string(self.reg(0))))
        self.stub('Delay_ms', lambda: setattr(b, 'now', b.now + self.reg(0)))
        self.stub('Delay_us', lambda: setattr(b, 'now', b.now + self.reg(0) / 1000))
        self.stub('HAL_Delay', lambda: setattr(b, 'now', b.now + self.reg(0)))
        self.stub('HAL_GetTick', lambda: int(b.now))
        self.stub('IIC_Start', lambda: b.start())
        self.stub('IIC_Stop', lambda: b.stop())
        self.stub('I2C_WriteByte', lambda: b.write_byte(self.reg(1)))
        self.stub('I2C_WaitAck', lambda: b.ack())
        self.stub('I2C_ReadByte', lambda: b.read_byte())
        self.stub('I2C_SendAck', lambda: 0)
        self.stub('I2C_DevicePresent', lambda: 0 if self.reg(1) in b.devs else 1)

    def _hook(self, u, address, size, data):
        cb = self.stubs.get(address)
        if cb:
            u.reg_write(UC_ARM_REG_R0, cb() or 0)
            u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))

    def reg(self, i):
        return self.u.reg_read([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][i])

    def stub(self, name, cb):
        self.stubs[self.sym[name]] = cb

    def string(self, addr):
        out = bytearray()
        while (c := self.u.mem_read(addr + len(out), 1)[0]):
            out.append(c)
        return out.decode()

    def call(self, name, *args):
        u, stop = self.u, 0x0803FFF0
        u.reg_write(UC_ARM_REG_SP, 0x2000F000)
        u.reg_write(UC_ARM_REG_LR, stop | 1)
        for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args):
            u.reg_write(r, v)
        u.emu_start(self.sym[name] | 1, stop, count=8_000_000)
        assert u.reg_read(UC_ARM_REG_PC) == stop, f"{name} did not return"
        return u.reg_read(UC_ARM_REG_R0) & 0xFF

    def parse(self, value):
        self.u.mem_write(0x20008000, json.dumps(value, separators=(',', ':')).encode() + b'\0')
        return self.call('JsonConfig_Parse', 0x20008000, self.sym['g_config'], 0x20009000)

    def config(self):
        return bytes(self.u.mem_read(self.sym['g_config'], 56))

    def floats(self, addr, n):
        return struct.unpack(f'<{n}f', bytes(self.u.mem_read(addr, 4 * n)))

    def command(self, text):
        self.u.mem_write(0x2000A000, text.encode() + b'\0')
        self.stub('Serial_GetCommandFlag', lambda: 1)
        self.stub('Serial_GetCommandBuffer', lambda: 0x2000A000)
        self.output.clear()
        self.call('CommandHandler_Process')
        return ''.join(self.output)


class AK09974CTests(unittest.TestCase):
    def setUp(self):
        f = self.f = Firmware()
        if 'actual_sensors' not in f.sym or 'AK09974C_Init' not in f.sym:
            self.skipTest('build is not configured for SENSOR_TYPE_AK09974C')
        f.call('Config_SetDefaults', f.sym['g_config'])
        self.actual = FakeAK(f.bus, field=(1000, -2000, 300))
        self.ref = FakeAK(f.bus, field=(10, 20, -30))
        f.bus.devs = {0x21: self.actual, 0x22: self.ref}

    def scan(self):
        self.assertEqual(self.f.call('Scan_Ports'), 1)

    def sample(self):
        f = self.f
        f.u.mem_write(f.sym['SENSOR_Run'], b'\x01')
        self.assertEqual(f.call('get_sensor_data'), 0)
        return f.floats(f.sym['sensor_data'], 8)

    def test_defaults_layout_and_validation(self):
        cfg = self.f.config()
        self.assertEqual(cfg[40:45], b'MA03\0')
        self.assertEqual(cfg[1], 2)                    # sensor_type AK09974C
        self.assertEqual((cfg[3], cfg[4]), (0, 1))     # gain unused, drive=low noise
        self.assertEqual((cfg[10], cfg[11]), (0, 1))   # normal range, continuous
        self.assertEqual(cfg[48], 10)                  # mcyc in former pad byte
        self.assertEqual(self.f.call('Config_Validate', self.f.sym['g_config']), 0)

    def test_repo_template_is_valid(self):
        template = json.loads((ROOT / "firmware/config/config_ma03.json").read_text(encoding="utf-8"))
        self.assertEqual(self.f.parse(template), 0)
        self.assertEqual(self.f.call('Config_Validate', self.f.sym['g_config']), 0)
        self.assertEqual(self.f.config()[48], template['sensor']['mcyc'])

    def test_validation_rules(self):
        f = self.f
        cases = [({'product': 'MA03', 'sensor': {'drive': 0, 'mcyc': 16}}, 43),   # SLN needs >= 0x11
                 ({'product': 'MA03', 'sensor': {'drive': 0, 'mcyc': 17}}, 0),
                 ({'product': 'MA03', 'sensor': {'opmode': 0, 'mcyc': 0}}, 0),    # single mode ignores mcyc
                 ({'product': 'MA03', 'sensor': {'osr': 4}}, 41),
                 ({'product': 'MA03', 'sensor': {'gain': 1}}, 40),
                 ({'product': 'MA03', 'sensor': {'opmode': 2}}, 42),
                 ({'product': 'MA03', 'sensor': {'type': 'AK09974C'}}, 0),
                 ({'product': 'MA03', 'sensor': {'type': 1}}, 2),
                 ({'product': 'MA02'}, 3)]                                         # not this build's sensor
        for cfg, code in cases:
            self.assertEqual(f.parse(cfg), 0, cfg)
            self.assertEqual(f.call('Config_Validate', f.sym['g_config']), code, cfg)

    def test_scan_init_order_and_continuous_apply(self):
        self.scan()
        for dev in (self.actual, self.ref):
            self.assertEqual(dev.violations, [])
            self.assertEqual(dev.writes[0], (0x20, [0x01]))   # VIO first
            self.assertEqual(dev.writes[1], (0x30, [0x01]))   # soft reset
            self.assertEqual(dev.writes[-1], (0x3C, [0x01]))  # RUN last, alone
            self.assertEqual((dev.mset & 0x07, dev.mcyc, dev.run), (0x01, 10, 1))
        self.assertEqual(self.f.u.mem_read(self.f.sym['actual_sensors'] + 1, 1)[0], 0x21)
        self.assertEqual(self.f.u.mem_read(self.f.sym['reference_sensors'] + 1, 1)[0], 0x22)

    def test_sample_conversion_normal_and_wide(self):
        self.scan()
        ax, ay, az, _, rx, ry, rz, _ = self.sample()
        for got, raw in zip((ax, ay, az, rx, ry, rz), (1000, -2000, 300, 10, 20, -30)):
            self.assertAlmostEqual(got, raw * 1.1, places=2)
        self.assertEqual(self.f.parse({'product': 'MA03', 'sensor': {'range': 1}}), 0)
        self.scan()
        self.assertTrue(self.actual.mset & 0x04)
        ax = self.sample()[0]
        self.assertAlmostEqual(ax, 1000 * 6.1, places=1)
        self.assertEqual(self.actual.violations + self.ref.violations, [])

    def test_continuous_waits_for_fresh_data(self):
        self.scan()
        self.sample()
        before = self.f.bus.now
        self.sample()
        self.assertGreaterEqual(self.f.bus.now - before, 4.0)   # next 5ms cycle, not stale data

    def test_single_mode_triggers_each_sample(self):
        self.assertEqual(self.f.parse({'product': 'MA03', 'sensor': {'opmode': 0}}), 0)
        self.scan()
        self.assertEqual((self.actual.mcyc, self.actual.run), (0, 0))
        for _ in range(3):
            n = sum(1 for w in self.actual.writes if w == (0x3C, [0x01]))
            self.sample()
            self.assertEqual(sum(1 for w in self.actual.writes if w == (0x3C, [0x01])), n + 1)
        self.assertEqual(self.actual.violations, [])

    def test_timeout_when_sensor_never_ready(self):
        self.scan()
        self.actual.next_cycle = None
        self.actual.drdy = 0
        f = self.f
        f.u.mem_write(f.sym['SENSOR_Run'], b'\x01')
        self.assertEqual(f.call('get_sensor_data'), 1)

    def test_getcfg_reports_mcyc(self):
        out = self.f.command('ML GETCFG')
        body = json.loads(out.split('CFG_BEGIN\r\n')[1].split('\r\nCFG_END')[0])
        self.assertEqual(body['product'], 'MA03')
        self.assertEqual(body['sensor']['type'], 2)
        self.assertEqual(body['sensor']['mcyc'], 10)

    def test_selftest_threshold_switch_linear_api(self):
        f = self.f
        if 'AK09974C_SelfTest' not in f.sym:
            self.skipTest('optional AK09974C APIs were removed by --gc-sections; see module docstring')
        self.assertEqual(f.parse({'product': 'MA03', 'sensor': {'opmode': 0}}), 0)
        self.scan()
        dev = f.sym['actual_sensors']
        self.assertEqual(f.call('AK09974C_SelfTest', dev, 0), 0)
        self.assertEqual(self.actual.mset & 0x08, 0)               # STEST restored
        self.actual.selftest_field = (3, -2, 10)
        self.assertEqual(f.call('AK09974C_SelfTest', dev, 0), 7)    # Z outside 25..75
        # threshold X: bop=0x1200, brp=0x0800
        self.assertEqual(f.call('AK09974C_SetThreshold', dev, 0, 0x1200, 0x0800), 0)
        self.assertEqual(self.actual.thr[0x34], [0x12, 0x00, 0x08, 0x00])
        self.assertEqual(f.call('AK09974C_SetThreshold', dev, 0, 0x0800, 0x1200), 5)  # bop must exceed brp
        self.assertEqual(f.call('AK09974C_SetSwitch', dev, 0x8F01), 0)
        self.assertEqual(self.actual.swset, [0x8B, 0x01])          # reserved bit 10 cleared
        self.assertEqual(f.call('AK09974C_SetLinear', dev, 0x4D), 0)
        self.assertEqual(self.actual.lset, 0x4D)
        self.assertEqual(self.actual.violations, [])

    def test_settings_rejected_while_running(self):
        self.scan()   # continuous mode running
        dev = self.f.sym['actual_sensors']
        self.assertEqual(self.f.call('AK09974C_SetRange', dev, 1), 4)
        self.assertEqual(self.f.call('AK09974C_SetMeasureCycle', dev, 20), 4)
        self.assertEqual(self.actual.violations, [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
