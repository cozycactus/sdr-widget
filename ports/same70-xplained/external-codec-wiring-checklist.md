# SAME70 Xplained External Codec Wiring Checklist

Use this checklist when an external original-style SDR Widget codec board is
about to be selected, powered, or wired to the stock SAME70 Xplained headers.
No external codec board is connected yet. Until every relevant hold point below
is passed with real measurements, firmware must keep reporting
`external_codec=0`, `i2sc=not_configured`, and `needs_external_codec_board`.

This checklist is for the UC3A3 original AK5394 ADC-board plus ES9023 DAC path
with `AD_MCLK`, `DA_MCLK`, `AD_SCLK`, `AD_LRCK`/`AD_FSYNC`, `AD_SDATA`,
`DA_SCLK`, `DA_LRCK`, `DA_SDATA`, reset, and ADC mode/control signals. It is
not for a generic USB/I2S audio module.

## Stop Conditions

Stop immediately and do not wire or enable codec firmware if any item is true:

- The codec board drives any SAME70 I/O above 3.3 V logic.
- The codec board pinout, clock direction, or power requirement is unknown.
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
- Confirm the clock plan follows the UC3A3 model: ADC-side audio clocking feeds
  `AD_MCLK` at 12.288 MHz to the CPU clock domain and `DA_MCLK` at 24.576 MHz
  to the ES9023 DAC.
- Confirm reset polarity and optional control bus requirements.
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
- Probe MCLK first, then BCLK/LRCK if the clock plan requires them.
- Confirm frequency, voltage, duty cycle, and no unexpected activity on data
  pins.
- Keep ADC and DAC data disconnected until clock-only measurements pass.

## Stage 6: Serial Audio Data

- Wire DAC serial data and ADC serial data only after clocks are verified.
- Add firmware pin mux/status reporting before enabling streaming through the
  codec path.
- Create a new external-codec acceptance gate separate from `board-ready`.
- The acceptance gate must prove real ADC/DAC movement, not generated USB audio
  and not host loopback alone.

## Candidate Header Summary

| Original signal | SAME70 pin | Header |
| --- | --- | --- |
| `DA_SDATA` to `TD` | PD26 | J502 pin 1 |
| `AD_SDATA` to `RD` | PA10 | J504 pin 2 |
| `AD_LRCK`/`AD_FSYNC` to `RF` | PD24 | J504 pin 1 |
| `AD_SCLK` to `RK` | PA22 | J504 pin 3 |
| `DA_SCLK` to `TK` | PB1 | J505 pin 8 or J507 pin 4 |
| `DA_LRCK` to `TF` | PB0 | J505 pin 7 or J507 pin 5 |
| `AD_MCLK`/`DA_MCLK` clock probe, direction TBD | PB13 | J504 pin 5 or J507 pin 19 |
| `AD_RSTN`/control GPIO default | PC17 | EXT1 pin 10 |
