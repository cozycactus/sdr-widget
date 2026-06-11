# AD1856 Formatter HDL

This directory contains a small generic-Verilog formatter for the preferred
low-jitter AD1856 mono path.

The intended hardware target is the smallest practical CPLD/FPGA with enough
flip-flops for a few counters and two 16-bit registers. The code is vendor
neutral and is suitable for tiny iCE40-class FPGAs or comparable CPLDs after
pin constraints are added for the exact board.

## Function

The formatter is the external audio-clock master:

```text
XO 11.2896 MHz -> formatter
formatter TK   -> SAME70 TK/PB1
formatter TF   -> SAME70 TF/PB0
SAME70 TD      -> formatter
formatter      -> AD1856 DATA/CLK/LE
```

SAME70 is a slave-TX sample source. The formatter captures the left 16-bit
sample from SAME70, ignores the right slot for the first mono proof, and shifts
the captured sample into AD1856 with a gated 16-pulse clock and a low-going
latch-enable pulse.

## Default Timing

- `clk_xo`: 11.2896 MHz.
- `same_tk`: 2.8224 MHz, generated as `clk_xo / 4`.
- `same_tf`: 44.1 kHz, low for the left 32-bit slot and high for the right
  32-bit slot.
- `ad1856_clk`: gated 16-pulse burst per output sample.
- `ad1856_le`: idle high, then low for two `clk_xo` cycles after the 16th bit.

The default input format is left-justified: the first left-channel bit is valid
on the first `same_tk` rising edge after `same_tf` goes low. If firmware later
uses one-bit-delayed I2S, set `INPUT_FIRST_BIT=1`.

## Simulation

```sh
make -C ports/same70-xplained ad1856-formatter-sim
```

The testbench feeds known left-channel samples on `TD`, checks the AD1856
serial output word at each `LE` pulse, and fails on mismatch. It also checks
the formatter timing:

- `same_tk` stays at `clk_xo / 4`.
- `same_tf` toggles every 32 `same_tk` periods.
- AD1856 `CLK` emits exactly 16 rising edges per word.
- AD1856 `DATA` is stable before each `CLK` rising edge.
- AD1856 `LE` falls only while `CLK` is low and stays low for two `clk_xo`
  cycles.

Generate a waveform dump with:

```sh
make -C ports/same70-xplained ad1856-formatter-vcd
```

The VCD file is written to `build/ad1856_formatter.vcd` inside this directory.

## Toolchain

The local Makefile prefers OSS CAD Suite when it is installed at:

```text
/Users/cozy/cozycactus/oss-cad-suite
```

Override the location with `OSS_CAD_SUITE=/path/to/oss-cad-suite`, or put
compatible `iverilog`, `vvp`, and `yosys` binaries in `PATH`.

Check the bundled tool versions with:

```sh
make -C ports/same70-xplained/hdl/ad1856_formatter tool-versions
```

## Synthesis Sanity Check

Run a board-neutral iCE40 synthesis check with:

```sh
make -C ports/same70-xplained ad1856-formatter-synth
```

This proves the generic Verilog lowers into tiny-FPGA primitives and writes
`build/ad1856_formatter-ice40.json`. It is not a finished bitstream yet:
the exact FPGA/CPLD board still needs pin constraints, voltage checks, and a
place-and-route target.

Run the board-wrapper synthesis check with:

```sh
make -C ports/same70-xplained ad1856-formatter-board-synth
```

The wrapper in `ad1856_formatter_board.v` removes simulation/debug outputs
from the top-level pinout so the first real bitstream only needs pins for
`clk_xo`, `reset_n`, `same_td`, `same_tk`, `same_tf`, and AD1856
`DATA`/`CLK`/`LE`.

## Bitstream Skeleton

The Makefile includes an iCE40 bitstream path, but it intentionally refuses to
run until a real board and pin map are selected:

```sh
make -C ports/same70-xplained ad1856-formatter-bitstream-help
```

Copy `constraints/ad1856_formatter-ice40.pcf.example` to a board-specific PCF,
fill in the actual package pins from the board schematic, then run:

```sh
make -C ports/same70-xplained ad1856-formatter-bitstream \
  ICE40_DEVICE=up5k ICE40_PACKAGE=sg48 PCF=path/to/board.pcf
```

Use the actual `ICE40_DEVICE` and `ICE40_PACKAGE` values for the selected
board; `up5k/sg48` is only an example. The bitstream path uses the board
wrapper, not the debug-heavy simulation top.

## UPduino v3.1 Starter Bitstream

The selected first external formatter board is UPduino v3.1. Its starter PCF is:

```text
constraints/upduino-v3.1-ad1856.pcf.example
```

Build the starter bitstream with:

```sh
make -C ports/same70-xplained ad1856-formatter-upduino-v31-bitstream
```

This uses `ICE40_DEVICE=up5k`, `ICE40_PACKAGE=sg48`, and the board wrapper
top-level. The output is `build/ad1856_formatter-ice40.bin`.
