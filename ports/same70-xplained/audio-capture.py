#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys

from same70_audio import (
    DEFAULT_DEVICE_NAME,
    find_serial_port,
    format_name,
    parse_serial_fields,
    require_serial_state,
    run_probe,
    run_serial,
)


VERIFY_MODES = {
    "loop": "loopback",
    "pattern": "pattern",
    "tone": "tone",
    "silence": "silence",
}


def run_wav_verify(path, source, rate, bits):
    script = os.path.join(os.path.dirname(__file__), "audio-wav-verify.py")
    cmd = [
        sys.executable,
        script,
        path,
        "--source", source,
        "--rate", str(rate),
        "--bits", str(bits),
    ]
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def default_output_path(source, rate, bits):
    fmt = format_name(rate, bits)
    if fmt is None:
        fmt = f"{rate}_{bits}"
    return f"/tmp/same70-{source}-{fmt}.wav"


def main():
    parser = argparse.ArgumentParser(description="Capture SAME70 USB audio input to WAV.")
    parser.add_argument("--probe", default="build/coreaudio-stream-probe")
    parser.add_argument("--device", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--serial")
    parser.add_argument("--source", choices=sorted(VERIFY_MODES), default="tone")
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--bits", type=int, default=24)
    parser.add_argument("--seconds", type=float, default=1.0)
    parser.add_argument("--output")
    parser.add_argument("--skip-wav-verify", action="store_true")
    parser.add_argument("--serial-timeout", type=float, default=1.5)
    args = parser.parse_args()

    fmt = format_name(args.rate, args.bits)
    if fmt is None:
        print("supported pairs are 48000/24 and 44100/16", file=sys.stderr)
        return 1

    port = find_serial_port(args.serial)
    if port is None:
        print("no /dev/cu.usbmodem* serial port found", file=sys.stderr)
        return 1
    if not os.path.exists(args.probe):
        print(f"probe not found: {args.probe}", file=sys.stderr)
        return 1

    output = args.output or default_output_path(args.source, args.rate, args.bits)

    try:
        baseline_status = run_serial(port, "usb", args.serial_timeout)
        baseline_fields = parse_serial_fields(baseline_status)

        source_status = run_serial(port, "audio " + args.source, args.serial_timeout)
        if f"source={args.source}" not in source_status:
            print(f"serial status did not report source={args.source}", file=sys.stderr)
            return 1

        run_probe(
            args.probe,
            args.device,
            args.seconds,
            args.rate,
            args.bits,
            1,
            VERIFY_MODES[args.source],
            ["--dump-input-wav", output],
        )
        if args.source != "loop" and not args.skip_wav_verify:
            run_wav_verify(output, args.source, args.rate, args.bits)

        run_serial(port, "audio loop", args.serial_timeout)
        status = run_serial(port, "usb", args.serial_timeout)
        if not require_serial_state(status, "loop", None, baseline_fields):
            return 1
    except subprocess.CalledProcessError as exc:
        return exc.returncode

    print("audio-capture: pass output=" + output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
