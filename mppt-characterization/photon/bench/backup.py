#!/usr/bin/env python3
"""Read-only backup of a Photon 3: hw rev, FRAM and all program memory.

Resets the unit into its CAN bootloader, reads everything, then jumps back to
the application. Writes nothing to the device.
  backup.py [outdir]
"""
import json
import os
import struct
import sys
import time

from photon_can import Bridge, Photon, telemetry

PM_END = 0xAC00     # dsPIC33FJ64GS606 program memory end (PC units), includes bootloader
FRAM_SIZE = 0x200


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), 'backups', time.strftime('%Y%m%d_%H%M%S'))
    os.makedirs(out, exist_ok=True)
    b = Bridge()
    p = Photon(b)
    print('ping:', p.ping())
    if not p.enter_bootloader():
        sys.exit('could not catch the bootloader')
    print('in bootloader')
    rev = p.read_hw_rev()
    print('hw_rev:', rev)

    fram = bytearray()
    for a in range(0, FRAM_SIZE, 4):
        fram += struct.pack('<I', p.read_fram(a))
    open(os.path.join(out, 'fram.bin'), 'wb').write(fram)
    print('fram: %d bytes, params length %d' % (len(fram), struct.unpack('<I', fram[:4])[0]))

    words = []
    t0 = time.time()
    for a in range(0, PM_END, 2):
        words.append(p.read_latch(a) & 0xFFFFFF)
        if a % 0x1000 == 0:
            print('  pm 0x%04x  %.0fs' % (a, time.time() - t0), flush=True)
    with open(os.path.join(out, 'program.bin'), 'wb') as f:
        for w in words:
            f.write(struct.pack('<I', w))
    # Intel hex in the XC16 byte layout (4 bytes per instruction word, byte address = 2 * PC).
    with open(os.path.join(out, 'program.hex'), 'w') as f:
        def rec(addr, typ, data):
            body = bytes([len(data), addr >> 8 & 0xFF, addr & 0xFF, typ]) + bytes(data)
            f.write(':' + body.hex().upper() + '%02X\n' % (-sum(body) & 0xFF))
        raw = b''.join(struct.pack('<I', w) for w in words)
        seg = -1
        for off in range(0, len(raw), 16):
            if off >> 16 != seg:
                seg = off >> 16
                rec(0, 4, [seg >> 8, seg & 0xFF])
            rec(off & 0xFFFF, 0, raw[off:off + 16])
        rec(0, 1, [])
    last = max((i for i, w in enumerate(words[:0x7000 // 2]) if w != 0xFFFFFF), default=0)
    meta = {'hw_rev': rev, 'pm_end': PM_END, 'app_last_used_pc': hex(last * 2),
            'seconds': round(time.time() - t0, 1), 'time': time.strftime('%Y-%m-%d %H:%M:%S')}
    json.dump(meta, open(os.path.join(out, 'meta.json'), 'w'), indent=1)
    print(meta)

    p.jump_to_app()
    fr = b.wait(lambda f: f[1] == 0x600, 5)
    print('back in app:', bool(fr), telemetry(fr[3]) if fr else None)
    b.close()
    print('saved to', out)


if __name__ == '__main__':
    main()
