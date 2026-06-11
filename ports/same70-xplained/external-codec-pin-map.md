# SAME70 Xplained External Codec Pin Map

This is the future wiring map for an external original UC3A3 SDR Widget audio
codec path on the stock SAME70 Xplained board. No external codec board is
connected yet, and the firmware must keep reporting `external_codec=0` until
this map is physically wired and verified.

The preferred ADC target is the UC3A3 schematic set in
`/Users/cozy/Downloads/DOCS-SDR-Widget-kit/UC3A3`: an AK5394 ADC-board
interface. The current first DAC experiment is one AD1856 mono output test;
the ES9023 DAC sheet remains the original UC3A3 reference path. This is
not a generic USB/I2S audio module. The current SAME70 board can prove USB
timing, generated audio, and bit-perfect loopback, but it cannot prove analog
SDR input or output without external ADC/DAC hardware.

The preferred future clocking model is documented in
`external-codec-low-jitter-clock-plan.md`: an external low-jitter clock board is
the ADC/DAC master, while SAME70 is an SSC-style RX/TX slave. `MCLK` should feed
the external ADC/DAC directly; SAME70 should observe the external `BCLK`/`LRCK`
on its serial clock/frame inputs.

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
- Follow `external-codec-low-jitter-clock-plan.md` for the preferred
  full-duplex external-clock architecture.
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
| External low-jitter `MCLK`, original `AD_MCLK`/`DA_MCLK` | -- | No preferred SAME70 route | -- | -- | Feed ADC/DAC directly from the external clock board; do not recreate MCLK through SAME70 | Use clock fanout/buffer or source-side series resistors |
| Optional diagnostic clock output, not external MCLK | PB13 | J504 pin 5 or J507 pin 19 | DAC0 or D38 | PCK0 | Reserved for firmware diagnostics only in the low-jitter plan | J504/J507 are not populated by default |
| 44.1 kHz family oscillator enable, future `XO_44_EN` | TBD | TBD | -- | GPIO | Firmware selects from USB rate; currently `xo_44_en=not_wired` | Choose after codec/clock board control pins are known |
| 48 kHz family oscillator enable, future `XO_48_EN` | TBD | TBD | -- | GPIO | Firmware selects from USB rate; currently `xo_48_en=not_wired` | Choose after codec/clock board control pins are known |
| Codec reset/control GPIO default, original `AD_RSTN` first | PC17 | EXT1 pin 10 | SPI_SS_B/GPIO | GPIO | Xplained Pro extension header | EXT1 has populated extension-header footprint; verify actual header population |
| CS4272 I2C SDA | PA3 | EXT1/EXT2 pin 11 or J500 pin 9 | I2C_SDA or SDA | TWD0 | Shared with camera connector, AT24MAC402, and EDBG | Selected control-port route; use only if sharing is acceptable |
| CS4272 I2C SCL | PA4 | EXT1/EXT2 pin 12 or J500 pin 10 | I2C_SCL or SCL | TWCK0 | Shared with camera connector, AT24MAC402, and EDBG | Selected control-port route; use only if sharing is acceptable |
| Power | -- | J501 pin 4 or EXT VCC pins | 3V3/VCC | 3.3 V supply | Logic supply only unless current budget is confirmed | Never use 5V for SAME70 I/O |
| Ground | -- | J501 pins 6/7 or nearby header GND | GND | Ground | Common reference | Connect before signals |

## AD1856 Mono DAC Test Signals

One AD1856 is the first selected DAC smoke test. It is a mono 16-bit PCM DAC,
not an I2S DAC and not yet a stereo/IQ output path. See
`external-dac-ad1856-mono-test.md` before wiring.

| AD1856 signal | SAME70 pin | Board header pin | SAME70 function | Notes |
| --- | --- | --- | --- | --- |
| `DATA` pin 7 | PD26 | J502 pin 1 | TD | Candidate serial data into one AD1856 |
| `CLK` pin 5 | PB1 | J505 pin 8 or J507 pin 4 | TK | Candidate data clock; must meet AD1856 timing |
| `LE` pin 6 | PB0 | J505 pin 7 or J507 pin 5 | TF | Candidate latch-enable pulse; do not assume normal LRCK is valid |
| `VOUT` pin 9 | -- | -- | -- | External analog low-pass/output stage |
| `+VL/-VL`, `+VS/-VS` | -- | -- | -- | External bipolar supplies; do not power from SAME70 3V3 |

## AD1856 Low-Jitter Formatter Signals

The preferred low-jitter AD1856 path inserts a small external formatter between
SAME70 and AD1856. In that path, SAME70 does not drive AD1856 pins directly.
The formatter owns AD1856 `DATA`, `CLK`, and `LE`; SAME70 provides samples and
accepts external transmit timing. See
`external-dac-ad1856-low-jitter-formatter.md` before designing this board.

| Signal | SAME70 pin | Board header pin | Direction at SAME70 | Notes |
| --- | --- | --- | --- | --- |
| Formatter sample input from SAME70 | PD26 | J502 pin 1 | Output, `TD` | Carries selected mono sample bits into formatter |
| Formatter transmit clock to SAME70 | PB1 | J505 pin 8 or J507 pin 4 | Input, `TK` | External clock domain; do not drive from SAME70 |
| Formatter transmit frame to SAME70 | PB0 | J505 pin 7 or J507 pin 5 | Input, `TF` | External frame/latch cadence; do not drive from SAME70 |
| Formatter to AD1856 `DATA` | -- | -- | -- | Formatter output to AD1856 pin 7 |
| Formatter to AD1856 `CLK` | -- | -- | -- | Gated 16-pulse formatter output to AD1856 pin 5 |
| Formatter to AD1856 `LE` | -- | -- | -- | Low-going formatter output to AD1856 pin 6 after 16 bits |

## Bring-Up Order

1. Confirm the exact external codec board pinout, clocking mode, and voltage.
2. Check SAME70-XPLD schematic continuity for every selected header pin.
3. With power off, verify no shorts between `3V3`, `5V0`, `GND`, and each
   signal.
4. Power only the stock SAME70 board and verify `board-ready` still passes.
5. Attach external board power/ground only; verify supply voltage and current.
6. Add reset/control wiring and verify GPIO-only control.
7. Add CS4272 I2C control on PA3/PA4 only after bus sharing is checked.
7. Add external `MCLK`, `BCLK`, and `LRCK` wiring and verify clock-only input
   at the codecs and SAME70 before connecting data pins.
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
