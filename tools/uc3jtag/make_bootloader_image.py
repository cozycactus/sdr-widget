#!/usr/bin/env python3
"""Merge the AT32UC3A3 USB DFU bootloader (bottom 8 KB) with an application hex
into one image for JTAG flashing, so the board boots the bootloader and DFU works.

    python3 make_bootloader_image.py [app.hex] [out.hex]
    # defaults: ../../Release/widget.hex -> bootloader/merged_boot_widget.hex

The bootloader owns 0x80000000-0x80001FFF; the app's trampoline in that range is
dropped (the bootloader hands off to program_start at 0x80002000). Then flash:

    python3 uc3jtag.py program bootloader/merged_boot_widget.hex
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import uc3jtag as u  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BOOT = os.path.join(HERE, "bootloader", "at32uc3a3-isp-1.0.3.hex")
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
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")


def main():
    app = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "Release", "widget.hex")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "bootloader", "merged_boot_widget.hex")
    mem = {}
    for addr, data in u.parse_ihex(app):
        for i, b in enumerate(data):
            if addr + i >= APP_BASE:
                mem[addr + i] = b
    for addr, data in u.parse_ihex(BOOT):
        for i, b in enumerate(data):
            mem[addr + i] = b
    emit_ihex(out, mem)
    print(f"wrote {out}: {os.path.basename(BOOT)} + {app} (app >= 0x{APP_BASE:08X})")


if __name__ == "__main__":
    main()
