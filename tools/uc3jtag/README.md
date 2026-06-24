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

Requirements: `openocd` (already installed) and Python 3 (stdlib only — **no
pip packages**).

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

| # | Command | Status | Needs |
|---|---------|--------|-------|
| 1 | `idcode`  | ✅ implemented | hardware wiring to verify |
| 2 | `halt`    | TODO | AVR32 JTAG opcodes (AVR_RESET, HALT) |
| 3 | memory    | TODO | NEXUS / MEMORY_WORD_ACCESS over JTAG |
| 4 | `erase`   | TODO | JTAG CHIP_ERASE opcode |
| 5 | `program` | TODO | FLASHC page-buffer write sequence |
| 6 | `fuses`   | TODO | GP/BOOTPROT fuses + DFU-bootloader restore |

Milestones 2–6 are protocol work on the same transport; opcodes/sequences come
from the AVR32 UC3 datasheet "Programming and Debugging" chapter. Each is gated
on the previous one passing on real hardware.

## Status

The full software path is validated end-to-end (launch OpenOCD → RPC connect →
`scan_chain` → decode IDCODE → graceful failure on an open chain). What remains
before milestone 1 "passes" is purely physical: wire either Atmel-ICE port to the UC3
JTAG pins and power the board.
