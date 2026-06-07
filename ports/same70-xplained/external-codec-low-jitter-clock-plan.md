# External Low-Jitter Full-Duplex Clock Plan

This is the preferred future clocking plan for using the stock SAME70 Xplained
with an external ADC/DAC board. It keeps the current stock-board firmware
boundary intact: until external hardware is wired and measured, `audio hw` must
continue reporting `external_codec=0`, `i2sc=not_configured`, and
`needs_external_codec_board`.

## Target Architecture

The external audio board is the clock master. SAME70 is an SSC-style serial
audio slave for both receive and transmit.

```text
external low-jitter clock board -> ADC MCLK/BCLK/LRCK
external low-jitter clock board -> DAC MCLK/BCLK/LRCK
external BCLK/LRCK              -> SAME70 RK/RF and TK/TF
ADC DATA                        -> SAME70 RD
SAME70 TD                       -> DAC DATA
```

Do not use SAME70 PLL/PCK-derived audio clocks for the low-jitter path. `PCK0`
on `PB13` may be useful as a firmware diagnostic output, but it is not the
preferred MCLK source for the external ADC/DAC board.

The first selected DAC experiment is one AD1856 mono output test. AD1856 is not
an I2S DAC; it needs `DATA`, `CLK`, and a low-going `LE` latch pulse after each
16-bit word. Treat the AD1856 path as a mono timing/analog-output smoke test
until a formatter or a second DAC channel exists. The final low-jitter version
must derive AD1856 `CLK`/`LE` from the external audio clock domain, because the
DAC latch edge is the jitter-sensitive event.

## Header Mapping

| Signal | Direction | SAME70 pin/header | Role |
| --- | --- | --- | --- |
| `ADC_DATA` / original `AD_SDATA` | ADC to SAME70 | `RD` on `PA10`, `J504 pin 2` | SSC RX data |
| `DAC_DATA` / original `DA_SDATA` | SAME70 to DAC | `TD` on `PD26`, `J502 pin 1` | SSC TX data |
| `BCLK` / original `AD_SCLK` + `DA_SCLK` | Clock board to SAME70/codecs | `RK` on `PA22`, `J504 pin 3`; `TK` on `PB1`, `J505 pin 8` or `J507 pin 4` | Shared serial bit clock domain |
| `LRCK` / original `AD_LRCK` + `DA_LRCK` | Clock board to SAME70/codecs | `RF` on `PD24`, `J504 pin 1`; `TF` on `PB0`, `J505 pin 7` or `J507 pin 5` | Shared serial frame clock domain |
| `MCLK` / original `AD_MCLK` + `DA_MCLK` | Clock board to ADC/DAC | No preferred SAME70 header route | Feed codecs directly; do not route through SAME70 |
| `AD_RSTN` / control | SAME70 to codec board | `PC17`, `EXT1 pin 10` | Reset/control bring-up GPIO |
| AD1856 mono `DATA`/`CLK`/`LE` | SAME70 or formatter to DAC | `TD`/`TK`/`TF` on `PD26`/`PB1`/`PB0` | Mono DAC smoke-test candidate, not normal I2S |

Use a clock fanout/buffer, or at least source-side series resistors for each
clock branch, before splitting `BCLK` and `LRCK` to ADC, DAC, and SAME70 pins.
Keep ground returns close to clock and data lines.

## Clock Families

Start with a single 44.1 kHz-family bring-up:

- `MCLK`: `22.5792 MHz` or `11.2896 MHz`, depending on the ADC/DAC board.
- `BCLK`: `2.8224 MHz` for stereo 32-bit frames (`44.1 kHz * 64`).
- `LRCK`: `44.1 kHz`.

For AD1856 mono, keep the first target at `44.1 kHz / 16-bit`. AD1856 has no
MCLK input; it needs a 16-bit serial word clocked into `DATA` plus a correctly
timed `LE` pulse. Do not wire `LRCK` directly to `LE` without measuring pulse
polarity, width, and alignment.

Add the 48 kHz family after the 44.1 kHz path is stable:

- `MCLK`: `24.576 MHz` or `12.288 MHz`, depending on the ADC/DAC board.
- `BCLK`: `3.072 MHz` for stereo 32-bit frames (`48 kHz * 64`).
- `LRCK`: `48 kHz`.

## USB Feedback

The first external-clock firmware should use explicit USB feedback endpoint 4.
Feedback must be derived from the real external-clock cadence: ADC RX frame
counts, DAC TX FIFO consumption, or the measured full-duplex FIFO drift. The
host should adapt to the external clock; firmware must not resample audio.

Implicit feedback is a later experiment after full-duplex ADC and DAC streaming
are proven to share the same external clock domain. Do not remove explicit
feedback until the ADC USB IN stream is known to represent the same clock that
drives DAC sample consumption.

## Bring-Up Order

1. Keep `external-codec-preflight` passing on the stock board.
2. Wire power and ground only, then measure current and rail voltage.
3. Wire reset/control only and verify the external board can be held inactive.
4. Wire `MCLK`, `BCLK`, and `LRCK` only; measure frequency, voltage, duty cycle,
   and signal integrity at the ADC, DAC, and SAME70 header pins.
5. Enable firmware pin-mux/status reporting for SSC slave clocks only.
6. Bring up ADC RX into a diagnostic buffer without sending analog SDR over USB.
7. Bring up DAC TX with silence or a deterministic pattern.
8. Add a separate external-codec acceptance gate proving real ADC/DAC movement,
   zero FIFO overflow/underflow, and bit-perfect sample transfer where possible.
