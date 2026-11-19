"""Run portable S0-S9 code from the ARM image, independently of the sensor manager.

Uses the same H7 single-precision Unicorn build as test_legacy_golden.py.
No new golden samples are recorded. All expectations are physical/behavioural.
"""
import json
import math
import struct
import unittest

import test_ak09974c_firmware as fw
from unicorn.arm_const import (UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
                               UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3)

BASE = 0x20010000
IN, TIME, CC, PARAMS, STATE, OUT, TRACE = (BASE+x for x in (0, 64, 128, 256, 512, 4608, 4864))


class Pipeline:
    def __init__(self, profile=1, flags=0, rate=100):
        self.f, self.flags, self.rate, self.seq = fw.Firmware(), flags, rate, 0
        self.f.call('Algo_DefaultParams', PARAMS)
        self.f.u.mem_write(PARAMS+12, bytes([profile]))
        self.f.call('Algo_Reset', STATE)
        self.f.u.mem_write(CC, bytes(104))
        self.f.u.mem_write(CC+64, b'\x01')  # calibrated
        self.f.u.mem_write(CC+80, bytes([1,0,0,0,1,0,0,0,1])*2)
        self.f.u.mem_write(CC+98, struct.pack('<Hf', 3, 0.1))
        self.f.u.mem_write(CC+68, struct.pack('<3f', 1,1,1))
        self.f.u.mem_write(CC+32, struct.pack('<4f4f', 1,1,1,0, 1,1,1,0))

    def param(self, key, value):
        offsets = {'dead':0, 'stab':8, 'spike_k':32, 'stab_k':36,
                   'kx':40, 'ky':44, 'kz':48}
        self.f.u.mem_write(PARAMS+offsets[key], struct.pack('<f', value))

    def step(self, a=(0,0,0), r=(0,0,0), valid=1, actual_us=None,
             reference_us=None, error=0, triggered=1, reference_output=1, trace=False, bypass=0):
        self.seq += 1
        t = actual_us if actual_us is not None else round(self.seq*1e6/self.rate)
        tr = reference_us if reference_us is not None else t
        self.f.u.mem_write(IN, struct.pack('<4f4fB3x', *a,25,*r,25,valid))
        self.f.u.mem_write(TIME, struct.pack('<IIIHBBB3x', t&0xFFFFFFFF,tr&0xFFFFFFFF,
                                           self.seq,self.rate,1,triggered,error))
        u = self.f.u
        stop = 0x0803FFF0
        sp = 0x2001F000
        u.reg_write(UC_ARM_REG_SP, sp)
        u.reg_write(UC_ARM_REG_LR, stop|1)
        for reg, v in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3),
                          (IN,TIME,CC,PARAMS)):
            u.reg_write(reg,v)
        u.mem_write(sp,struct.pack('<6I', self.flags,reference_output,STATE,OUT,bypass,TRACE if trace else 0))
        # Algo_Process has eight arguments; extra zero is harmless stack space.
        u.emu_start(self.f.sym['Algo_ProcessTrace' if trace else 'Algo_Process']|1,stop,count=2_000_000)
        assert u.reg_read(UC_ARM_REG_PC)==stop
        b=bytes(u.mem_read(OUT,68))
        return b[32],struct.unpack_from('<3f',b),struct.unpack_from('<3f',b,16)


class AlgorithmTests(unittest.TestCase):
    def test_trace_does_not_change_outputs_in_either_profile(self):
        for profile in (0, 1):
            normal, traced = Pipeline(profile=profile, flags=255), Pipeline(profile=profile, flags=255)
            for i in range(20):
                a, r = (math.sin(i)*4, 2, -3), (i*.1, 1, -.5)
                self.assertEqual(normal.step(a, r), traced.step(a, r, trace=True))
            data=bytes(traced.f.u.mem_read(TRACE,256))
            present, executed=struct.unpack_from('<HH',data,248)
            self.assertEqual(present,1023 if profile else 1013)
            self.assertEqual(executed,present)
            self.assertEqual(data[253],1)

    def test_trace_records_real_stage_values_and_bypass(self):
        p=Pipeline(flags=8)
        _,a,_=p.step((10,20,30),(1,2,3),trace=True)
        self.assertEqual(a,(9,22,33))
        data=bytes(p.f.u.mem_read(TRACE,256))
        self.assertEqual(struct.unpack_from('<3f',data,0),(10,20,30))
        self.assertEqual(struct.unpack_from('<3f',data,4*12),a)
        self.assertTrue(struct.unpack_from('<H',data,250)[0] & (1<<4))
        _,a,_=p.step((10,20,30),(1,2,3),trace=True,bypass=1<<4)
        self.assertEqual(a,(10,20,30))
        self.assertFalse(struct.unpack('<H',p.f.u.mem_read(TRACE+250,2))[0] & (1<<4))

    def test_invalid_trace_never_replays_previous_valid_nodes(self):
        p=Pipeline(flags=4)
        p.step((10,20,30),trace=True)
        p.step((math.nan,0,0),trace=True)
        data=bytes(p.f.u.mem_read(TRACE,256))
        self.assertEqual(data[253:255],bytes([0,5]))
        self.assertEqual(struct.unpack_from('<HH',data,248),(0,0))

    def test_v2_background_gain_and_signs(self):
        p=Pipeline(flags=9)
        p.param('kx',2)
        valid,a,r=p.step((20,30,40),(3,4,5))
        self.assertEqual(valid,1)
        self.assertEqual(a,(14,34,45))
        self.assertEqual(r,(3,4,5))

    def test_time_alignment_uses_microseconds_and_reference_delta(self):
        p=Pipeline(flags=8)
        p.step((0,0,0),(1,0,0),actual_us=11000,reference_us=10000)
        _,a,_=p.step((5,0,0),(3,0,0),actual_us=22000,reference_us=20000)
        self.assertAlmostEqual(a[0],1.6,places=5)  # 5 - (3 + 2/10000*2000)

    def test_continuous_observation_does_not_invent_alignment(self):
        p=Pipeline(flags=8)
        p.step(r=(1,0,0),actual_us=11000,reference_us=10000,triggered=0)
        _,a,_=p.step((5,0,0),(3,0,0),actual_us=22000,reference_us=20000,triggered=0)
        self.assertEqual(a[0],2)

    def test_invalid_nan_and_faults_do_not_update_filter(self):
        p=Pipeline(flags=4)
        self.assertEqual(p.step((10,10,10))[1],(10,10,10))
        for a,error,valid in [((1000,0,0),0,0),((math.nan,0,0),0,1),((1000,0,0),2,1)]:
            self.assertEqual(p.step(a,error=error,valid=valid)[0],0)
        self.assertEqual(p.step((10,10,10))[1],(10,10,10))

    def test_mapping_overflow_is_not_emitted_as_valid_infinity(self):
        p=Pipeline(flags=16)
        p.f.u.mem_write(CC+68,struct.pack('<3f',1e38,1,1))
        self.assertEqual(p.step((100,0,0))[0],0)

    def test_unipolar_is_independent_of_mapping(self):
        p=Pipeline(flags=64)
        self.assertEqual(p.step((1,2,-3),r=(1,2,-4))[1:],((1,2,3),(1,2,-4)))
        self.assertEqual(p.step((1,2,3))[1][2],0)
        p=Pipeline(flags=16)
        self.assertEqual(p.step((1,2,-3))[1][2],-3)

    def test_hysteresis_uses_combined_compensation_noise(self):
        p=Pipeline(flags=2|8)
        p.param('dead',1)
        self.assertEqual(p.step((1.3,0,0))[1][0],0)  # threshold sqrt(2)
        self.assertAlmostEqual(p.step((1.5,0,0))[1][0],1.5)
        self.assertAlmostEqual(p.step((1.1,0,0))[1][0],1.1)
        self.assertEqual(p.step((.9,0,0))[1][0],0)

    def test_iir_time_constant_is_consistent_at_20_and_100_hz(self):
        results=[]
        for rate in (20,100):
            p=Pipeline(flags=4,rate=rate)
            p.step()
            for _ in range(rate):
                _,a,_=p.step((10,0,0))
            results.append(a[0])
        self.assertLess(abs(results[0]-results[1]),.1)
        self.assertGreater(min(results),9.8)

    def test_spike_rejects_two_points_then_accepts_real_step(self):
        p=Pipeline(flags=128)
        for _ in range(10): p.step()
        self.assertEqual(p.step((100,0,0))[1][0],0)
        self.assertEqual(p.step((100,0,0))[1][0],0)
        self.assertEqual(p.step()[1][0],0)
        for _ in range(3): _,a,_=p.step((100,0,0))
        self.assertEqual(a[0],100)
        self.assertEqual(p.step((100,7,0))[1][0],100)

    def test_spike_timeout_never_locks_on_ramp(self):
        p=Pipeline(flags=128)
        for _ in range(10): p.step()
        outputs=[p.step((i*50,0,0))[1][0] for i in range(1,36)]
        self.assertGreater(max(outputs),1000)

    def test_timestamp_wrap_is_safe(self):
        p=Pipeline(flags=8)
        p.step(r=(1,0,0),actual_us=0xFFFFF000,reference_us=0xFFFFF000)
        _,a,_=p.step((5,0,0),(2,0,0),actual_us=0x2000,reference_us=0x1000)
        self.assertAlmostEqual(a[0],2.5)

    def test_two_channel_states_are_independent(self):
        p,q=Pipeline(flags=4),Pipeline(flags=4)
        p.step((100,0,0))
        self.assertEqual(q.step((10,0,0))[1][0],10)

    def test_reference_debug_toggle_does_not_change_actual_filter_state(self):
        p,q=Pipeline(flags=4|8),Pipeline(flags=4|8)
        for i in range(40):
            a,r=(i,2*i,-i),(i*.1,0,0)
            self.assertEqual(p.step(a,r,reference_output=i%2)[1],q.step(a,r,reference_output=0)[1])

    def test_orientation_rotates_offsets_and_noise_with_samples(self):
        p=Pipeline(flags=1)
        p.f.u.mem_write(CC,struct.pack('<4f',10,20,30,0))
        p.f.u.mem_write(CC+32,struct.pack('<4f',1,2,3,0))
        p.f.u.mem_write(CC+80,struct.pack('<9b',0,1,0,1,0,0,0,0,-1))
        self.assertEqual(p.step((11,22,33))[1],(2,1,-3))
        self.assertEqual(struct.unpack('<3f',bytes(p.f.u.mem_read(OUT+52,12))),(2,1,3))

    def test_legacy_time_scaled_window(self):
        p=Pipeline(profile=0,flags=4,rate=100)
        for _ in range(50): p.step()
        self.assertAlmostEqual(p.step((50,0,0))[1][0],1)

    def test_timed_window_expires_old_samples_after_gap(self):
        p=Pipeline(profile=0,flags=4)
        p.step((100,0,0),actual_us=10000)
        self.assertEqual(p.step((10,0,0),actual_us=700000)[1][0],10)

    def test_stability_decay_matches_across_rates(self):
        values=[]
        for rate in (20,100):
            p=Pipeline(flags=32,rate=rate)
            p.param('stab',1)
            for _ in range(rate*2): _,a,_=p.step((.1,0,0))
            values.append(a[0])
        self.assertLess(abs(values[0]-values[1]),.003)
        self.assertLess(max(values),.04)


class FirmwareIntegrationTests(unittest.TestCase):
    def test_legacy_calibration_uses_300_valid_samples_without_counter_wrap(self):
        f=self.f
        f.u.mem_write(f.sym['SENSOR_Run'],b'\x01')
        attempts=0
        def acquire():
            nonlocal attempts
            attempts+=1
            if attempts in (25,125,225): return 1
            index=attempts-sum(attempts>=x for x in (25,125,225))
            a=(1000+index,-2000+2*index,3000-index)
            r=(100+index,200-index,300+3*index)
            f.u.mem_write(f.sym['sensor_data'],struct.pack('<4f4fB3x',*a,25,*r,25,1))
            return 0
        f.stub('get_sensor_data',acquire)
        self.assertEqual(f.call('Sensor_CalibrateOwned'),0)
        self.assertEqual(attempts,303)
        result=bytes(f.u.mem_read(f.sym['sensor_calibration'],68))
        for actual,expected in zip(struct.unpack_from('<3f',result),(1150.5,-1699,2849.5)):
            self.assertAlmostEqual(actual,expected,places=3)
        self.assertEqual(result[64],1)
        self.assertAlmostEqual(struct.unpack_from('<f',result,32)[0],math.sqrt(7525),places=3)

    def setUp(self):
        self.f=fw.Firmware()
        self.f.call('Config_SetDefaults',self.f.sym['g_config'])
        self.f.call('AlgoParams_Load',self.f.sym['g_config'])

    def test_node_commands_are_runtime_only_and_protect_mandatory_stages(self):
        f=self.f
        original=bytes(f.u.mem_read(f.sym['g_config'],96))
        self.assertIn('trace=0',f.command('ML NODES'))
        self.assertIn('Error:',f.command('ML NODE 0 0'))
        self.assertIn('Error:',f.command('ML NODE 3 1'))  # absent in legacy
        self.assertIn('Error:',f.command('ML NODE 999 0'))
        self.assertIn('NODE s=4 enabled=0',f.command('ML NODE 4 0'))
        self.assertIn('bypass=16',f.command('ML NODES'))
        self.assertIn('NODE s=4 enabled=1',f.command('ML NODE 4 1'))
        self.assertIn('bypass=0',f.command('ML NODES'))
        self.assertIn('TRACE 1',f.command('ML TRACE 1'))
        self.assertIn('trace=1',f.command('ML NODES'))
        self.assertIn('Error:',f.command('ML TRACE 2'))
        self.assertIn('TRACE 0',f.command('ML TRACE 0'))
        self.assertEqual(bytes(f.u.mem_read(f.sym['g_config'],96)),original)

    def test_schema5_migration_preserves_parameters_and_defaults_to_legacy(self):
        f=self.f
        old=bytearray(f.u.mem_read(f.sym['g_config'],52))
        old[0]=5; old[2]=0x3B
        struct.pack_into('<f',old,16,1.25)
        flash=0x081E0000
        f.u.mem_map(flash,0x20000)
        f.u.mem_write(flash,struct.pack('<I',0x52454C57)+old+struct.pack('<I',sum(old)))
        f.stub('plat_cfgstore_ptr',lambda:flash)
        self.assertEqual(f.call('Config_Load',f.sym['g_config']),0)
        result=bytes(f.u.mem_read(f.sym['g_config'],96))
        self.assertEqual(result[:3],bytes([6,2,0x3B]))
        self.assertEqual(struct.unpack_from('<f',result,16)[0],1.25)
        self.assertEqual(result[52],0)
        self.assertEqual(struct.unpack_from('<H',result,54)[0],500)
        self.assertEqual(f.call('Config_Validate',f.sym['g_config']),0)
        # Corrupt old payload must never be treated as a successful migration.
        f.u.mem_write(flash+4+16,b'\xFE')
        self.assertEqual(f.call('Config_Load',f.sym['g_config']),1)

    def test_schema6_roundtrip_and_corrupt_extension(self):
        f=self.f
        self.assertIn('SET algo.profile=v2',f.command('ML SET algo.profile v2'))
        self.assertIn('SET algo.kx=1.25',f.command('ML SET algo.kx 1.25'))
        flash=0x081E0000
        f.u.mem_map(flash,0x20000)
        f.stub('plat_cfgstore_ptr',lambda:flash)
        f.stub('plat_cfgstore_write',lambda:f.u.mem_write(flash,bytes(f.u.mem_read(f.reg(0),f.reg(1)))))
        self.assertEqual(f.call('Config_Save',f.sym['g_config']),0)
        self.assertEqual(f.call('Config_Load',f.sym['g_config']),0)
        self.assertEqual(bytes(f.u.mem_read(f.sym['g_config']+52,1)),b'\x01')
        f.u.mem_write(flash+4+52,b'\x00')
        self.assertEqual(f.call('Config_Load',f.sym['g_config']),1)

    def test_profile_and_bounds_validation_and_getcfg(self):
        f=self.f
        for command in ['ML SET algo.profile v2','ML SET algo.window_ms 250','ML SET algo.kx 1.25']:
            self.assertIn('SET ',f.command(command))
        for command in ['ML SET algo.profile unknown','ML SET algo.window_ms 501',
                        'ML SET algo.tau_lp_ms 0','ML SET algo.spike_k nan',
                        'ML SET algo.kx 101','ML SET algo.spike_hold_ms 1']:
            self.assertIn('Error:',f.command(command))
        text=f.command('ML GETCFG')
        cfg=json.loads(text.split('CFG_BEGIN\r\n')[1].split('\r\nCFG_END')[0])
        self.assertEqual(cfg['algo']['profile'],1)
        self.assertEqual(cfg['algo']['window_ms'],250)
        self.assertEqual(cfg['algo']['kx'],1.25)
        self.assertLess(len(json.dumps(cfg)),1023)

    def test_reference_output_requires_v2_wire_format(self):
        f=self.f
        self.assertIn('required',f.command('ML REFOUT 1'))
        self.assertEqual(f.command('ML FORMAT v2'),'FORMAT v2\r\n')
        self.assertEqual(f.command('ML REFOUT 1'),'REFOUT 1\r\n')
        self.assertEqual(f.command('ML FORMAT legacy'),'FORMAT legacy\r\n')

    def test_timed_calibration_metadata_and_quality_rejection(self):
        f=self.f
        f.bus.devs={0x21:fw.FakeAK(f.bus,(100,-200,300)),0x22:fw.FakeAK(f.bus,(10,20,-30))}
        self.assertIn('SET ',f.command('ML SET algo.profile v2'))
        self.assertEqual(f.call('sensor_start'),0)
        status=f.command('ML STATUS')
        self.assertIn('calibrated=1',status)
        self.assertIn('CALIB version=1 samples=100 attempts=100',status)
        self.assertIn('temp_valid=0',status)
        self.assertEqual(f.floats(f.sym['sensor_calibration'],3),(110,-220,330))
        record=bytes(f.u.mem_read(f.sym['sensor_calibration'],68))
        meta=bytes(f.u.mem_read(f.sym['sensor_calibration_metadata'],32))
        checksum=2166136261
        for x in record+meta[:28]: checksum=((checksum^x)*16777619)&0xFFFFFFFF
        self.assertEqual(checksum,struct.unpack_from('<I',meta,28)[0])
        f.bus.devs[0x21].err=1
        self.assertEqual(f.call('Sensor_CalibrateOwned'),1)
        self.assertEqual(bytes(f.u.mem_read(f.sym['sensor_calibration']+64,1)),b'\x00')

    def test_calibration_requires_at_least_90_percent_valid_samples(self):
        f=self.f
        f.command('ML SET algo.profile v2')
        f.u.mem_write(f.sym['SENSOR_Run'],b'\x01')
        for failures,expected in [(10,0),(11,1)]:
            calls=[0]
            def read():
                calls[0]+=1
                valid=calls[0]>failures
                f.u.mem_write(f.sym['sensor_data'],struct.pack('<4f4fB3x',1,2,3,25,4,5,6,25,valid))
                return 0 if valid else 1
            f.stub('get_sensor_data',read)
            self.assertEqual(f.call('Sensor_CalibrateOwned'),expected)
            self.assertEqual(calls[0],100)

    def test_wire_format_exports_invalid_samples_and_reference_fields(self):
        f=self.f
        f.command('ML FORMAT v2');f.command('ML REFOUT 1')
        addr=f.sym['sensor_data']
        f.u.mem_write(addr,struct.pack('<4f4fB3x',1,2,3,25,4,5,-6,25,1))
        f.call('data_process')
        f.call('format_processed_sensor_data',0,BASE,384)
        line=f.string(BASE)
        self.assertIn('valid=1',line);self.assertIn('rx=4',line);self.assertIn('rrz=-6',line)
        self.assertNotIn(' T=',line)  # AK09974C has no measured temperature
        f.u.mem_write(addr+32,b'\x00');f.call('data_process')
        f.call('format_processed_sensor_data',0,BASE,384)
        self.assertIn('valid=0',f.string(BASE))
        f.command('ML FORMAT legacy')
        self.assertEqual(f.call('format_processed_sensor_data',0,BASE,384),0)


if __name__=='__main__':
    unittest.main(verbosity=2)
