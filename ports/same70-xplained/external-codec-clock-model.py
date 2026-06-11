#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path


USB_HS_MICROFRAMES_PER_SECOND = 8000
USB_HS_FEEDBACK_FRAC_BITS = 16

CS4272_MASTER_RATIOS_WITH_64FS_BCLK = {
    "single": (256, 384, 512, 768),
    "double": (128, 192, 256, 384),
    "quad": (128, 192),
}

CS4272_TARGET_USB_BITS = (16, 18, 20, 24)

CS4272_STANDARD_USB_RATE_PLANS = (
    {
        "family": "48k",
        "speed": "single",
        "rate": 8000,
        "mclk_hz": 6144000,
        "mclk_lrck_ratio": 768,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
        "clock_source": "family_programmable",
    },
    {
        "family": "44k1",
        "speed": "single",
        "rate": 11025,
        "mclk_hz": 8467200,
        "mclk_lrck_ratio": 768,
        "xo_enable": "xo_44_en",
        "xo_disable": "xo_48_en",
        "clock_source": "family_programmable",
    },
    {
        "family": "48k",
        "speed": "single",
        "rate": 12000,
        "mclk_hz": 9216000,
        "mclk_lrck_ratio": 768,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
        "clock_source": "family_programmable",
    },
    {
        "family": "48k",
        "speed": "single",
        "rate": 16000,
        "mclk_hz": 12288000,
        "mclk_lrck_ratio": 768,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
    },
    {
        "family": "44k1",
        "speed": "single",
        "rate": 22050,
        "mclk_hz": 11289600,
        "mclk_lrck_ratio": 512,
        "xo_enable": "xo_44_en",
        "xo_disable": "xo_48_en",
    },
    {
        "family": "48k",
        "speed": "single",
        "rate": 24000,
        "mclk_hz": 12288000,
        "mclk_lrck_ratio": 512,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
    },
    {
        "family": "48k",
        "speed": "single",
        "rate": 32000,
        "mclk_hz": 12288000,
        "mclk_lrck_ratio": 384,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
    },
    {
        "family": "44k1",
        "speed": "single",
        "rate": 44100,
        "mclk_hz": 11289600,
        "mclk_lrck_ratio": 256,
        "xo_enable": "xo_44_en",
        "xo_disable": "xo_48_en",
        "feedback_symbol": "audio_feedback_44k1_hs",
    },
    {
        "family": "48k",
        "speed": "single",
        "rate": 48000,
        "mclk_hz": 12288000,
        "mclk_lrck_ratio": 256,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
        "feedback_symbol": "audio_feedback_48k_hs",
    },
    {
        "family": "44k1",
        "speed": "double",
        "rate": 88200,
        "mclk_hz": 11289600,
        "mclk_lrck_ratio": 128,
        "xo_enable": "xo_44_en",
        "xo_disable": "xo_48_en",
    },
    {
        "family": "48k",
        "speed": "double",
        "rate": 96000,
        "mclk_hz": 12288000,
        "mclk_lrck_ratio": 128,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
    },
    {
        "family": "44k1",
        "speed": "quad",
        "rate": 176400,
        "mclk_hz": 22579200,
        "mclk_lrck_ratio": 128,
        "xo_enable": "xo_44_en",
        "xo_disable": "xo_48_en",
    },
    {
        "family": "48k",
        "speed": "quad",
        "rate": 192000,
        "mclk_hz": 24576000,
        "mclk_lrck_ratio": 128,
        "xo_enable": "xo_48_en",
        "xo_disable": "xo_44_en",
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
    family = plan["family"]
    speed = plan["speed"]
    mclk_hz = plan["mclk_hz"]
    mclk_ratio = plan["mclk_lrck_ratio"]
    bclk_ratio = 64
    bclk_hz = rate * bclk_ratio

    if mclk_hz % rate != 0:
        raise ValueError(f"{rate} Hz MCLK is not an integer multiple of Fs")
    if (mclk_hz // rate) != mclk_ratio:
        raise ValueError(f"{rate} Hz MCLK ratio does not match the plan")
    ratio_is_codec_standard = mclk_ratio in CS4272_MASTER_RATIOS_WITH_64FS_BCLK[speed]
    if not ratio_is_codec_standard:
        raise ValueError(f"{rate} Hz {speed} speed ratio {mclk_ratio} cannot use 64fs BCLK")
    if mclk_hz % bclk_hz != 0:
        raise ValueError(f"{rate} Hz MCLK is not an integer multiple of BCLK")

    feedback_value = hs_feedback_value(rate)
    feedback_bytes = little_endian_u32(feedback_value)
    symbol = plan.get("feedback_symbol")
    firmware_status = "planned"
    if symbol is not None:
        firmware_bytes = arrays.get(symbol)
        if firmware_bytes != feedback_bytes:
            raise ValueError(
                f"{symbol} is {format_bytes(firmware_bytes or ())}, "
                f"expected {format_bytes(feedback_bytes)}"
            )
        firmware_status = "ok"
    else:
        symbol = "planned_descriptor_feedback_tbd"

    print(
        "clock_model=CS4272_STANDARD_RATE_MATRIX "
        f"selected_family={family} speed={speed} rate={rate} "
        f"usb_bits={','.join(str(bit) for bit in CS4272_TARGET_USB_BITS)} "
        f"mclk_hz={mclk_hz} bclk_hz={bclk_hz} lrck_hz={rate} "
        f"clock_source={plan.get('clock_source', 'fixed_family_xo')} "
        f"xo_enable={plan['xo_enable']} xo_disable={plan['xo_disable']} "
        f"mclk_div_bclk={mclk_hz // bclk_hz} mclk_div_lrck={mclk_ratio} "
        f"codec_standard_ratio={int(ratio_is_codec_standard)} "
        f"feedback_hs_16_16=0x{feedback_value:08x} "
        f"feedback_bytes={format_bytes(feedback_bytes)} "
        f"firmware_symbol={symbol} firmware={firmware_status}"
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
        for plan in CS4272_STANDARD_USB_RATE_PLANS:
            verify_plan(plan, arrays)
    except (OSError, ValueError) as exc:
        print(f"external-codec-clock-model: {exc}", file=sys.stderr)
        return 1

    print("external-codec-clock-model: pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
