"""Minimal client for the Tritium CAN-Ethernet bridge and the Photon 3.

The bridge speaks the Tritium TCP protocol: a 24-byte handshake, then 14-byte
frames (id u32 BE, flags u8, dlc u8, 8 data bytes). The bridge is on bus 13
and only reachable from the Mac's link-local alias (169.254.1.10 on en8).
"""
import queue
import socket
import struct
import threading
import time
import uuid

BRIDGE = '169.254.210.192'
SOURCE = '169.254.1.10'
BUS = 13
PORT = 4876

F_EXT, F_RTR, F_SETTINGS, F_HEARTBEAT = 0x01, 0x02, 0x40, 0x80

# Stock Photon application and bootloader commands (base + 0xA0).
SPECIAL = 0xA0
READ_LATCH, ACK, JUMP, PING, READ_FRAM, READ_HW_REV, RESET = 0x01, 0x05, 0x06, 0x07, 0x09, 0x0A, 0xFE
BOOTLOADER_PING, USER_CODE_PING = 0x01, 0x02
MAGIC = bytes([45, 78, 69, 21])


class Bridge:
    def __init__(self, bridge=BRIDGE, source=SOURCE, bus=BUS):
        self.sock = socket.create_connection((bridge, PORT), timeout=3, source_address=(source, 0))
        hs = bytearray(24)
        struct.pack_into('>LLQ', hs, 0, 0, 0x20000000, 0x0054726974697560 | bus)
        struct.pack_into('<Q', hs, 16, (uuid.getnode() << 16) & (2**64 - 1))
        self.sock.sendall(hs)
        self.sock.settimeout(0.2)
        self.rx = queue.Queue()
        self.listeners = []
        self._stop = False
        self._buf = b''
        self._first = True
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        while not self._stop:
            try:
                d = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not d:
                break
            self._buf += d
            if self._first and len(self._buf) >= 16:
                if self._buf[1:8] == b'Tritium':
                    self._buf = self._buf[16:]
                self._first = False
            while len(self._buf) >= 14 and not self._first:
                f, self._buf = self._buf[:14], self._buf[14:]
                cid, fl, dlc = struct.unpack('>IBB', f[:6])
                if fl & (F_HEARTBEAT | F_SETTINGS):
                    continue
                frame = (time.time(), cid, fl, f[6:6 + min(dlc, 8)])
                for fn in self.listeners:
                    fn(frame)
                self.rx.put(frame)

    def send(self, cid, data=b'', flags=0):
        p = bytearray(14)
        struct.pack_into('>IBB', p, 0, cid, flags, len(data))
        p[6:6 + len(data)] = data
        self.sock.sendall(p)

    def flush(self):
        while True:
            try:
                self.rx.get_nowait()
            except queue.Empty:
                return

    def wait(self, pred, timeout=1.0):
        end = time.time() + timeout
        while time.time() < end:
            try:
                fr = self.rx.get(timeout=max(0.001, end - time.time()))
            except queue.Empty:
                break
            if pred(fr):
                return fr
        return None

    def close(self):
        self._stop = True
        self.sock.close()


def telemetry(data):
    """Decode a stock 0x600/0x601 frame -> (vin V, iin A, vout V, temp C)."""
    vin, iin, vout, t = struct.unpack('<HHHh', data)
    return vin / 100, iin / 1000, vout / 100, t / 100


class Photon:
    def __init__(self, bridge, base=0x600):
        self.b = bridge
        self.base = base
        self.cmd_id = base + SPECIAL

    def _special(self, data):
        self.b.send(self.cmd_id, bytes(data).ljust(8, b'\0'))

    def _is_reply(self, fr, cmd=None):
        _, cid, fl, d = fr
        return cid == self.cmd_id and len(d) >= 1 and (cmd is None or d[0] == cmd)

    def ping(self, timeout=0.5):
        """Return 'bootloader', 'app' or None."""
        self.b.flush()
        self._special([PING])
        fr = self.b.wait(lambda f: self._is_reply(f, ACK) and len(f[3]) >= 2, timeout)
        if not fr:
            return None
        return {BOOTLOADER_PING: 'bootloader', USER_CODE_PING: 'app'}.get(fr[3][1], 'unknown')

    def enter_bootloader(self):
        """Reset the unit and catch it inside the bootloader's 4 s window."""
        self.b.flush()
        self._special(bytes([RESET, 0, 0, 0]) + MAGIC)
        self.b.wait(lambda f: self._is_reply(f, ACK), 1.0)
        end = time.time() + 6
        while time.time() < end:
            if self.ping(0.2) == 'bootloader':
                return True
        return False

    def _read32(self, cmd, addr, tries=5):
        req = bytes([cmd]) + struct.pack('<I', addr)[:3]
        for _ in range(tries):
            self.b.flush()
            self._special(req)
            fr = self.b.wait(lambda f: self._is_reply(f, cmd) and len(f[3]) == 8 and f[3][1:4] == req[1:4], 0.3)
            if fr:
                return struct.unpack('<I', fr[3][4:8])[0]
        raise TimeoutError('no reply to cmd 0x%02x addr 0x%06x' % (cmd, addr))

    def read_latch(self, addr):
        return self._read32(READ_LATCH, addr)

    def read_fram(self, addr):
        return self._read32(READ_FRAM, addr)

    def read_hw_rev(self):
        self.b.flush()
        self._special([READ_HW_REV])
        fr = self.b.wait(lambda f: self._is_reply(f, READ_HW_REV) and len(f[3]) == 8, 0.5)
        return struct.unpack('<I', fr[3][4:8])[0] if fr else None

    def jump_to_app(self):
        self._special([JUMP])
        self.b.wait(lambda f: self._is_reply(f, ACK), 1.0)


# ---------------------------------------------------------------------------
# Bootloader programming (same sequence as Dilithium's can_program_cli.py).
WRITE_LATCH, WRITE_BLOCK, ERASE_BLOCK = 0x02, 0x03, 0x04
BOOTLOAD_START, BLOCK_SIZE, ROW_SIZE = 0x7000, 0x400, 0x80


def _acked(p, data, tries=5, timeout=0.5):
    for _ in range(tries):
        p.b.flush()
        p._special(data)
        if p.b.wait(lambda f: p._is_reply(f, ACK), timeout):
            return
    raise TimeoutError('no ACK for %s' % bytes(data).hex())


def erase_block(p, addr):
    _acked(p, bytes([ERASE_BLOCK]) + struct.pack('<I', addr)[:3], timeout=1.0)


def write_latch(p, addr, word):
    _acked(p, bytes([WRITE_LATCH]) + struct.pack('<I', addr)[:3] + struct.pack('<I', word))


def write_row(p, addr):
    _acked(p, bytes([WRITE_BLOCK]) + struct.pack('<I', addr)[:3], timeout=1.0)


def load_hex_words(path, end=BOOTLOAD_START):
    """Intel hex (XC16 layout) -> {pc_addr: 24-bit word} for pc < end."""
    mem, base = {}, 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(':'):
            continue
        n, a, t = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
        d = bytes.fromhex(line[9:9 + 2 * n])
        if t == 4:
            base = int.from_bytes(d, 'big') << 16
        elif t == 0:
            for i, x in enumerate(d):
                mem[base + a + i] = x
    words = {}
    for b in sorted(mem):
        pc = (b // 4) * 2
        if pc < end and pc not in words:
            words[pc] = mem.get(pc * 2, 0xFF) | mem.get(pc * 2 + 1, 0xFF) << 8 | mem.get(pc * 2 + 2, 0xFF) << 16
    return words
