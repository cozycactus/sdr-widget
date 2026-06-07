#!/usr/bin/env python3
import argparse
import sys

from same70_audio import find_serial_port, run_serial


def main():
    parser = argparse.ArgumentParser(description="Run one SAME70 serial console command.")
    parser.add_argument("command")
    parser.add_argument("--serial")
    parser.add_argument("--timeout", type=float, default=1.5)
    parser.add_argument("--expect", action="append", default=[])
    args = parser.parse_args()

    port = find_serial_port(args.serial)
    if port is None:
        print("no /dev/cu.usbmodem* serial port found", file=sys.stderr)
        return 1

    output = run_serial(port, args.command, args.timeout)
    for expected in args.expect:
        if expected not in output:
            print(f"serial output missing expected text: {expected}", file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
