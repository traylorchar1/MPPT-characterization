#!/usr/bin/env python3
"""Program the Photon application (0x0000-0x6FFF) over CAN through its bootloader.

  flash.py HEXFILE [--yes] [--resume]   (--resume: unit already in bootloader)

Erases the application blocks, writes every non-blank row, verifies the whole
application area by reading it back, then starts the application. The
bootloader (0x7000+) and FRAM are not touched; the bootloader substitutes its
own jump at address 0, which verify accounts for.
"""
import sys
import time

from photon_can import (BLOCK_SIZE, BOOTLOAD_START, ROW_SIZE, Bridge, Photon, erase_block,
                        load_hex_words, telemetry, write_latch, write_row)


def main():
    path = sys.argv[1]
    words = load_hex_words(path)
    if words.get(0, 0xFFFFFF) & 0xFF0000 != 0x040000:
        sys.exit('address 0 is not a goto; refusing')
    image = [words.get(pc, 0xFFFFFF) for pc in range(0, BOOTLOAD_START, 2)]
    rows = [r for r in range(0, BOOTLOAD_START, ROW_SIZE)
            if r == 0 or any(image[(r + i) // 2] != 0xFFFFFF for i in range(0, ROW_SIZE, 2))]
    print('%s: %d rows to write, last used 0x%04x' % (path, len(rows),
          max(pc for pc in range(0, BOOTLOAD_START, 2) if image[pc // 2] != 0xFFFFFF)))
    if '--yes' not in sys.argv:
        sys.exit('dry run; add --yes to program')
    t_wait = time.time()
    while True:
        try:
            b = Bridge()
            break
        except OSError:
            if time.time() - t_wait > 300:
                sys.exit('bridge not reachable for 5 min')
            time.sleep(1)
    p = Photon(b)
    if '--resume' in sys.argv:
        # The unit is already sitting in its bootloader after an interrupted flash.
        if not any(p.ping() == 'bootloader' for _ in range(10)):
            sys.exit('--resume: unit is not in the bootloader')
    elif not p.enter_bootloader():
        sys.exit('could not catch the bootloader')
    print('hw_rev', p.read_hw_rev())
    t0 = time.time()

    def reconnect():
        # Link dropped: wait for the bridge, reconnect, and confirm the unit is
        # still in its bootloader before carrying on.
        nonlocal_b[0].close()
        t_wait = time.time()
        while time.time() - t_wait < 300:
            try:
                nb = Bridge()
                np_ = Photon(nb)
                if np_.ping() == 'bootloader':
                    print('  reconnected after %.0fs' % (time.time() - t_wait), flush=True)
                    nonlocal_b[0], nonlocal_p[0] = nb, np_
                    return
                nb.close()
            except OSError:
                pass
            time.sleep(2)
        sys.exit('bridge did not come back within 5 min; unit is in its bootloader, rerun with --resume')

    nonlocal_b, nonlocal_p = [b], [p]

    def retry(fn, *a):
        while True:
            try:
                return fn(nonlocal_p[0], *a)
            except (OSError, TimeoutError) as e:
                print('  link problem (%s); waiting for bridge' % e, flush=True)
                reconnect()

    for blk in range(0, BOOTLOAD_START, BLOCK_SIZE):
        retry(erase_block, blk)
    print('erased in %.1fs' % (time.time() - t0))
    for k, r in enumerate(rows):
        while True:
            try:
                for i in range(0, ROW_SIZE, 2):
                    write_latch(nonlocal_p[0], r + i, image[(r + i) // 2])
                write_row(nonlocal_p[0], r)
                break
            except (OSError, TimeoutError) as e:
                # Latches are volatile; rewrite the whole row after reconnecting.
                print('  link problem at row 0x%04x (%s); waiting for bridge' % (r, e), flush=True)
                reconnect()
        if k % 20 == 0:
            print('  row 0x%04x (%d/%d) %.0fs' % (r, k, len(rows), time.time() - t0), flush=True)
    b, p = nonlocal_b[0], nonlocal_p[0]
    print('written in %.1fs; verifying' % (time.time() - t0))
    bad = []
    for pc in range(0, BOOTLOAD_START, 2):
        got = retry(lambda q, a: q.read_latch(a), pc) & 0xFFFFFF
        p = nonlocal_p[0]
        want = 0x047000 if pc == 0 else (0 if pc == 2 else image[pc // 2])
        if pc == 2 and got in (0, image[1]):
            continue
        if got != want:
            bad.append((pc, want, got))
            if len(bad) > 20:
                break
    if bad:
        print('VERIFY FAILED (staying in bootloader):', ['0x%04x want %06x got %06x' % x for x in bad[:10]])
        sys.exit(1)
    print('verify OK in %.1fs total; starting application' % (time.time() - t0))
    p.jump_to_app()
    fr = b.wait(lambda f: f[1] == 0x600, 5)
    print('app running:', bool(fr), telemetry(fr[3]) if fr else None)
    print('ping:', p.ping())
    b.close()


if __name__ == '__main__':
    main()
