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
serial output word at each `LE` pulse, and fails on mismatch.

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
