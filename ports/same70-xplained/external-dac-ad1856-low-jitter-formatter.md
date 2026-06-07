# AD1856 Low-Jitter Formatter Plan

This is the preferred AD1856 architecture after the direct mono smoke test.
The goal is to keep the DAC latch timing outside SAME70 and inside an external
low-jitter audio clock domain.

## Preferred Architecture

Use a small external formatter between SAME70 and AD1856:

```text
external XO / clock divider -> formatter timing
formatter BCLK/LRCK         -> SAME70 TK/TF as slave-TX timing inputs
SAME70 TD                   -> formatter sample input
formatter DATA/CLK/LE       -> AD1856 pins 7/5/6
AD1856 VOUT                 -> analog low-pass/output stage
```

SAME70 must not generate AD1856 `CLK` or `LE` in the low-jitter path. It should
only place sample bits onto `TD` while the external formatter clocks the
transmit side. The formatter captures the selected sample, then shifts it to
AD1856 using low-jitter `DATA`, gated `CLK`, and low-going `LE`.

## Signal Direction

| Signal | SAME70 side | Formatter side | AD1856 side |
| --- | --- | --- | --- |
| Sample data | `TD` on `PD26`, `J502 pin 1`, SAME70 output | input FIFO/shift capture | formatter output to AD1856 pin 7 `DATA` |
| Transmit clock | `TK` on `PB1`, `J505 pin 8` or `J507 pin 4`, SAME70 input | clock output to SAME70 | separate gated output to AD1856 pin 5 `CLK` |
| Transmit frame | `TF` on `PB0`, `J505 pin 7` or `J507 pin 5`, SAME70 input | frame output to SAME70 | low-going latch output to AD1856 pin 6 `LE` |
| Audio clock source | no SAME70 MCLK route | low-jitter XO/divider | no AD1856 MCLK input |

For the first mono DAC, the formatter should use the left channel sample exactly
and ignore the right channel. Do not average left/right for the bit-perfect
proof; averaging creates new samples.

## Clocking

For the first 44.1 kHz family bring-up:

- `XO`: `11.2896 MHz` or `22.5792 MHz`.
- SAME70 formatter input side: normal external audio clock/frame, preferably
  `BCLK=2.8224 MHz` and `LRCK=44.1 kHz` so the future ADC/DAC path shares the
  same cadence.
- AD1856 output side: exactly 16 rising `CLK` edges per mono sample, MSB first,
  then a low-going `LE` pulse after the 16th bit.

Do not feed continuous `BCLK` directly into AD1856 `CLK` unless the formatter
also guarantees that the 16 most recent bits before `LE` are the intended DAC
word. The safer design is a gated 16-pulse AD1856 clock per sample.

## USB Feedback

Keep explicit USB feedback for the first external-clock firmware. Feedback
should come from the real external clock domain: formatter FIFO fill, SAME70
TX consumption against external `TK`/`TF`, ADC RX frame counts, or measured
full-duplex drift once the ADC is wired.

The firmware must not resample audio to hide clock drift. The host should adapt
to the external clock.

## Bring-Up Order

1. Keep the current direct AD1856 mono smoke-test plan available for pin and
   analog-output proof.
2. Build the formatter clock-only: XO, dividers, `BCLK`, `LRCK`, AD1856 gated
   `CLK`, and `LE`; leave AD1856 `DATA` and SAME70 `TD` disconnected.
3. Scope `BCLK/LRCK` at SAME70 `TK/TF` and `CLK/LE` at AD1856 pins.
4. Connect SAME70 `TD` to formatter input and capture a deterministic silence
   pattern, then a low-amplitude tone.
5. Connect formatter `DATA` to AD1856 pin 7 only after timing is verified.
6. Add an AD1856 formatter acceptance gate separate from `board-ready`.

## HDL Starter

The first generic Verilog implementation lives in:

```text
ports/same70-xplained/hdl/ad1856_formatter
```

It targets an `11.2896 MHz` external XO, generates `2.8224 MHz` `same_tk`,
generates `44.1 kHz` `same_tf`, captures the left 16-bit sample from SAME70
`TD`, and outputs a gated 16-pulse AD1856 `CLK` plus low-going `LE`.

Run the formatter simulation with:

```sh
make -C ports/same70-xplained ad1856-formatter-sim
```
