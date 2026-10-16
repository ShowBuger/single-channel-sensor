"""Run configuration regressions against the built ARM Debug binary using Unicorn.

Build with cmake --build firmware/build, install `unicorn`, then run this file.
Only peripheral operations are stubbed; parsing, validation, serialization and
Flash save/load logic execute the firmware's compiled instructions.
"""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("FIRMWARE_BUILD_DIR", ROOT / "firmware/build"))
sys.path.insert(0, str(BUILD / "regression-tools"))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import (
    UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0,
    UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3,
)


class Firmware:
    def __init__(self):
        self.symbols = {}
        output = subprocess.check_output(
            ["arm-none-eabi-nm", "--defined-only", str(BUILD / "single_channel_sensor.elf")], text=True)
        for line in output.splitlines():
            parts = line.split()
            if len(parts) == 3:
                self.symbols[parts[2]] = int(parts[0], 16)
        self.u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        self.u.mem_map(0x08000000, 0x10000)
        binary = (BUILD / "single_channel_sensor.bin").read_bytes()
        self.u.mem_write(0x08000000, binary)
        self.u.mem_map(0x20000000, 0x5000)
        self.u.mem_map(0x40003000, 0x1000)
        s = self.symbols
        data = binary[s['_sidata'] - 0x08000000:s['_sidata'] - 0x08000000 + s['_edata'] - s['_sdata']]
        self.u.mem_write(s['_sdata'], data)
        self.stubs = {}
        self.output = []
        self.u.hook_add(UC_HOOK_CODE, self._hook)
        self.stub('Log_Print', lambda: 0)
        self.stub('Serial_SendString', lambda: self.output.append(self.string(self.reg(0))))

    def _hook(self, u, address, size, data):
        if address in self.stubs:
            value = self.stubs[address]()
            u.reg_write(UC_ARM_REG_R0, value or 0)
            u.reg_write(UC_ARM_REG_PC, u.reg_read(UC_ARM_REG_LR))

    def reg(self, index):
        return self.u.reg_read([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3][index])

    def stub(self, name, callback):
        self.stubs[self.symbols[name]] = callback

    def string(self, address):
        out = bytearray()
        while True:
            byte = self.u.mem_read(address + len(out), 1)[0]
            if not byte:
                return out.decode()
            out.append(byte)

    def call(self, name, *args):
        u = self.u
        stop = 0x0800FFF0
        u.reg_write(UC_ARM_REG_SP, 0x20004F00)
        u.reg_write(UC_ARM_REG_LR, stop | 1)
        for reg, value in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args):
            u.reg_write(reg, value)
        u.emu_start(self.symbols[name] | 1, stop, count=4000000)
        assert u.reg_read(UC_ARM_REG_PC) == stop, f"{name} did not return"
        return u.reg_read(UC_ARM_REG_R0)

    def parse(self, value):
        text = json.dumps(value, separators=(',', ':')) if isinstance(value, dict) else value
        self.u.mem_write(0x20002000, text.encode() + b'\0')
        return self.call('JsonConfig_Parse', 0x20002000, self.symbols['g_config'], 0x20003000)

    def defaults(self, product):
        self.u.mem_write(0x20002000, product.encode() + b'\0')
        return self.call('Config_SetDefaultsForProduct', self.symbols['g_config'], 0x20002000)

    def config(self):
        return bytes(self.u.mem_read(self.symbols['g_config'], 56))

    def validate(self):
        return self.call('Config_Validate', self.symbols['g_config'])

    def command(self, text):
        self.u.mem_write(0x20003200, text.encode() + b'\0')
        self.stub('Serial_GetCommandFlag', lambda: 1)
        self.stub('Serial_GetCommandBuffer', lambda: 0x20003200)
        self.output.clear()
        self.call('serial_command_process')
        return ''.join(self.output)

    def fake_flash(self):
        self.stub('HAL_FLASH_Unlock', lambda: 0)
        self.stub('HAL_FLASH_Lock', lambda: 0)
        self.stub('HAL_FLASHEx_Erase', lambda: self.u.mem_write(0x0800FC00, b'\xff' * 1024))
        self.stub('HAL_FLASH_Program', lambda: self.u.mem_write(self.reg(1), struct.pack('<I', self.reg(2))))

    def fake_sensors(self, failed=False):
        for name in ['MLX90393_begin', 'MLX90393_configureAll', 'TMAG3001_Init',
                     'TMAG3001_SetMagGain', 'TMAG3001_SetConvAvg', 'TMAG3001_SetRange',
                     'TMAG3001_SetOperatingMode', 'TMAG3001_SetMagChannels']:
            self.stub(name, lambda: int(failed))


class TmagBus:
    """Peripheral model: delayed conversions, persistent ready bit and rolling count."""
    def __init__(self, f):
        self.f, self.now = f, 0
        self.regs, self.writes, self.feeds, self.transactions = {}, [], [], []
        self.state = {addr: dict(count=0, ready=0, due=None) for addr in (0x36, 0x35)}
        self.fail_reg = None
        self.never_ready = False
        self.tx = []
        f.stub('TMAG3001_ReadReg', self.read_reg)
        f.stub('TMAG3001_WriteReg', self.write_reg)
        f.stub('TMAG3001_ReadRegs', self.read_regs)
        f.stub('I2C_DevicePresent', lambda: 0)
        f.stub('HAL_GetTick', lambda: self.now)
        f.stub('Delay_ms', self.delay)
        f.stub('IWDG_Feed', lambda: self.feeds.append(self.now))
        f.stub('IIC_Start', lambda: self.tx.clear())
        f.stub('I2C_WriteByte', lambda: self.tx.append(f.reg(1)))
        f.stub('I2C_WaitAck', lambda: 0)
        f.stub('IIC_Stop', self.stop)

    def address(self):
        return self.f.u.mem_read(self.f.reg(0) + 1, 1)[0]

    def delay(self):
        self.now += self.f.reg(0)

    def status(self, addr):
        state = self.state[addr]
        if self.never_ready:
            return 0
        while state['due'] is not None and self.now >= state['due']:
            state['count'] = (state['count'] + 1) & 7
            state['ready'] = 1
            state['due'] = state['due'] + 5 if self.regs.get((addr, 1), 0) & 3 == 2 else None
        return state['count'] << 5 | state['ready']

    def read_reg(self):
        addr, reg = self.address(), self.f.reg(1)
        value = self.status(addr) if reg == 0x18 else self.regs.get((addr, reg), 0)
        self.f.u.mem_write(self.f.reg(2), bytes([value]))
        return 0

    def write_reg(self):
        addr, reg, value = self.address(), self.f.reg(1), self.f.reg(2)
        self.writes.append((addr, reg, value))
        if self.fail_reg == reg:
            return 1
        self.regs[addr, reg] = value
        if reg == 1:
            self.state[addr]['due'] = self.now + 5 if value & 3 == 2 else None
        return 0

    def read_regs(self):
        addr, reg, out, size = self.address(), self.f.reg(1), self.f.reg(2), self.f.reg(3)
        if reg == 0x0d:
            data = bytes([8, 0x49, 0x54])  # auto-detected A2
        else:
            data = struct.pack('>hhhB', 160, -320, 480, self.status(addr))
        self.f.u.mem_write(out, data[:size])
        return 0

    def stop(self):
        if len(self.tx) == 2 and self.tx[1] == 0x98:
            self.state[self.tx[0] >> 1]['due'] = self.now + 5
            self.transactions.append(tuple(self.tx))


class TmagTests(unittest.TestCase):
    def setUp(self):
        self.f = Firmware()
        self.f.call('Config_SetDefaults', self.f.symbols['g_config'])
        self.bus = TmagBus(self.f)

    def test_factory_defaults_and_preserved_flash_layout(self):
        cfg = self.f.config()
        self.assertEqual(cfg[1:5], bytes([1, 0, 5, 4]))
        self.assertEqual(cfg[11:15], bytes([2, 7, 0, 4]))
        self.assertEqual(cfg[40:45], b'MA02\0')
        self.assertEqual(self.f.validate(), 0)

    def test_numeric_and_product_validation(self):
        for bad in [{'sensor': {'gain': 256}}, {'sensor': {'gain': 1.5}},
                    {'algo': {'xmap': 1e100}}, {'product': 'MA03'}]:
            self.assertNotEqual(self.f.parse(bad), 0)
        for bad in [{'product': 'MA01', 'sensor': {'type': 1}},
                    {'product': 'MA01', 'sensor': {'type': 0}}]:
            self.assertEqual(self.f.parse(bad), 0)
            self.assertNotEqual(self.f.validate(), 0)

    def test_real_scan_applies_correct_registers_and_auto_detects_version(self):
        f, bus = self.f, self.bus
        self.assertEqual(f.call('Scan_Ports'), 1)
        self.assertEqual(f.u.mem_read(f.symbols['tmag_actual_sensors'] + 1, 1)[0], 0x36)
        self.assertEqual(f.u.mem_read(f.symbols['tmag_reference_sensors'] + 1, 1)[0], 0x35)
        for addr in (0x36, 0x35):
            self.assertEqual(bus.regs[addr, 0] & 0x1c, 0x10)  # average 16x in CONFIG_1
            self.assertEqual(bus.regs[addr, 1] & 3, 2)
            self.assertEqual(bus.regs[addr, 3] & 3, 0)
            self.assertIn((addr, 0x18, 0x10), bus.writes)
            self.assertIn((addr, 0x1c, 6), bus.writes)
        handle = f.symbols['tmag_actual_sensors']
        # ARM EABI selects a byte-sized enum for these values.
        self.assertEqual(f.u.mem_read(handle + 2, 1)[0], 1)

    def test_range_change_preserves_other_register_bits(self):
        f, bus = self.f, self.bus
        f.call('Scan_Ports')
        bus.regs[0x36, 3] = 0xc0
        handle = f.symbols['tmag_actual_sensors']
        self.assertEqual(f.call('TMAG3001_SetRange', handle, 1), 0)
        self.assertEqual(bus.regs[0x36, 3], 0xc3)
        self.assertEqual(f.call('TMAG3001_SetRange', handle, 0), 0)
        self.assertEqual(bus.regs[0x36, 3], 0xc0)

    def test_init_recovers_read_format_modified_by_old_trigger(self):
        self.bus.regs[0x36,0] = 1
        self.bus.regs[0x35,0] = 1
        self.assertEqual(self.f.call('Scan_Ports'),1)
        for addr in (0x36,0x35):
            self.assertIn((addr,0,0),self.bus.writes)
            self.assertEqual(self.bus.regs[addr,0] & 3,0)

    def test_failed_register_write_prevents_ready_and_running(self):
        self.bus.fail_reg = 9
        self.assertEqual(self.f.call('sensor_start'), 1)
        self.assertEqual(self.f.u.mem_read(self.f.symbols['SENSOR_Run'], 1)[0], 0)
        self.assertEqual(self.f.u.mem_read(self.f.symbols['SENSOR_List'], 1)[0], 0)

    def test_continuous_calibration_waits_for_ten_distinct_samples(self):
        f, bus = self.f, self.bus
        self.assertEqual(f.call('sensor_start'), 0)
        self.assertGreaterEqual(bus.now, 50)
        self.assertEqual(bus.state[0x36]['count'], 10 % 8)
        self.assertEqual(f.u.mem_read(f.symbols['sensor_calibration'] + 64, 1)[0], 1)
        self.assertTrue(all(b-a <= 3 for a,b in zip(bus.feeds, bus.feeds[1:])))

    def test_standby_calibration_uses_control_byte_triggers(self):
        self.assertEqual(self.f.parse({'product': 'MA02', 'sensor': {'opmode': 0}}), 0)
        self.assertEqual(self.f.call('sensor_start'), 0)
        self.assertEqual(len(self.bus.transactions), 20)
        self.assertEqual(set(self.bus.transactions), {(0x6c, 0x98), (0x6a, 0x98)})
        self.assertEqual(self.bus.regs[0x36, 0] & 3, 0)  # I2C read mode was not modified

    def test_timeout_is_bounded_and_feeds_watchdog(self):
        self.bus.never_ready = True
        self.assertEqual(self.f.call('sensor_start'), 1)
        self.assertLessEqual(self.bus.now, 2200)
        self.assertGreater(self.bus.now, 100)
        self.assertEqual(self.f.u.mem_read(self.f.symbols['SENSOR_Run'], 1)[0], 0)
        self.assertTrue(all(b-a <= 3 for a,b in zip(self.bus.feeds, self.bus.feeds[1:])))

    def test_sleep_modes_do_not_falsely_start_host_sampling(self):
        for mode in (1, 3):
            self.assertEqual(self.f.parse({'product': 'MA02', 'sensor': {'opmode': mode}}), 0)
            self.assertEqual(self.f.call('sensor_start'), 1)
            self.assertEqual(self.f.u.mem_read(self.f.symbols['SENSOR_Run'], 1)[0], 0)

    def test_import_applies_to_hardware_and_preserves_inactive_fields(self):
        f = self.f
        f.fake_flash()
        wire = '{"product":"MA02","sensor":{"gain":9,"osr":5,"filt":5,"resx":3,"opmode":2}}'
        f.command('ML IMPORT_START '+str(len(wire)))
        for i in range(0,len(wire),47): f.command('ML IMPORT_DATA '+wire[i:i+47])
        self.assertIn('IMPORT_OK', f.command('ML IMPORT_END'))
        self.assertEqual(self.bus.regs[0x36,9], 9)
        self.assertEqual(self.bus.regs[0x35,0] & 0x1c, 0x14)
        f.command('ML IMPORT_CANCEL')
        actual = json.loads(f.command('ML GETCFG').splitlines()[1])
        self.assertEqual(actual['product'], 'MA02')
        self.assertEqual(actual['sensor']['filt'], 5)
        self.assertEqual(actual['sensor']['resx'], 3)
        self.assertEqual(f.call('Config_Load', f.symbols['g_config']), 0)
        self.assertEqual(f.config()[5:7], bytes([5,3]))

    def test_import_and_apply_commands_report_hardware_failure(self):
        f = self.f
        f.fake_flash()
        self.bus.fail_reg = 9
        wire = '{"product":"MA02"}'
        f.command('ML IMPORT_START '+str(len(wire)))
        f.command('ML IMPORT_DATA '+wire)
        self.assertIn('IMPORT_ERROR: config saved but sensor apply failed', f.command('ML IMPORT_END'))
        f.command('ML IMPORT_CANCEL')
        self.assertIn('Config apply failed', f.command('ML APPLYCFG'))

    def test_real_snapshot_has_all_fields_and_matches_web_schema(self):
        f = self.f
        f.command('ML SET sensor.gain 5')
        snapshot = json.loads(f.command('ML GETCFG').splitlines()[1])
        script = "const C=require('./gui/web/core.js');const p=C.prepareImport(JSON.parse(process.argv[1]));if(p.missing.length)throw Error(p.missing);"
        subprocess.run(['node','-e',script,json.dumps(snapshot)],cwd=ROOT,check=True)
        self.assertEqual(snapshot['product'],'MA02')
        self.assertEqual(snapshot['sensor']['type'],1)

    def test_failed_sample_does_not_output_previous_sample(self):
        f = self.f
        self.assertEqual(f.call('sensor_start'),0)
        self.bus.never_ready = True
        self.assertEqual(f.call('get_sensor_data'),1)
        self.assertEqual(f.u.mem_read(f.symbols['processed_sensor_data']+32,1)[0],0)

    def test_legacy_flash_product_migrates_without_losing_user_values(self):
        f = self.f
        f.fake_flash()
        self.assertEqual(f.parse({'product':'MA02','sensor':{'gain':9},'algo':{'xmap':1.25}}),0)
        self.assertEqual(f.call('Config_Save',f.symbols['g_config']),0)
        base = 0x0800fc04
        f.u.mem_write(base+40,b'MA01\0')
        raw = bytes(f.u.mem_read(base,52))
        f.u.mem_write(base+52,struct.pack('<I',sum(raw)))
        self.assertEqual(f.call('Config_Load',f.symbols['g_config']),0)
        self.assertEqual(f.config()[40:45],b'MA02\0')
        self.assertEqual(f.config()[3],9)
        self.assertEqual(struct.unpack('<f',f.config()[16:20])[0],1.25)


if __name__ == '__main__':
    unittest.main(verbosity=2)
