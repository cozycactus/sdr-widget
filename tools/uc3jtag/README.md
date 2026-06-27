# uc3jtag — flash/debug an AT32UC3A3256 over JTAG with an Atmel-ICE, on macOS

macOS has **no** tool that can program an AVR32 UC3 part with an Atmel-ICE:
OpenOCD only supports the AVR32 *AP7000* (not UC3) and ships no AVR32 flash
driver; `avrdude`/`avarice` are 8-bit AVR only; `atprogram` is Windows-only.
This tool fills that gap so you can recover a board whose DFU bootloader is
gone and JTAG is the only way in.

## How it works

We do **not** reimplement CMSIS-DAP or the JTAG state machine. OpenOCD already
drives this exact Atmel-ICE reliably in CMSIS-DAP + JTAG mode, so we run it as a
dumb transport and speak its **Tcl-RPC** protocol over a local TCP socket,
issuing raw `irscan` / `drscan` / `runtest`. On top of that we implement only
the AVR32-UC3-specific protocol OpenOCD lacks.

```
uc3jtag.py ──Tcl-RPC(socket)──► openocd ──CMSIS-DAP/USB──► Atmel-ICE ──JTAG──► UC3A3
   (UC3 protocol: irscan/drscan)   (transport only)      (AVR or SAM port)
```

Requirements: `openocd` (already installed) and Python 3. `uc3jtag.py` itself
uses only the Python stdlib. `nanovna_clock.py` is optional and additionally
requires `pyserial`.

## Wiring — either Atmel-ICE port works

Per Microchip's docs the Atmel-ICE's two 10-pin connectors are *"directly
electrically connected"* — they carry the **same JTAG signals**
(TCK/TMS/TDI/TDO/nSRST/VTG), only on different pin positions. The
"SAM = CMSIS-DAP / AVR = atprogram" distinction is about the *software protocol*,
not the physical wires, so CMSIS-DAP JTAG reaches the UC3 through the **AVR port
too**. Use whichever connector matches your target board's pinout. (Red ribbon
stripe = pin 1.)

| Signal | AVR port pin | SAM port pin | UC3A3 pin |
|---|---|---|---|
| VTG (reference sense only) | 4 | 1 | VDDIO (3.3 V) |
| TMS | 5 | 2 | TMS |
| TCK | 1 | 4 | TCK |
| TDO | 3 | 6 | TDO |
| TDI | 9 | 8 | TDI |
| nSRST *(optional now)* | 6 | 10 | RESET_N |
| GND | 2, 10 | 3, 5, 9 | GND |

**Power the board from its own supply** — the Atmel-ICE only *senses* VTG, it
does not power the target (same lesson as the SAM E70 attempt).

## Usage

Milestone 1 — read the device IDCODE (the viability gate):

```bash
cd tools/uc3jtag
python3 uc3jtag.py idcode          # add -v to trace the RPC traffic
```

- A real IDCODE (manufacturer decodes to **Atmel/Microchip**, `lsb=1`) ⇒ the
  generic-JTAG-through-CMSIS-DAP approach works and we can build the rest.
- `0x00000000` or `0xFFFFFFFF` ⇒ open / unpowered chain ⇒ check power, wiring, VTG.

Quick check without the Python wrapper:

```bash
openocd -f openocd/uc3a3.cfg -c "init; scan_chain; shutdown"
```

## Roadmap

| # | Command | Status | Notes |
|---|---------|--------|-------|
| 1 | `idcode`        | ✅ **verified on HW** | reads `0x7202603F` (AT32UC3A3) |
| 2 | `did`           | ✅ **verified on HW** | NEXUS reg read; DID == IDCODE |
| 3 | `read`          | ✅ **verified on HW** | MEMORY_WORD_ACCESS; reads flash reliably |
| 4 | `write` (mem)   | ✅ **verified on HW** | MWA write (SRAM read-back OK) |
| 5 | `flashinfo`     | ✅ **verified on HW** | FLASHC base 0xFFFE1400; FSR/size |
| 6 | `halt`/resume   | ✅ **verified on HW** | OCD DC.DBE\|DBR halts; resume needs RETD via DINST (clearing DBR is a no-op) |
| 7 | `erase`         | coded, destructive | ERASE_ALL via FCMD (now halts CPU first) |
| 8 | `program`       | coded | per-page erase+write+verify; `--app-only` preserves the bootloader region, `--erase-all` wipes the whole chip first |
| 9 | `fuses`         | TODO | GP/BOOTPROT fuses + DFU-bootloader restore |

### ⚠️ Lesson learned (the hard way)
The first `erase` ran **without halting the CPU**. The core kept executing from
flash while it was being erased, faulted, and pulled the flash controller into
reset — wedging the SAB (all reads returned `0x00000001`, FSR `FSZ=0`). The TAP
still responded (not bricked), but **recovery required a power cycle**. Fix:
`erase`/`program` now issue an OCD **CPU halt (DC.DBE|DBR)** before touching
flash. `program` erases each page it writes. Note that a normal application HEX
(e.g. `Release/widget.hex`) contains a reset trampoline in the bootloader region
(`0x80000000-0x80001FFF`), so programming it **overwrites any installed
bootloader** there — `program` warns when this will happen. Pass `--app-only` to
drop those records and preserve an existing bootloader, or `--erase-all` only
when you intend to wipe the whole chip first (clean slate / clear stale pages).

The NEXUS / Memory-Word-Access scans are ported verbatim from OpenOCD's
`src/target/avr32_jtag.c` (the bit-field layouts are reproduced in
`uc3jtag.py`). Remaining milestones need the FLASHC programming sequence and the
CHIP_ERASE/AVR_RESET opcodes from the AVR32 UC3 datasheet "Programming and
Debugging" chapter. Each is gated on the previous passing on real hardware.

## Status

Validated end-to-end on real hardware (Atmel-ICE **AVR port** → AT32UC3A3256):
IDCODE, OCD register read via NEXUS, and system-bus reads via
MEMORY_WORD_ACCESS all work from macOS. Diagnostic finding: the **top of flash
(bootloader region) reads `0xFFFFFFFF`** — the DFU bootloader has been erased,
which is why DFU no longer enumerates. Next phase: chip-erase + FLASHC flash
programming to reflash the application (and optionally restore the bootloader).
