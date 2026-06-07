#!/usr/bin/env python3
import argparse
import glob
import os
import re
import select
import subprocess
import sys
import termios
import time


DEFAULT_DEVICE_NAME = "Yoyodyne SDR-Widget"
DEFAULT_SERIAL_PORT = "/dev/cu.usbmodem1462302"
SERIAL_COUNTERS = ("stall", "err", "crc", "over", "under", "drop")


def find_serial_port(requested):
    if requested:
        return requested
    env_port = os.environ.get("SAME70_SERIAL_PORT")
    if env_port:
        return env_port
    if os.path.exists(DEFAULT_SERIAL_PORT):
        return DEFAULT_SERIAL_PORT
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    return ports[0] if ports else None


def configure_serial(fd):
    attrs = termios.tcgetattr(fd)
    attrs[4] = termios.B9600
    attrs[5] = termios.B9600
    attrs[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[2] &= ~(termios.PARENB | termios.CSTOPB)
    attrs[3] &= ~(termios.ECHO | termios.ICANON)
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def send_serial_command(port, command, timeout):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    data = b""
    try:
        configure_serial(fd)
        time.sleep(0.2)
        os.write(fd, (command + "\r").encode("ascii"))
        deadline = time.time() + timeout
        while time.time() < deadline:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if fd in ready:
                try:
                    chunk = os.read(fd, 4096)
                except BlockingIOError:
                    continue
                if chunk:
                    data += chunk
    finally:
        os.close(fd)
    return data.decode("latin1", errors="replace")


def run_probe(probe, device, seconds, rate, bits, runs, verify):
    cmd = [
        probe,
        "--device", device,
        "--seconds", str(seconds),
        "--runs", str(runs),
        "--rate", str(rate),
        "--bits", str(bits),
        "--verify", verify,
    ]
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def run_serial(port, command, timeout):
    print("+ serial " + command, flush=True)
    output = send_serial_command(port, command, timeout)
    print(output, end="" if output.endswith("\n") else "\n")
    return output


def parse_serial_fields(output):
    fields = {}

    for key, value in re.findall(r"\b([A-Za-z_]+)=([^\s]+)", output):
        fields[key] = value
    return fields


def parse_counter(fields, key):
    value = fields.get(key)

    if value is None:
        return None
    return int(value.split("/", 1)[0], 0)


def require_serial_state(output, source, fmt, baseline=None):
    fields = parse_serial_fields(output)
    failures = []

    if fields.get("config") != "1":
        failures.append("config is not 1")
    if fields.get("source") != source:
        failures.append(f"source is not {source}")
    if fields.get("fmt") != fmt:
        failures.append(f"fmt is not {fmt}")
    for key in SERIAL_COUNTERS:
        current = parse_counter(fields, key)
        if current is None:
            failures.append(f"missing {key}")
            continue
        if baseline is not None:
            start = parse_counter(baseline, key)
            if start is None:
                failures.append(f"missing baseline {key}")
            elif current > start:
                failures.append(f"{key} increased from {start} to {current}")

    if failures:
        for failure in failures:
            print("serial status check failed: " + failure, file=sys.stderr)
        return False
    return True


def main():
    parser = argparse.ArgumentParser(description="Run SAME70 USB audio verification.")
    parser.add_argument("--probe", default="build/coreaudio-stream-probe")
    parser.add_argument("--device", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--serial")
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--serial-timeout", type=float, default=1.5)
    parser.add_argument("--loopback-runs", type=int, default=2)
    parser.add_argument("--pattern-runs", type=int, default=2)
    parser.add_argument("--skip-loopback", action="store_true")
    parser.add_argument("--skip-pattern", action="store_true")
    args = parser.parse_args()

    port = find_serial_port(args.serial)
    if port is None:
        print("no /dev/cu.usbmodem* serial port found", file=sys.stderr)
        return 1
    if not os.path.exists(args.probe):
        print(f"probe not found: {args.probe}", file=sys.stderr)
        return 1

    try:
        baseline_status = run_serial(port, "usb", args.serial_timeout)
        baseline_fields = parse_serial_fields(baseline_status)

        if not args.skip_loopback:
            run_serial(port, "audio loop", args.serial_timeout)
            run_probe(args.probe, args.device, args.seconds, 48000, 24, args.loopback_runs, "loopback")
            run_probe(args.probe, args.device, args.seconds, 44100, 16, args.loopback_runs, "loopback")

        if not args.skip_pattern:
            run_serial(port, "audio pattern", args.serial_timeout)
            run_probe(args.probe, args.device, args.seconds, 48000, 24, args.pattern_runs, "pattern")
            run_probe(args.probe, args.device, args.seconds, 44100, 16, args.pattern_runs, "pattern")

        run_serial(port, "audio loop", args.serial_timeout)
        run_probe(args.probe, args.device, 1.0, 48000, 24, 1, "loopback")
        status = run_serial(port, "usb", args.serial_timeout)
        if not require_serial_state(status, "loop", "48k24/48k24", baseline_fields):
            return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode

    print("audio-verify: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
