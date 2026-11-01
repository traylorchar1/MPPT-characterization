#!/usr/bin/env python3
"""Photon application service client (base+0xD0 requests, base+0xE0 replies).

  svc.py id                        protocol identifier
  svc.py get NAME|ID [...]         read registers
  svc.py set NAME|ID VALUE [...]   write (TUNE registers are live)
  svc.py stop | start | retrack
  svc.py dump                      all registers
"""
import struct
import sys

from photon_can import Bridge

FIELDS = {}  # name -> (id, type)
for k, n in {2: 'batt_v_raw', 3: 'batt_i_raw', 4: 'temp_ambient', 5: 'safety', 6: 'control', 9: 'loops',
             17: 'alg', 18: 'state_max_dac', 21: 'fpga_fault'}.items():
    FIELDS[n] = (k, 'i')
for c, base in ((0, 100), (1, 200)):
    for off, n in {1: 'v_raw', 3: 'v_goal_raw', 6: 'i_term', 7: 'i_raw', 10: 'temp', 11: 'dac', 12: 'dir',
                   22: 'p_raw_sweep_mpp', 23: 'v_raw_sweep_mpp', 25: 'sweep_done', 27: 'voc_raw', 28: 'vsc_raw',
                   40: 'po_step', 41: 'po_p_prev', 42: 'po_v_avg', 43: 'po_i_avg', 44: 'po_clamp'}.items():
        FIELDS['c%d.%s' % (c, n)] = (base + off, 'i')
for k, n, t in [(1000, 'serial_number', 'i'), (1001, 'hw_rev', 'i'), (1002, 'device.v_scale', 'f'),
                (1003, 'device.i_scale', 'f'), (1004, 'v_cv_charge', 'f'), (1005, 'v_cv_charge_hyst', 'f'),
                (1010, 'c0.v_scale', 'f'), (1011, 'c0.i_scale', 'f'), (1012, 'c1.v_scale', 'f'),
                (1013, 'c1.i_scale', 'f'), (1020, 'can_bitrate', 'i'), (1021, 'can_base_addr', 'i'),
                (1031, 'min_retrack_time', 'i')]:
    FIELDS[n] = (k, t)
TUNE = ['algorithm', 'algorithm_ticks', 'retrack_ticks', 'sweep_timeout_ticks', 'voc_ticks', 'sweep_dac_step',
        'inc_step', 'p_divisor', 'i_divisor', 'min_voltage_raw', 'max_voltage_raw', 'max_pid_error',
        'sweep_floor_percent', 'power_drop_percent', 'po_period_ticks', 'po_settle_ticks', 'po_step_min',
        'po_step_max', 'po_up_clamp', 'po_down_clamp', 'hold_v_raw', 'stream', 'max_dac', 'oc_arr_raw', 'ceiling_creep']
for i, n in enumerate(TUNE):
    FIELDS[n] = (1100 + i, 'i')
BY_ID = {v[0]: (k, v[1]) for k, v in FIELDS.items()}
CONTROL = ['INIT', 'PRECHARGING', 'CLOSING_MAIN', 'STARTING', 'CAN_DISABLE', 'TRACKING', 'ERROR_OPEN_RELAY',
           'ERROR_CLOSED_RELAY', 'TEST_DAC_STEP', 'TEST_CONTROL_VOLTAGE', 'TEST_MODE', 'CONSTANT_VOLTAGE']
ALG = ['SWEEP', 'PERTURB_OBSERVE', 'INCREMENTAL_CONDUCTANCE', 'MEASURE_VOC']
ERRORS = {1: 'unknown field/op', 2: 'state does not allow it', 3: 'read-only or out of range', 4: 'active fault'}


class Service:
    def __init__(self, bridge=None, base=0x600):
        self.b = bridge or Bridge()
        self.req, self.rep = base + 0xD0, base + 0xE0
        self.seq = 0

    def call(self, op, fid=0, value=b'\0\0\0\0', tries=3):
        for _ in range(tries):
            self.seq = (self.seq + 1) & 0xFF
            data = bytes([op, self.seq]) + struct.pack('<H', fid) + value
            self.b.flush()
            self.b.send(self.req, data)
            fr = self.b.wait(lambda f: f[1] == self.rep and len(f[3]) == 8 and f[3][1] == self.seq, 0.5)
            if fr:
                d = fr[3]
                if d[0] == 0xFF:
                    raise RuntimeError('error %d: %s' % (d[4], ERRORS.get(d[4], '?')))
                return d[4:8]
        raise TimeoutError('no reply')

    def resolve(self, name):
        if name in FIELDS:
            return FIELDS[name]
        fid = int(name, 0)
        return fid, BY_ID.get(fid, (None, 'i'))[1]

    def get(self, name):
        fid, t = self.resolve(name)
        v = self.call(1, fid)
        return struct.unpack('<f' if t == 'f' else '<i', v)[0]

    def set(self, name, value):
        fid, t = self.resolve(name)
        self.call(2, fid, struct.pack('<f', float(value)) if t == 'f' else struct.pack('<i', int(float(value))))

    def stop(self): self.call(3)
    def start(self): self.call(4)
    def retrack(self): self.call(5)

    def identify(self):
        return struct.unpack('<I', self.call(0))[0]


def fmt(name, v):
    if name == 'control' and 0 <= v < len(CONTROL):
        return '%d (%s)' % (v, CONTROL[v])
    if name == 'alg' and 0 <= v < len(ALG):
        return '%d (%s)' % (v, ALG[v])
    return ('%.6g' % v) if isinstance(v, float) else str(v)


def main(argv):
    s = Service()
    cmd, args = (argv[0], argv[1:]) if argv else ('dump', [])
    if cmd == 'id':
        print('0x%08x' % s.identify())
    elif cmd == 'get':
        for n in args:
            print(n, fmt(n, s.get(n)))
    elif cmd == 'set':
        for n, v in zip(args[::2], args[1::2]):
            s.set(n, v)
            print(n, '->', fmt(n, s.get(n)))
    elif cmd in ('stop', 'start', 'retrack'):
        getattr(s, cmd)()
        print(cmd, 'ok')
    elif cmd == 'dump':
        for n in sorted(FIELDS, key=lambda k: FIELDS[k][0]):
            try:
                print('%-22s %s' % (n, fmt(n, s.get(n))))
            except Exception as e:
                print('%-22s %s' % (n, e))
    else:
        sys.exit(__doc__)
    s.b.close()


if __name__ == '__main__':
    main(sys.argv[1:])
