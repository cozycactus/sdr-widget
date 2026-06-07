#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys

from same70_audio import (
    DEFAULT_DEVICE_NAME,
    find_serial_port,
    parse_serial_fields,
    require_audio_outdiag,
    require_serial_state,
    run_probe,
    run_serial,
)


SILENT_OUTPUT_ARGS = ["--silent-output"]


def run_checked_probe(args, port, rate, bits, runs, verify, extra_args=None, seconds=None):
    duration = args.seconds if seconds is None else seconds
    if not args.skip_outdiag_check:
        run_serial(port, "audio outdiag reset", args.serial_timeout)
    run_probe(args.probe, args.device, duration, rate, bits, runs, verify, extra_args)
    if not args.skip_outdiag_check:
        output = run_serial(port, "audio outdiag", args.serial_timeout)
        expect_nonzero = (extra_args is None) or ("--silent-output" not in extra_args)
        label = f"{verify} {rate} Hz {bits}-bit"
        if not require_audio_outdiag(output, expect_nonzero, label):
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
    parser.add_argument("--tone-runs", type=int, default=1)
    parser.add_argument("--sine-runs", type=int, default=1)
    parser.add_argument("--melody-runs", type=int, default=1)
    parser.add_argument("--silence-runs", type=int, default=1)
    parser.add_argument("--skip-loopback", action="store_true")
    parser.add_argument("--skip-pattern", action="store_true")
    parser.add_argument("--skip-tone", action="store_true")
    parser.add_argument("--skip-sine", action="store_true")
    parser.add_argument("--skip-melody", action="store_true")
    parser.add_argument("--skip-silence", action="store_true")
    parser.add_argument("--skip-final-loopback", action="store_true")
    parser.add_argument("--skip-outdiag-check", action="store_true")
    parser.add_argument("--skip-serial-counter-check", action="store_true")
    parser.add_argument("--strict-serial-counter-check", action="store_true")
    args = parser.parse_args()

    port = find_serial_port(args.serial)
    if port is None:
        print("no /dev/cu.usbmodem* serial port found", file=sys.stderr)
        return 1
    if not os.path.exists(args.probe):
        print(f"probe not found: {args.probe}", file=sys.stderr)
        return 1

    try:
        baseline_fields = None
        if not args.skip_serial_counter_check:
            baseline_status = run_serial(port, "usb", args.serial_timeout)
            baseline_fields = parse_serial_fields(baseline_status)
        expected_source = None
        expected_fmt = None

        if not args.skip_loopback:
            run_serial(port, "audio loop", args.serial_timeout)
            if not run_checked_probe(args, port, 48000, 24, args.loopback_runs, "loopback"):
                return 1
            if not run_checked_probe(args, port, 44100, 16, args.loopback_runs, "loopback"):
                return 1
            expected_source = "loop"
            expected_fmt = "44k16/44k16"

        if not args.skip_pattern:
            run_serial(port, "audio pattern", args.serial_timeout)
            if not run_checked_probe(
                    args, port, 48000, 24, args.pattern_runs, "pattern",
                    SILENT_OUTPUT_ARGS):
                return 1
            if not run_checked_probe(
                    args, port, 44100, 16, args.pattern_runs, "pattern",
                    SILENT_OUTPUT_ARGS):
                return 1
            expected_source = "pattern"
            expected_fmt = "44k16/44k16"

        if not args.skip_tone:
            run_serial(port, "audio tone", args.serial_timeout)
            if not run_checked_probe(
                    args, port, 48000, 24, args.tone_runs, "tone",
                    SILENT_OUTPUT_ARGS):
                return 1
            if not run_checked_probe(
                    args, port, 44100, 16, args.tone_runs, "tone",
                    SILENT_OUTPUT_ARGS):
                return 1
            expected_source = "tone"
            expected_fmt = "44k16/44k16"

        if not args.skip_sine:
            run_serial(port, "audio sine", args.serial_timeout)
            if not run_checked_probe(
                    args, port, 48000, 24, args.sine_runs, "sine",
                    SILENT_OUTPUT_ARGS):
                return 1
            if not run_checked_probe(
                    args, port, 44100, 16, args.sine_runs, "sine",
                    SILENT_OUTPUT_ARGS):
                return 1
            expected_source = "sine"
            expected_fmt = "44k16/44k16"

        if not args.skip_melody:
            run_serial(port, "audio melody", args.serial_timeout)
            if not run_checked_probe(
                    args, port, 48000, 24, args.melody_runs, "melody",
                    SILENT_OUTPUT_ARGS):
                return 1
            if not run_checked_probe(
                    args, port, 44100, 16, args.melody_runs, "melody",
                    SILENT_OUTPUT_ARGS):
                return 1
            expected_source = "melody"
            expected_fmt = "44k16/44k16"

        if not args.skip_silence:
            run_serial(port, "audio silence", args.serial_timeout)
            if not run_checked_probe(
                    args, port, 48000, 24, args.silence_runs, "silence",
                    SILENT_OUTPUT_ARGS):
                return 1
            if not run_checked_probe(
                    args, port, 44100, 16, args.silence_runs, "silence",
                    SILENT_OUTPUT_ARGS):
                return 1
            expected_source = "silence"
            expected_fmt = "44k16/44k16"

        if not args.skip_final_loopback:
            run_serial(port, "audio loop", args.serial_timeout)
            if not run_checked_probe(args, port, 48000, 24, 1, "loopback", seconds=1.0):
                return 1
            expected_source = "loop"
            expected_fmt = "48k24/48k24"

        status = run_serial(port, "usb", args.serial_timeout)
        if not require_serial_state(
            status, expected_source, expected_fmt, baseline_fields,
            allow_underflow_restart=not args.strict_serial_counter_check):
            return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode

    print("audio-verify: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
