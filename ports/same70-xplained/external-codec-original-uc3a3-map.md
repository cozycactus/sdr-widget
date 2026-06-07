# Original UC3A3 Codec Wiring Map

This file records the original SDR Widget kit wiring that should drive the
future SAME70 external-codec path. The source of truth is the local UC3A3
schematic set in:

```text
/Users/cozy/Downloads/DOCS-SDR-Widget-kit/UC3A3
```

Relevant sheets:

- `AT32UC3A3256-2.12.pdf`: UC3A3 MCU pins, JTAG, USB, oscillator, and audio
  nets.
- `MISC_IO-3.10.pdf`: AK5394 board connector, DAC clock/control connector,
  I2C, LCD, and miscellaneous I/O.
- `ES9023-DAC.pdf`: ES9023 DAC and its I2S connector.
- `Power-2.02.pdf`: UC3A3 power and power connection to the AK5394 board.

The SAME70 port must follow this original wiring model, not a generic I2S or
USB audio module. Until matching external hardware is connected and verified,
firmware must continue reporting `external_codec=0`, `i2sc=not_configured`,
and `needs_external_codec_board`.

## Original Clock Model

The UC3A3 design is clocked from the ADC-side audio clocking, not from an
arbitrary local MCU audio clock.

- `AD_MCLK`: 12.288 MHz feed from the AK5394 ADC board to the CPU. The UC3A3
  board definition names this `AK5394_AD_MCLK` on `AVR32_PIN_PC04`, and
  `FOSC1` is `12288000`.
- `DA_MCLK`: 24.576 MHz feed from the ADC card to the ES9023 DAC MCLK input.
  The `MISC_IO` sheet annotates this as `24.576Mhz from the ADC card to the
  ES9023`; the `ES9023-DAC` sheet annotates it as `24.576Mhz clock`.
- `DA_SCLK`/`DA_LRCK`: DAC bit clock and frame clock. Legacy firmware derives
  `GCLK1` from the external OSC1 clock and uses it for `DA_SCLK`; SSC TX then
  runs in the same audio-clock domain.
- `AD_SCLK`/`AD_LRCK`/`AD_FSYNC`: ADC-side serial clocks/frame signals feeding
  SSC RX.

SAME70 clock bring-up must preserve this clock-domain relationship. Treat the
current SAME70 `PCK0` pin-map entry as only a provisional clock probe until the
external ADC-clock direction is confirmed on real hardware.

For the stock SAME70 Xplained header route, the preferred adaptation is not to
recreate the UC3A3 `PC04`/OSC1 input exactly. Instead, use an external
low-jitter audio clock board as the ADC/DAC master, feed `MCLK` directly to the
codecs, and feed the same `BCLK`/`LRCK` clock domain into SAME70 `RK`/`RF` and
`TK`/`TF` as documented in `external-codec-low-jitter-clock-plan.md`.

## AK5394 Board Connector

`MISC_IO-3.10.pdf` names J303 as the interface connections to the AK5394 board:

| J303 pin | Original net | Meaning |
| --- | --- | --- |
| 1 | `AD_SDATA` | ADC serial data into MCU |
| 2 | `AD_FSYNC` | ADC frame sync option |
| 3 | `AD_MCLK` | 12.288 MHz feed to CPU |
| 4 | `AD_ZCAL` | ADC zero-calibration control |
| 5 | `AD_CAL` | ADC calibration active/status |
| 6 | `AD_RSTN` | ADC reset, active low |
| 7 | `AD_LRCK` | ADC LRCK/frame clock |
| 8 | `AD_SCLK` | ADC bit clock |

`MISC_IO-3.10.pdf` names J304 as the second AK5394/DAC-support connector:

| J304 pin | Original net | Meaning |
| --- | --- | --- |
| 1 | `DA_MCLK` | 24.576 MHz feed from ADC card to ES9023 |
| 2 | `AD_HPFE` | ADC high-pass filter enable |
| 3 | `AD_DFS1` | ADC sample-rate mode/control |
| 4 | `AD_DFS0` | ADC sample-rate mode/control |
| 5 | `AD_SMODE2` | ADC serial mode control |
| 6 | `AD_SMODE1` | ADC serial mode control |

## ES9023 DAC Connector

`ES9023-DAC.pdf` routes the ES9023 pins through 4-pin I2S headers:

| I2S pin | Original net | ES9023 pin |
| --- | --- | --- |
| 1 | `DA_MCLK` | `MCLK` pin 13 |
| 2 | `DA_SCLK` | `BCK` pin 1 |
| 3 | `DA_SDATA` | `SDI` pin 3 |
| 4 | `DA_LRCK` | `LRCK` pin 2 |

## AD1856 Mono DAC Deviation

The current first DAC experiment intentionally deviates from the original
ES9023 DAC sheet: use one AD1856 for a mono 16-bit output smoke test. AD1856 is
not an I2S DAC and has no MCLK input; it needs `DATA`, `CLK`, and `LE`.
Treat ES9023 as the original UC3A3 reference path, and treat AD1856 as the
selected mono-first DAC experiment documented in
`external-dac-ad1856-mono-test.md`.

## Legacy Firmware Pin Evidence

The original firmware board definition in
`src/SOFTWARE_FRAMEWORK/BOARDS/SDRwdgtLite/SDRwdgt.h` confirms the AK5394
control and receive-side pins:

| Firmware symbol | UC3A3 pin |
| --- | --- |
| `AK5394_DFS0` | `PB00` |
| `AK5394_DFS1` | `PB01` |
| `AK5394_RSTN` | `PB03` |
| `AK5394_HPFE` | `PB04` |
| `AK5394_ZCAL` | `PB05` |
| `AK5394_CAL` | `PB06` |
| `AK5394_SMODE1` | `PB07` |
| `AK5394_SMODE2` | `PB08` |
| `AK5394_FSYNC` / `AK5394_LRCK_IN` | `PX26` |
| `AK5394_SDATA` | `PX25` |
| `AK5394_SCLK` | `PX28` |
| `AK5394_AD_MCLK` | `PC04` |

Legacy firmware in `src/taskAK5394A.c` enables external OSC1 from the AK5394A
12.288 MHz oscillator, configures `GCLK1` for DAC clocking, assigns SSC
RX/TX pins, and uses PDCA for SSC RX/TX sample movement.

## SAME70 Mapping Intent

The SAME70 external path should map original UC3A3 nets onto header-exposed
SAME70 SSC-style pins, while preserving the original signal roles:

| Original net | SAME70 candidate role | SAME70 candidate pin/header |
| --- | --- | --- |
| `DA_SDATA` | DAC serial data out | `TD` on `PD26`, `J502 pin 1` |
| `AD_SDATA` | ADC serial data in | `RD` on `PA10`, `J504 pin 2` |
| `AD_LRCK` / `AD_FSYNC` | ADC receive frame | `RF` on `PD24`, `J504 pin 1` |
| `AD_SCLK` | ADC receive bit clock | `RK` on `PA22`, `J504 pin 3` |
| `DA_SCLK` | DAC transmit bit clock | `TK` on `PB1`, `J505 pin 8` or `J507 pin 4` |
| `DA_LRCK` | DAC transmit frame | `TF` on `PB0`, `J505 pin 7` or `J507 pin 5` |
| `AD_MCLK` | External ADC clock domain / CPU reference equivalent | direct external-clock feed to ADC; no preferred SAME70 header route |
| `DA_MCLK` | ES9023 MCLK from ADC card | direct external-clock feed to DAC; no preferred SAME70 header route |

Do not treat `PCK0` on `PB13` as the external codec MCLK source for the
low-jitter plan. Keep it reserved for optional diagnostics unless a later
measured hardware revision proves that routing through SAME70 is acceptable.
