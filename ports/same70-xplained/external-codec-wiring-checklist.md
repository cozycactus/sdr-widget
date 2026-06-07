# SAME70 Xplained External Codec Wiring Checklist

Use this checklist when an external original-style SDR Widget codec board is
about to be selected, powered, or wired to the stock SAME70 Xplained headers.
No external codec board is connected yet. Until every relevant hold point below
is passed with real measurements, firmware must keep reporting
`external_codec=0`, `i2sc=not_configured`, and `needs_external_codec_board`.

This checklist is for the UC3A3 original AK5394 ADC-board plus the selected DAC
path. The original DAC reference is ES9023; the current first DAC experiment is
one AD1856 mono output test with `DATA`, `CLK`, and `LE`. It is not for a
generic USB/I2S audio module. The preferred clocking target is the full-duplex
external low-jitter plan in `external-codec-low-jitter-clock-plan.md`: the
external board is clock master, SAME70 is an SSC RX/TX slave, and USB OUT uses
explicit feedback until the real ADC/DAC path is proven.

## Stop Conditions

Stop immediately and do not wire or enable codec firmware if any item is true:

- The codec board drives any SAME70 I/O above 3.3 V logic.
- The codec board pinout, clock direction, or power requirement is unknown.
- The selected clock plan requires SAME70 to recreate DAC/ADC MCLK from PCK0
  instead of feeding MCLK directly from the external low-jitter clock board.
- AD1856 is powered from SAME70 `3V3`, or its required bipolar supplies are not
  available and measured.
- AD1856 `LE` timing is assumed to be normal I2S `LRCK` without scope
  verification.
- The AD1856 low-jitter path wires SAME70 directly to AD1856 `CLK`/`LE`
  instead of using an external formatter clocked from the low-jitter domain.
- Any short is measured between `3V3`, `5V0`, `GND`, or a selected signal.
- `make -C ports/same70-xplained external-codec-preflight` fails.
- `make -C ports/same70-xplained board-ready` fails on the stock board.
- The new firmware path would claim analog SDR input/output before a separate
  external-codec acceptance gate proves real ADC/DAC data.

## Stage 0: Stock Board Baseline

- Run `make -C ports/same70-xplained external-codec-preflight`.
- Run `make -C ports/same70-xplained board-ready`.
- Confirm `audio hw` still reports `external_codec=0` and
  `needs_external_codec_board`.
- Confirm the target USB cable and EDBG USB cable are both connected as needed.

## Stage 1: External Codec Board Selection

- Confirm the board exposes separate serial ADC data and DAC data or a mode
  compatible with SAME70 SSC-style transmit/receive wiring.
- Confirm the board follows the UC3A3 original signal model in
  `external-codec-original-uc3a3-map.md`, including J303/J304 and ES9023 I2S
  net names.
- Confirm all digital I/O is 3.3 V-compatible, or add level shifting before any
  SAME70 signal connection.
- Confirm the clock plan follows `external-codec-low-jitter-clock-plan.md`:
  external low-jitter `MCLK` feeds ADC/DAC directly, external `BCLK` feeds both
  `RK` and `TK`, external `LRCK` feeds both `RF` and `TF`, and SAME70 does not
  recreate MCLK through PCK0.
- Confirm the first firmware path will use explicit USB feedback derived from
  real SSC/FIFO cadence, with no sample-rate conversion.
- Confirm reset polarity and optional control bus requirements.
- For the mono AD1856 test, confirm the board exposes `DATA`, `CLK`, `LE`,
  bipolar supplies, common ground, and an analog output filter.
- For the preferred low-jitter AD1856 path, confirm the formatter drives
  AD1856 `DATA`, gated `CLK`, and low-going `LE`, while SAME70 only provides
  sample data and accepts external transmit timing.
- Confirm the 3.3 V current draw is safe if powered from SAME70 Xplained
  headers; otherwise use an external 3.3 V supply with common ground.

## Stage 2: Unpowered Checks

- Verify the final header route against `external-codec-pin-map.md`.
- Connect ground first and verify common ground continuity.
- Measure resistance from `3V3` to `GND`.
- Measure resistance from `5V0` to every selected signal.
- Measure resistance from each selected signal to adjacent header pins.
- Leave data pins disconnected until power, reset, and clock checks pass.

## Stage 3: Power Only

- Power only the SAME70 board and verify `board-ready` still passes.
- Attach external board power and ground only.
- Measure 3.3 V at the codec board.
- Measure current draw and check for unexpected heating.
- Run `external-codec-preflight` again; it must still pass.

## Stage 4: Reset And Control

- Wire only codec reset/control after the power-only stage passes.
- Keep serial audio pins disconnected.
- Verify reset can hold the codec inactive before any clocks or data are used.
- If optional I2C is used, confirm PA3/PA4 sharing is acceptable before wiring.

## Stage 5: Clock-Only Probe

- Wire only the selected clock signals after reset/control passes.
- Probe external MCLK at the ADC/DAC first, then BCLK/LRCK at the ADC, DAC,
  and SAME70 header pins.
- Confirm frequency, voltage, duty cycle, fanout/signal integrity, and no
  unexpected activity on data pins.
- Keep ADC and DAC data disconnected until clock-only measurements pass.

## Stage 6: Serial Audio Data

- Wire DAC serial data and ADC serial data only after clocks are verified.
- Add firmware pin mux/status reporting before enabling streaming through the
  codec path.
- Create a new external-codec acceptance gate separate from `board-ready`.
- The acceptance gate must prove real ADC/DAC movement, not generated USB audio
  and not host loopback alone.
- The first acceptance gate should keep explicit feedback endpoint 4 enabled and
  prove zero FIFO overflow/underflow before any implicit-feedback experiment.

## Candidate Header Summary

| Original signal | SAME70 pin | Header |
| --- | --- | --- |
| `DA_SDATA` to `TD` | PD26 | J502 pin 1 |
| `AD_SDATA` to `RD` | PA10 | J504 pin 2 |
| `AD_LRCK`/`AD_FSYNC` to `RF` | PD24 | J504 pin 1 |
| `AD_SCLK` to `RK` | PA22 | J504 pin 3 |
| `DA_SCLK` to `TK` | PB1 | J505 pin 8 or J507 pin 4 |
| `DA_LRCK` to `TF` | PB0 | J505 pin 7 or J507 pin 5 |
| External `AD_MCLK`/`DA_MCLK` direct to ADC/DAC | -- | No preferred SAME70 route |
| Optional diagnostic `PCK0`, not codec MCLK | PB13 | J504 pin 5 or J507 pin 19 |
| `AD_RSTN`/control GPIO default | PC17 | EXT1 pin 10 |

For the AD1856 mono test, interpret the DAC-side candidates as `DATA` on `TD`,
`CLK` on `TK`, and `LE` on `TF` only after `external-dac-ad1856-mono-test.md`
timing checks are satisfied.

For the preferred AD1856 low-jitter formatter path, `TD` goes to the formatter,
external formatter timing goes into SAME70 `TK`/`TF`, and the formatter drives
AD1856 `DATA`/`CLK`/`LE`. See
`external-dac-ad1856-low-jitter-formatter.md`.
