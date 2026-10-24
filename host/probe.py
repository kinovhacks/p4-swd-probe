#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host bridge and self-test tool for the ESP32-P4 UART SWD probe.

The probe sends CMSIS-DAP packets over a 921600-baud serial link using the
same framing as OpenOCD's CMSIS-DAP TCP backend. This tool either exposes
that link as a local TCP server (for OpenOCD) or runs a protocol self-test.
"""
import argparse
import json
import select
import socket
import struct
import time

import serial

HEADER = struct.Struct('<4sHBB')
BAUD = 921600
PRODUCT = 'P4 SWD UART Probe'


class Probe:
    def __init__(self, port):
        # Configure DTR/RTS before opening: some USB-UART drivers pulse the
        # board's auto-reset circuit when the port is opened.
        self.s = serial.Serial()
        self.s.port = port
        self.s.baudrate = BAUD
        self.s.timeout = 2
        self.s.write_timeout = 2
        self.s.dtr = False
        self.s.rts = False
        self.s.open()
        time.sleep(1)
        self.s.reset_input_buffer()

    def exact(self, n):
        b = bytearray()
        while len(b) < n:
            part = self.s.read(n - len(b))
            if not part:
                raise TimeoutError('P4 response timeout')
            b.extend(part)
        return bytes(b)

    def command(self, data):
        self.s.write(HEADER.pack(b'DAP\0', len(data), 1, 0) + data)
        window = bytearray()
        deadline = time.monotonic() + 3
        while bytes(window) != b'DAP\0':
            if time.monotonic() > deadline:
                raise TimeoutError('Frame sync timeout')
            window.extend(self.exact(1))
            window = window[-4:]
        magic, n, kind, res = HEADER.unpack(bytes(window) + self.exact(4))
        if magic != b'DAP\0' or kind != 2 or res or not 1 <= n <= 1024:
            raise ValueError('Invalid frame')
        return self.exact(n)

    def close(self):
        try:
            self.command(b'\x03')
        except (OSError, ValueError, TimeoutError):
            pass
        finally:
            self.s.close()


def test(p):
    checks = {}
    for name, i in [('vendor', 1), ('product', 2), ('version', 4),
                    ('capabilities', 0xf0), ('packet_size', 0xff)]:
        r = p.command(bytes([0, i]))
        assert r[0] == 0 and r[1] == len(r) - 2
        checks[name] = r[2:].hex() if i >= 0xf0 else r[2:].rstrip(b'\0').decode()

    diag = p.command(b'\x80')
    assert len(diag) == 15
    checks['revision'] = int.from_bytes(diag[2:4], 'little')
    checks['flash_bytes'] = int.from_bytes(diag[4:8], 'little')
    checks['gpio'] = list(diag[12:15])

    r = p.command(b'\x81')
    assert r == b'\x81\x00', r.hex()
    checks['gpio_self_readback'] = '6/6 levels passed; no external loopback'
    assert p.command(b'\x02\x01') == b'\x02\x01'
    assert p.command(b'\x11' + struct.pack('<I', 100000)) == b'\x11\x00'
    assert p.command(b'\x04\x00\x02\x00\x00\x00') == b'\x04\x00'
    assert p.command(bytes([0x12, 56]) + b'\xff' * 7) == b'\x12\x00'
    r = p.command(bytes([5, 0, 1, 2]))
    assert r[0] == 5 and r[1] == 0 and r[2] != 1, r.hex()
    checks['unconnected_target_read'] = r.hex()
    assert p.command(b'\x03') == b'\x03\x00'

    invalid = [b'\x11', b'\x11\0\0\0\0', b'\x05\0\xff', b'\x06\0\xff\xff\x02',
               b'\x7f\xff', b'\x1d\xff']
    for packet in invalid:
        assert p.command(packet) == b'\xff'
    checks['malformed_rejected'] = len(invalid)

    t = time.monotonic()
    for _ in range(1000):
        r = p.command(b'\x00\x02')
        assert PRODUCT in r.decode(errors='replace')
    checks['stress_packets'] = 1000
    checks['stress_seconds'] = round(time.monotonic() - t, 3)
    print(json.dumps(checks, indent=2))


def exact_socket(c, n):
    b = bytearray()
    while len(b) < n:
        x = c.recv(n - len(b))
        if not x:
            raise EOFError
        b.extend(x)
    return bytes(b)


def serve(p, port):
    with socket.socket() as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('127.0.0.1', port))
        s.listen(1)
        print(f'CMSIS-DAP serial bridge listening on 127.0.0.1:{port}', flush=True)
        while True:
            c, _ = s.accept()
            with c:
                c.settimeout(2)
                try:
                    while True:
                        # Keep an idle GDB connection alive; the probe releases
                        # its GPIOs by itself if the bridge stops for ~5 s.
                        if not select.select([c], [], [], 1)[0]:
                            p.command(b'\x00\xf0')
                            continue
                        m, n, k, r = HEADER.unpack(exact_socket(c, 8))
                        if m != b'DAP\0' or k != 1 or r or not 1 <= n <= 1024:
                            raise ValueError('Invalid request')
                        out = p.command(exact_socket(c, n))
                        c.sendall(HEADER.pack(m, len(out), 2, 0) + out)
                except (EOFError, socket.timeout):
                    pass
                finally:
                    try:
                        p.command(b'\x03')
                    except (OSError, ValueError, TimeoutError):
                        pass


def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('--port', required=True,
                   help='serial port of the ESP32-P4 UART bridge')
    a.add_argument('mode', choices=['test', 'serve'])
    a.add_argument('--tcp-port', type=int, default=4441,
                   help='loopback TCP port for serve mode (default: 4441)')
    args = a.parse_args()
    p = Probe(args.port)
    try:
        if args.mode == 'test':
            test(p)
        else:
            serve(p, args.tcp_port)
    except KeyboardInterrupt:
        pass
    finally:
        p.close()


if __name__ == '__main__':
    main()
