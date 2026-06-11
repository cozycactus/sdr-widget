#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path


USB_HS_MICROFRAMES_PER_SECOND = 8000
USB_HS_FEEDBACK_FRAC_BITS = 16

CS4272_MASTER_256FS_PLANS = (
    {
        "rate": 44100,
        "bits": 16,
        "mclk_hz": 11289600,
        "bclk_ratio": 64,
        "feedback_symbol": "audio_feedback_44k1_hs",
    },
    {
        "rate": 48000,
        "bits": 24,
        "mclk_hz": 12288000,
        "bclk_ratio": 64,
        "feedback_symbol": "audio_feedback_48k_hs",
    },
)


def hs_feedback_value(rate):
    return (rate << USB_HS_FEEDBACK_FRAC_BITS) // USB_HS_MICROFRAMES_PER_SECOND


def little_endian_u32(value):
    return tuple((value >> (8 * index)) & 0xff for index in range(4))


def format_bytes(values):
    return " ".join(f"{value:02x}" for value in values)


def parse_uint8_arrays(path):
    text = path.read_text(encoding="utf-8")
    arrays = {}
    pattern = re.compile(r"static const uint8_t\s+(\w+)\[\]\s*=\s*\{([^}]+)\};")
    for match in pattern.finditer(text):
        name = match.group(1)
        if not name.startswith("audio_feedback_"):
            continue
        values = []
        for raw_value in match.group(2).split(","):
            raw_value = raw_value.strip()
            if raw_value:
                values.append(int(raw_value, 0))
        arrays[name] = tuple(values)
    return arrays


def verify_plan(plan, arrays):
    rate = plan["rate"]
    bits = plan["bits"]
    mclk_hz = plan["mclk_hz"]
    bclk_ratio = plan["bclk_ratio"]
    bclk_hz = rate * bclk_ratio
    mclk_ratio = mclk_hz // rate

    if mclk_hz % rate != 0:
        raise ValueError(f"{rate} Hz MCLK is not an integer multiple of Fs")
    if mclk_ratio != 256:
        raise ValueError(f"{rate} Hz MCLK ratio is {mclk_ratio}, expected 256")
    if mclk_hz % bclk_hz != 0:
        raise ValueError(f"{rate} Hz MCLK is not an integer multiple of BCLK")

    feedback_value = hs_feedback_value(rate)
    feedback_bytes = little_endian_u32(feedback_value)
    symbol = plan["feedback_symbol"]
    firmware_bytes = arrays.get(symbol)
    if firmware_bytes != feedback_bytes:
        raise ValueError(
            f"{symbol} is {format_bytes(firmware_bytes or ())}, "
            f"expected {format_bytes(feedback_bytes)}"
        )

    print(
        "clock_model=CS4272_MASTER_256FS "
        f"rate={rate} bits={bits} "
        f"mclk_hz={mclk_hz} bclk_hz={bclk_hz} lrck_hz={rate} "
        f"mclk_div_bclk={mclk_hz // bclk_hz} mclk_div_lrck={mclk_ratio} "
        f"feedback_hs_16_16=0x{feedback_value:08x} "
        f"feedback_bytes={format_bytes(feedback_bytes)} "
        f"firmware_symbol={symbol} firmware=ok"
    )


def main():
    parser = argparse.ArgumentParser(
        description="Verify external CS4272 clock ratios and current USB HS feedback bytes."
    )
    parser.add_argument(
        "--firmware",
        default="same70_usb.c",
        type=Path,
        help="Path to same70_usb.c containing feedback byte arrays.",
    )
    args = parser.parse_args()

    try:
        arrays = parse_uint8_arrays(args.firmware)
        for plan in CS4272_MASTER_256FS_PLANS:
            verify_plan(plan, arrays)
    except (OSError, ValueError) as exc:
        print(f"external-codec-clock-model: {exc}", file=sys.stderr)
        return 1

    print("external-codec-clock-model: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
