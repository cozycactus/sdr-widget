#!/usr/bin/env python3
"""Back up the entire AT32UC3A3256 flash (main 256KB + user page) to an Intel
HEX file via the verified uc3jtag read path. Read-only; does not modify flash."""
import sys, os, time
HERE = "/Users/ruslanmigirov/cozycactus/sdr-widget/tools/uc3jtag"
sys.path.insert(0, HERE)
import uc3jtag as u
from make_bootloader_image import emit_ihex

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    HERE, "bootloader", "flash_backup_%s.hex" % time.strftime("%Y%m%d_%H%M%S"))

def main():
    mk = lambda: u.OpenOCD()
    # discover flash size
    with mk() as ocd:
        size = u.flashc_flash_size(ocd)
    pages = size // u.PAGE_BYTES
    print("flash size: %d KB (%d pages); reading..." % (size // 1024, pages))

    mem = {}
    GROUP = 16  # restart the OpenOCD session every 16 pages (RPC wedge guard)
    t0 = time.time()
    for start in range(0, pages, GROUP):
        with mk() as ocd:
            for pi in range(start, min(start + GROUP, pages)):
                base = u.FLASH_BASE + pi * u.PAGE_BYTES
                words = u.mwa_read_words(ocd, base, u.PAGE_WORDS)
                for j, w in enumerate(words):
                    a = base + j * 4
                    mem[a] = (w >> 24) & 0xFF
                    mem[a + 1] = (w >> 16) & 0xFF
                    mem[a + 2] = (w >> 8) & 0xFF
                    mem[a + 3] = w & 0xFF
        print("\r  %d/%d pages" % (min(start + GROUP, pages), pages), end="", flush=True)
    print()

    # user page (single 512B page at 0x80800000)
    with mk() as ocd:
        words = u.mwa_read_words(ocd, u.USER_PAGE_ADDR, u.PAGE_WORDS)
    for j, w in enumerate(words):
        a = u.USER_PAGE_ADDR + j * 4
        mem[a] = (w >> 24) & 0xFF
        mem[a + 1] = (w >> 16) & 0xFF
        mem[a + 2] = (w >> 8) & 0xFF
        mem[a + 3] = w & 0xFF

    emit_ihex(OUT, mem)
    # strip all-0xFF trailing pages? No -- keep full image for exact restore.
    nonff = sum(1 for v in mem.values() if v != 0xFF)
    print("wrote %s" % OUT)
    print("  %d bytes total, %d non-0xFF bytes, %.1fs" % (len(mem), nonff, time.time() - t0))
    print("BACKUP_PATH=%s" % OUT)

if __name__ == "__main__":
    main()
