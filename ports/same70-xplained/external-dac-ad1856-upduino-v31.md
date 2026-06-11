# AD1856 Formatter UPduino v3.1 Pin Plan

This is the selected first FPGA board plan for the low-jitter AD1856 mono
formatter. The board is UPduino v3.1 with a Lattice iCE40UP5K FPGA and the
OSS CAD Suite iCE40 flow.

This does not change the stock SAME70 firmware boundary: until hardware is
wired and verified, firmware must still report `external_codec=0` and must not
claim analog SDR output.

## Why This Board

- UPduino v3.1 is small and inexpensive.
- It has an iCE40UP5K, enough logic by a large margin for the formatter.
- It has an on-board FTDI programmer, so no separate programmer is required for
  the first bring-up.
- Its FPGA I/O is available on 0.1 inch headers, which is convenient for the
  SAME70 Xplained headers and the AD1856 breadboard/prototype wiring.
- The current HDL already synthesizes with `synth_ice40` and has a no-debug
  board wrapper in `ad1856_formatter_board.v`.

## Selected UPduino v3.1 Mapping

Use 3.3 V logic only. Do not connect any AD1856 analog or bipolar supply rail
to UPduino or SAME70 logic pins.

The `gpio_*` names below are UPduino board/pinout labels. The actual
`upduino-v3.1-ad1856.pcf.example` file uses the corresponding iCE40UP5K SG48
package pin numbers, as required by `nextpnr-ice40`.

| Formatter signal | UPduino label | UPduino header pin | Direction | Connect to | Notes |
| --- | --- | ---: | --- | --- | --- |
| `clk_xo` | `gpio_35` | 16 | XO to UPduino | 11.2896 MHz low-jitter XO output | UPduino docs call `gpio_35` the ideal external clock input. |
| `reset_n` | `gpio_37` | 18 | SAME70 or pull-up to UPduino | SAME70 `PC17`, `EXT1 pin 10`, or 10 k pull-up to 3.3 V | Active-low reset input. Keep high for normal operation. |
| `same_td` | `gpio_23` | 11 | SAME70 to UPduino | SAME70 `TD`, `PD26`, `J502 pin 1` | SAME70 slave-TX sample data into formatter. |
| `same_tk` | `gpio_25` | 12 | UPduino to SAME70 | SAME70 `TK`, `PB1`, `J505 pin 8` or `J507 pin 4` | Formatter-generated BCLK into SAME70. |
| `same_tf` | `gpio_26` | 13 | UPduino to SAME70 | SAME70 `TF`, `PB0`, `J505 pin 7` or `J507 pin 5` | Formatter-generated LRCK/frame into SAME70. |
| `ad1856_data` | `gpio_27` | 14 | UPduino to AD1856 | AD1856 pin 7 `DATA` | Mono sample data, MSB first. |
| `ad1856_clk` | `gpio_32` | 15 | UPduino to AD1856 | AD1856 pin 5 `CLK` | Gated 16-pulse clock per sample. |
| `ad1856_le` | `gpio_31` | 17 | UPduino to AD1856 | AD1856 pin 6 `LE` | Low-going latch pulse after the 16th bit. |

All grounds must be common: SAME70 GND, UPduino GND, XO GND, and AD1856 digital
GND. Keep the first wiring short and point-to-point. Add series damping
resistors near the UPduino outputs if scope traces show ringing.

## Build

The board-specific PCF starter lives at:

```text
ports/same70-xplained/hdl/ad1856_formatter/constraints/upduino-v3.1-ad1856.pcf.example
```

Build the UPduino v3.1 starter bitstream with:

```sh
make -C ports/same70-xplained ad1856-formatter-upduino-v31-bitstream
```

The target uses:

```text
ICE40_DEVICE=up5k
ICE40_PACKAGE=sg48
PCF=constraints/upduino-v3.1-ad1856.pcf.example
```

The output is:

```text
ports/same70-xplained/hdl/ad1856_formatter/build/ad1856_formatter-ice40.bin
```

The current starter map has been routed by `nextpnr-ice40`; the latest timing
report showed `Max frequency for clock 'clk_xo...': 68.32 MHz`, passing the
11.2896 MHz external XO target.

Do not program or wire the board from this map until the physical UPduino v3.1
board revision and pin labels have been checked against the board in hand.

## Bring-Up Order

1. Program UPduino with only `clk_xo`, `reset_n`, and scope probes connected.
2. Verify `same_tk=2.8224 MHz`, `same_tf=44.1 kHz`, and AD1856 `CLK`/`LE`
   timing from the external XO.
3. Connect SAME70 `TK`/`TF` only and verify the SAME70 pinmux/status path.
4. Connect SAME70 `TD` and test generated silence first.
5. Connect AD1856 `DATA`, then listen only after AD1856 power rails and analog
   output filtering are verified.

## Sources

- UPduino documentation: iCE40UP5K board, global clock pin notes, and pinout:
  https://upduino.readthedocs.io/en/latest/features/specs.html
- Lattice UPduino v3.1 page: iCE40UP5K, FTDI programmer, headers, and board
  features:
  https://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/iCE40_UPduino_Board
