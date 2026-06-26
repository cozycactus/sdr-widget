#!/usr/bin/env python3
"""Merge a local AT32UC3A3 USB DFU bootloader hex with an application hex
into one image for JTAG flashing, so the board boots the bootloader and DFU works.

    python3 make_bootloader_image.py [app.hex] [out.hex] --bootloader BOOT.hex
    # app/out defaults: ../../Release/widget.hex -> bootloader/merged_boot_widget.hex

The bootloader owns 0x80000000-0x80001FFF; the app's trampoline in that range is
dropped (the bootloader hands off to program_start at 0x80002000). Then flash:

    python3 uc3jtag.py program --erase-all --program-user-page bootloader/merged_boot_widget.hex
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uc3jtag as u  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_APP = os.path.join(HERE, "..", "..", "Release", "widget.hex")
DEFAULT_OUT = os.path.join(HERE, "bootloader", "merged_boot_widget.hex")
DEFAULT_BOOT = os.path.join(HERE, "bootloader", "at32uc3a3-isp-1.0.3.hex")
APP_BASE = 0x80002000   # bootloader owns everything below this


def emit_ihex(path, mem):
    addrs = sorted(mem)
    out = []
    upper = None
    i, n = 0, len(addrs)
    while i < n:
        start = addrs[i]
        run = [mem[start]]
        j = i + 1
        while (j < n and addrs[j] == addrs[j - 1] + 1 and len(run) < 16
               and (addrs[j] >> 16) == (start >> 16)):
            run.append(mem[addrs[j]])
            j += 1
        hi = start >> 16
        if hi != upper:
            upper = hi
            d = bytes([2, 0, 0, 4, (hi >> 8) & 0xFF, hi & 0xFF])
            out.append(":02000004%04X%02X" % (hi, (-sum(d)) & 0xFF))
        lo = start & 0xFFFF
        d = bytes([len(run), (lo >> 8) & 0xFF, lo & 0xFF, 0] + run)
        out.append(":%02X%04X00%s%02X"
                   % (len(run), lo, bytes(run).hex().upper(), (-sum(d)) & 0xFF))
        i = j
    out.append(":00000001FF")
    out_dir = os.path.dirname(os.path.abspath(path))
    os.makedirs(out_dir, exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")


def build_merged_image(app, boot):
    """Overlay a bootloader HEX onto an app HEX.

    The bootloader owns [FLASH_BASE, APP_BASE); the app owns [APP_BASE, ...).
    App records in the bootloader window are dropped (the bootloader provides
    its own reset trampoline). To avoid silent corruption:
      - app bytes below APP_BASE but outside the bootloader window are rejected;
      - bootloader bytes at/above APP_BASE are rejected (they'd clobber the app).
    """
    mem = {}
    dropped_app_bytes = 0
    for addr, data in u.parse_ihex(app):
        for i, b in enumerate(data):
            a = addr + i
            if a >= APP_BASE:
                mem[a] = b
            elif u.FLASH_BASE <= a < APP_BASE:
                dropped_app_bytes += 1          # bootloader window: bootloader wins
            else:
                raise u.OpenOCDError(
                    f"app byte at 0x{a:08X} is below the bootloader window "
                    f"(0x{u.FLASH_BASE:08X}); refusing to merge")
    for addr, data in u.parse_ihex(boot):
        for i, b in enumerate(data):
            a = addr + i
            if a >= APP_BASE:
                raise u.OpenOCDError(
                    f"bootloader byte at 0x{a:08X} is at/above APP_BASE "
                    f"(0x{APP_BASE:08X}); it would overwrite the application")
            mem[a] = b
    return mem, dropped_app_bytes


def main():
    parser = argparse.ArgumentParser(
        description="Merge a local UC3A3 DFU bootloader HEX with an application HEX.")
    parser.add_argument("app", nargs="?", default=DEFAULT_APP,
                        help="application Intel HEX")
    parser.add_argument("out", nargs="?", default=DEFAULT_OUT,
                        help="output merged Intel HEX")
    parser.add_argument("--bootloader",
                        default=os.environ.get("UC3_BOOTLOADER_HEX", DEFAULT_BOOT),
                        help="local AT32UC3A3 DFU bootloader Intel HEX")
    args = parser.parse_args()

    if not os.path.exists(args.bootloader):
        parser.error(
            "bootloader HEX not found; pass --bootloader PATH or set "
            "UC3_BOOTLOADER_HEX")

    mem, dropped = build_merged_image(args.app, args.bootloader)
    if not mem:
        parser.error("merged image is empty (no flash data from app or bootloader)")
    emit_ihex(args.out, mem)
    print(f"wrote {args.out}: {os.path.basename(args.bootloader)} + {args.app} "
          f"(app >= 0x{APP_BASE:08X})")
    if dropped:
        print(f"dropped {dropped} app byte(s) below 0x{APP_BASE:08X}")


if __name__ == "__main__":
    main()
