#!/usr/bin/env python3
import argparse
import shlex
import subprocess
import sys
import time


DEFAULT_DEVICE = "16c0:05dc"
DEFAULT_SERIAL = "1.0.0.0.0.0.0"
DEFAULT_FEATURES = "10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms"

EXPECTED_LISTING = [
    "major 10",
    "minor 37",
    "board = none widget usbi2s usbdac test",
    "image = flashyblinky uac1_audio uac1_dg8saq uac2_audio uac2_dg8saq hpsdr test",
    "in = normal swapped",
    "out = normal swapped",
    "adc = none ak5394a",
    "dac = none cs4344 es9022",
    "lcd = none hd44780 ks0073",
    "log = none 250ms 500ms 1sec 2sec",
]


def collapse(text):
    return " ".join(text.split())


def format_command(command):
    return " ".join(shlex.quote(part) for part in command)


def run_control(args, option, extra_args=None, timeout=None, check=True, log_failure=True):
    command = [args.widget_control]
    if args.serial:
        command.extend(["-u", args.serial])
    command.append(option)
    if extra_args:
        command.extend(extra_args)
    print(f"+ {format_command(command)}", flush=True)
    completed = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout or args.timeout,
        check=False,
    )
    if completed.stdout:
        print(completed.stdout, end="" if completed.stdout.endswith("\n") else "\n")
    if completed.stderr and (log_failure or completed.returncode == 0):
        print(completed.stderr, end="" if completed.stderr.endswith("\n") else "\n", file=sys.stderr)
    if check and completed.returncode != 0:
        print(f"widget-verify: {option} failed with exit {completed.returncode}", file=sys.stderr)
        return None
    return completed.stdout


def require(condition, message):
    if not condition:
        print(f"widget-verify: {message}", file=sys.stderr)
        return False
    return True


def verify_feature_line(label, output, expected):
    lines = [collapse(line) for line in output.splitlines() if line.strip()]
    if not require(len(lines) == 1, f"{label} returned {len(lines)} feature lines"):
        return False
    if not require(lines[0] == expected, f"{label} mismatch: got '{lines[0]}', expected '{expected}'"):
        return False
    return True


def verify_listing(output):
    lines = {collapse(line) for line in output.splitlines() if line.strip()}
    for expected in EXPECTED_LISTING:
        if not require(expected in lines, f"feature listing missing '{expected}'"):
            return False
    return True


def wait_for_device(args):
    deadline = time.monotonic() + args.reset_timeout
    while time.monotonic() < deadline:
        output = run_control(args, "-d", timeout=args.timeout, check=False, log_failure=False)
        if output is not None and collapse(output) == collapse(args.expected_features):
            return True
        time.sleep(0.5)
    print("widget-verify: device did not return expected defaults after reset", file=sys.stderr)
    return False


def main():
    parser = argparse.ArgumentParser(description="Verify SAME70 SDR Widget USB control requests.")
    parser.add_argument("--widget-control", default="../../widget-control")
    parser.add_argument("--expected-device", default=DEFAULT_DEVICE)
    parser.add_argument("--expected-serial", default=DEFAULT_SERIAL)
    parser.add_argument("--expected-features", default=DEFAULT_FEATURES)
    parser.add_argument("--serial", help="Pass a specific -u serialId to widget-control.")
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--reset-check", action="store_true",
                        help="Also exercise widget-control -r and wait for feature readback.")
    parser.add_argument("--set-current-check", action="store_true",
                        help="Also send the current feature values through widget-control -s.")
    parser.add_argument("--reset-timeout", type=float, default=10.0)
    args = parser.parse_args()

    expected_features = collapse(args.expected_features)
    output = run_control(args, "-a")
    if output is None:
        return 1
    device_lines = [line.strip() for line in output.splitlines() if line.strip()]
    expected_device_line = f"{args.expected_device} {args.expected_serial}"
    if not require(expected_device_line in device_lines, f"enumeration missing '{expected_device_line}'"):
        return 1

    outputs = {}
    for option, label in (("-d", "defaults"), ("-g", "nvram"), ("-m", "ram")):
        output = run_control(args, option)
        if output is None or not verify_feature_line(label, output, expected_features):
            return 1
        outputs[label] = collapse(output)
    if not require(outputs["defaults"] == outputs["nvram"] == outputs["ram"],
                   "default, nvram, and ram feature values differ"):
        return 1

    output = run_control(args, "-l")
    if output is None or not verify_listing(output):
        return 1

    if args.set_current_check:
        output = run_control(args, "-s", extra_args=outputs["nvram"].split())
        if output is None:
            return 1
        output = run_control(args, "-g")
        if output is None or not verify_feature_line("nvram after set-current", output, expected_features):
            return 1

    if args.reset_check:
        output = run_control(args, "-r")
        if output is None:
            return 1
        if not wait_for_device(args):
            return 1

    print("widget-verify: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
