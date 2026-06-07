#!/usr/bin/env python3
import argparse
import glob
import os
import select
import subprocess
import sys
import termios
import time


DEFAULT_DEVICE_NAME = "Yoyodyne SDR-Widget"
DEFAULT_SERIAL_PORT = "/dev/cu.usbmodem1462302"


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
        if ("source=loop" not in status) or ("fmt=48k24/48k24" not in status):
            print("final serial status did not report loop 48k24/48k24", file=sys.stderr)
            return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode

    print("audio-verify: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
