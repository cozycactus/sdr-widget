#!/usr/bin/env python3
"""Use a NanoVNA-H4 as a programmable clock source: park CH0 (S11/TX) at a fixed
CW frequency. Drive XIN1 through a squaring buffer (e.g. 74LVC1GU04 @3.3V).

Usage:  python3 nanovna_clock.py [freq_hz] [serial_port]
        python3 nanovna_clock.py 12288000
"""
import glob
import sys
import time

import serial
from serial.tools import list_ports


def find_port():
    # Prefer a device that identifies as a NanoVNA (STM VID 0x0483 / product name),
    # so we don't grab some unrelated USB-serial device.
    for p in list_ports.comports():
        if p.vid == 0x0483 or (p.product and "nanovna" in p.product.lower()):
            return p.device
    g = sorted(glob.glob("/dev/cu.usbmodem*"))
    return g[0] if g else None


def cmd(ser, c, wait=0.35):
    ser.reset_input_buffer()
    ser.write((c + "\r").encode())
    time.sleep(wait)
    return ser.read(8192).decode(errors="replace").replace("\r", "")


def main():
    freq = int(sys.argv[1]) if len(sys.argv) > 1 else 12288000
    port = sys.argv[2] if len(sys.argv) > 2 else find_port()
    if not port:
        sys.exit("no /dev/cu.usbmodem* found")
    ser = serial.Serial(port, 115200, timeout=0.4)
    time.sleep(0.2)
    cmd(ser, "")  # flush any prompt
    print(f"port: {port}")
    print("version:\n  " + cmd(ser, "version").strip().replace("\n", "\n  "))
    # Park at a single CW frequency: start == stop. `cw` if available, else sweep.
    cmd(ser, f"cw {freq}")
    cmd(ser, f"sweep {freq} {freq} 101")
    print("sweep state: " + cmd(ser, "sweep").strip())
    ser.close()
    print(f"\n>>> NanoVNA CH0 now parked at CW {freq/1e6:.6f} MHz")


if __name__ == "__main__":
    main()
