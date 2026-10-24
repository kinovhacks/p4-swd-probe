#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Logging wrapper around `probe.py serve` for protocol debugging.

Usage: p4bridge_log.py <serial-port> [tcp-port]

Every DAP request/response pair is appended to p4_wire.log next to this file.
"""
import argparse
import pathlib
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import probe as m  # noqa: E402

LOGPATH = HERE / 'p4_wire.log'
_log = None

_orig = m.Probe.command


def command(self, data):
    r = _orig(self, data)
    if _log is not None:
        t = time.strftime('%H:%M:%S')
        _log.write(f'{t} >>> {data.hex()}\n{t} <<< {r.hex()}\n')
        _log.flush()
    return r


m.Probe.command = command


def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('port')
    a.add_argument('tcp_port', nargs='?', type=int, default=4441)
    args = a.parse_args()
    global _log
    _log = open(LOGPATH, 'a')
    p = m.Probe(args.port)
    print(f'logging bridge on 127.0.0.1:{args.tcp_port}, log -> {LOGPATH}',
          flush=True)
    try:
        m.serve(p, args.tcp_port)
    except KeyboardInterrupt:
        pass
    finally:
        p.close()


if __name__ == '__main__':
    main()
