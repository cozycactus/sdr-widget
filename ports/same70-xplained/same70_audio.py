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


def run_serial(port, command, timeout):
    print("+ serial " + command, flush=True)
    output = send_serial_command(port, command, timeout)
    print(output, end="" if output.endswith("\n") else "\n")
    return output


def run_probe(probe, device, seconds, rate, bits, runs, verify, extra_args=None):
    cmd = [
        probe,
        "--device", device,
        "--seconds", str(seconds),
        "--runs", str(runs),
        "--rate", str(rate),
        "--bits", str(bits),
        "--verify", verify,
    ]
    if extra_args:
        cmd.extend(extra_args)
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


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


def require_serial_state(output, source=None, fmt=None, baseline=None,
                         allow_underflow_restart=False):
    fields = parse_serial_fields(output)
    failures = []
    under_delta = 0
    err_delta = 0

    if baseline is not None:
        under_start = parse_counter(baseline, "under")
        under_current = parse_counter(fields, "under")
        err_start = parse_counter(baseline, "err")
        err_current = parse_counter(fields, "err")
        if (under_start is not None) and (under_current is not None):
            under_delta = max(0, under_current - under_start)
        if (err_start is not None) and (err_current is not None):
            err_delta = max(0, err_current - err_start)

    if fields.get("config") != "1":
        failures.append("config is not 1")
    if (source is not None) and (fields.get("source") != source):
        failures.append(f"source is not {source}")
    if (fmt is not None) and (fields.get("fmt") != fmt):
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
                if allow_underflow_restart and key == "under":
                    continue
                if allow_underflow_restart and key == "err" and err_delta <= under_delta:
                    continue
                failures.append(f"{key} increased from {start} to {current}")

    if failures:
        for failure in failures:
            print("serial status check failed: " + failure, file=sys.stderr)
        return False
    return True


def format_name(rate, bits):
    if (rate == 48000) and (bits == 24):
        return "48k24"
    if (rate == 44100) and (bits == 16):
        return "44k16"
    return None
