# I2S Codec Bit-Perfect Low-Jitter Plan

This is the practical external-codec path to prefer before building more
AD1856 formatter hardware. It keeps the same stock-board boundary as the other
external-codec documents: until a real codec board is wired and measured,
firmware must continue reporting `external_codec=0`, `i2sc=not_configured`, and
`needs_external_codec_board`.

## Preferred Part Class

Use a CS4272-class stereo ADC/DAC codec first. CS4272 is the preferred first
target because it has both ADC and DAC paths, supports slave serial audio port
operation, and supports I2S serial audio up to 24-bit words. A PCM3060-class
codec is the backup practical option if CS4272 hardware is not convenient.

This is not a generic USB/I2S audio module plan. The USB device remains the
SAME70 firmware. The external board exposes raw codec clocks and serial audio
signals to the SAME70 headers.

## Target Architecture

The external low-jitter clock domain owns audio timing. SAME70 is an SSC-style
slave for both USB OUT to DAC and ADC to USB IN.

```text
external low-jitter XO / clock board -> codec MCLK/BCLK/LRCK
external BCLK/LRCK                  -> SAME70 RK/RF and TK/TF
host USB OUT samples                -> USB ring, no DSP, no dither,
                                       no volume apply, no resampling
SAME70 TD                           -> codec DAC SDATA
codec ADC SDATA                     -> SAME70 RD
SAME70 feedback endpoint 4          -> explicit USB feedback from the
                                       external-clock consumption/capture rate
```

Do not derive codec MCLK from SAME70 PCK0 for this path. PCK0 diagnostic output
on PB13 is optional only.

## CS4272 Clock Division

The first CS4272 bring-up should use CS4272 master-mode 256fs clocking. In that
mode the codec receives a low-jitter MCLK input and divides it to make the
serial clocks that SAME70 uses as a slave:

```text
11.2896 MHz MCLK -> 44.1 kHz LRCK, 2.8224 MHz BCLK
12.2880 MHz MCLK -> 48.0 kHz LRCK, 3.0720 MHz BCLK
```

The divider is therefore inside the codec clock domain, not inside SAME70:
`MCLK/LRCK = 256`, `BCLK/LRCK = 64`, and `MCLK/BCLK = 4`. The offline
`external-codec-clock-model` target verifies these ratios and checks that the
current firmware USB high-speed feedback constants match the same 44.1 kHz and
48 kHz sample rates.

## Two Oscillator Selection

Two frequency families require two low-jitter oscillator sources or one
low-jitter clock generator with two exact audio families. SAME70 should select
the family from the USB sample rate request, not synthesize MCLK itself:

```text
selected_family=from_usb_rate
44.1 kHz USB mode -> XO_44_EN asserted, XO_48_EN deasserted
48.0 kHz USB mode -> XO_48_EN asserted, XO_44_EN deasserted
```

The current stock board has no oscillator-select wiring, so live firmware status
must continue to report `xo_44_en=not_wired` and `xo_48_en=not_wired`.

The hardware must make the selection fail-safe. Do not allow both oscillator
outputs to drive the same MCLK node. Use oscillator output-enable pins, a
low-jitter clock mux, or separate fanout paths with only the selected family
connected to the codec clock input. During any family switch, stop or mute the
stream, hold the codec in reset or mute, change the oscillator select, wait for
the new clock to settle, then release reset and restart the stream with matching
explicit feedback.

## Candidate Header Signals

| Codec signal | SAME70 route | Board header | Direction |
| --- | --- | --- | --- |
| DAC SDATA | `TD` on `PD26` | `J502 pin 1` | SAME70 to codec |
| ADC SDATA | `RD` on `PA10` | `J504 pin 2` | Codec to SAME70 |
| LRCK / frame | `RF` on `PD24`, `TF` on `PB0` | `J504 pin 1`, `J505 pin 7` or `J507 pin 5` | Clock board to SAME70/codecs |
| BCLK / bit clock | `RK` on `PA22`, `TK` on `PB1` | `J504 pin 3`, `J505 pin 8` or `J507 pin 4` | Clock board to SAME70/codecs |
| MCLK | No preferred SAME70 route | Direct to codec board | Clock board to codec |
| `XO_44_EN` | TBD GPIO | TBD | SAME70 to clock board |
| `XO_48_EN` | TBD GPIO | TBD | SAME70 to clock board |
| Reset/control GPIO | `PC17` | `EXT1 pin 10` | SAME70 to codec |
| Optional I2C control | `PA3`/`PA4` | `EXT1/EXT2 pins 11/12` or `J500 pins 9/10` | Shared bus, confirm before wiring |

Exact CS4272 package pins, pull-ups, mode straps, analog supplies, and reset
polarity remain board-design tasks. SAME70 Xplained header I/O is 3.3 V only.

## Bit-Perfect Claim

The first bit-perfect claim is only at the digital serial boundary:

- For USB OUT, the PCM sample sent by the host must equal the I2S/SSC word
  observed on `TD` before the codec DAC digital filter.
- For ADC USB IN, the I2S/SSC word captured from `RD` must equal the PCM sample
  delivered to the host after packet framing.

The codec DAC interpolation filter, ADC decimation filter, high-pass filter,
analog reconstruction path, and analog capture path are outside that strict
digital bit-perfect claim. They may be excellent, but they are not a byte-for-
byte PCM transport boundary.

## Firmware Bring-Up

1. Keep `audio hw` reporting no external codec on the stock board.
2. Add pin-mux/status reporting only, still with `external_codec=0`.
3. Prove external clocks at the header pins with a scope or logic analyzer.
4. Enable SAME70 SSC slave TX with silence and a deterministic sample pattern.
5. Decode `BCLK`/`LRCK`/`TD` on a logic analyzer and compare words against the
   source WAV or generated pattern.
6. Enable SAME70 SSC slave RX and compare `RD` words against USB IN samples.
7. Add a new external-codec acceptance gate separate from `board-ready`.

The first firmware mode should keep explicit USB feedback endpoint 4. Implicit
feedback can be tried only after ADC and DAC streams are proven to share the
same external clock domain and the ADC USB IN stream is valid enough to act as
the feedback source.

## References

- CS4272 product page and datasheet: https://www.cirrus.com/products/cs4272
- CS4272 datasheet PDF: https://statics.cirrus.com/pubs/proDatasheet/CS4272_F2.pdf
- PCM3060 product page: https://www.ti.com/product/PCM3060
- PCM3060 datasheet PDF: https://www.ti.com/lit/gpn/PCM3060
