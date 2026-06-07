# SAME70 Xplained External Codec Pin Map

This is the future wiring map for an external original UC3A3 SDR Widget audio
codec path on the stock SAME70 Xplained board. No external codec board is
connected yet, and the firmware must keep reporting `external_codec=0` until
this map is physically wired and verified.

The preferred target is the UC3A3 schematic set in
`/Users/cozy/Downloads/DOCS-SDR-Widget-kit/UC3A3`: an AK5394 ADC-board
interface plus the ES9023 DAC sheet, not a generic USB/I2S audio module. The
current SAME70 board can prove USB timing, generated audio, and bit-perfect
loopback, but it cannot prove analog SDR input or output without external
ADC/DAC hardware.

## Rules Before Wiring

- Use only 3.3 V logic. Do not connect 5 V logic to SAME70 I/O pins.
- Use board `3V3` and `GND` for low-power external logic only; check current
  limits before powering a full codec board from the Xplained board.
- Treat every listed signal as unverified until checked against the SAME70-XPLD
  schematic and the exact external codec board schematic.
- Do not enable a codec/I2S/SSC/I2SC firmware path until the no-short and
  voltage checks pass with the board unpowered, then powered.
- Keep `audio hw` reporting `external_codec=0`, `i2sc=not_configured`, and
  `needs_external_codec_board` until hardware is actually attached.
- Run `make -C ports/same70-xplained external-codec-preflight` before wiring or
  codec-firmware work; it checks this map and the live `audio hw` boundary.
- Follow `external-codec-original-uc3a3-map.md` as the source-of-truth signal
  model before using this SAME70 header projection.
- Follow `external-codec-wiring-checklist.md` before selecting, powering, or
  wiring an external codec board.

## Candidate Digital Audio Signals

| Future signal | SAME70 pin | Board header pin | Board marking | SAME70 function | Shared notes | Solder/resistor notes |
| --- | --- | --- | --- | --- | --- | --- |
| DAC serial data, original `DA_SDATA` | PD26 | J502 pin 1 | AD0 | TD | Analog-low Arduino header | J502 is not populated by default; no 0-ohm change noted for pin 1 |
| ADC serial data, original `AD_SDATA` | PA10 | J504 pin 2 | AD9 | RD | Analog-high Arduino header | J504 is not populated by default |
| ADC frame, original `AD_LRCK`/`AD_FSYNC` | PD24 | J504 pin 1 | AD8 | RF | Analog-high Arduino header | J504 is not populated by default |
| ADC bit clock, original `AD_SCLK` | PA22 | J504 pin 3 | AD10 | RK | Analog-high Arduino header | J504 is not populated by default |
| DAC bit clock, original `DA_SCLK` | PB1 | J505 pin 8 or J507 pin 4 | D14 or D23 | TK | Shared with TXD0 on J505 and D23 on J507 | J505/J507 are not populated by default |
| DAC frame, original `DA_LRCK` | PB0 | J505 pin 7 or J507 pin 5 | D15 or D24 | TF | Shared with RXD0 on J505 and D24 on J507 | J505/J507 are not populated by default |
| Provisional clock probe for original `AD_MCLK`/`DA_MCLK` | PB13 | J504 pin 5 or J507 pin 19 | DAC0 or D38 | PCK0 | Direction is TBD; UC3A3 schemes feed 12.288 MHz to CPU and 24.576 MHz to ES9023 from ADC side | J504/J507 are not populated by default |
| Codec reset/control GPIO default, original `AD_RSTN` first | PC17 | EXT1 pin 10 | SPI_SS_B/GPIO | GPIO | Xplained Pro extension header | EXT1 has populated extension-header footprint; verify actual header population |
| Optional I2C SDA | PA3 | EXT1/EXT2 pin 11 or J500 pin 9 | I2C_SDA or SDA | TWD0 | Shared with camera connector, AT24MAC402, and EDBG | Use only if sharing is acceptable |
| Optional I2C SCL | PA4 | EXT1/EXT2 pin 12 or J500 pin 10 | I2C_SCL or SCL | TWCK0 | Shared with camera connector, AT24MAC402, and EDBG | Use only if sharing is acceptable |
| Power | -- | J501 pin 4 or EXT VCC pins | 3V3/VCC | 3.3 V supply | Logic supply only unless current budget is confirmed | Never use 5V for SAME70 I/O |
| Ground | -- | J501 pins 6/7 or nearby header GND | GND | Ground | Common reference | Connect before signals |

## Bring-Up Order

1. Confirm the exact external codec board pinout, clocking mode, and voltage.
2. Check SAME70-XPLD schematic continuity for every selected header pin.
3. With power off, verify no shorts between `3V3`, `5V0`, `GND`, and each
   signal.
4. Power only the stock SAME70 board and verify `board-ready` still passes.
5. Attach external board power/ground only; verify supply voltage and current.
6. Add reset/control wiring and verify GPIO-only control.
7. Add clock wiring and verify clock-only output before connecting data pins.
8. Add serial audio data pins and create a new explicit external-codec gate.

For the detailed staged checklist, see `external-codec-wiring-checklist.md`.

## Firmware Boundary

Current firmware support is intentionally limited to reporting this future map
through `audio hw`. The stock-board acceptance gates remain USB loopback,
generated audio, UAC controls, and serial diagnostics.
`external-codec-preflight` is only a pre-wiring boundary gate: it verifies this
map and confirms that the connected stock board still reports no external
codec. A future external-codec acceptance gate must be separate from
`board-ready` and must prove real ADC/DAC movement before analog SDR audio is
claimed.
