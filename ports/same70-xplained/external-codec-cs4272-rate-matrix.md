# CS4272 Rate And Bit-Depth Matrix

This is the target capability matrix for the future CS4272-class external codec
path. It is not the current stock-board USB descriptor. Until the external
codec and clock hardware exist, firmware must keep reporting
`external_codec=0`, `i2sc=not_configured`, and `needs_external_codec_board`.

## Codec Capability Boundary

The CS4272 codec supports I2S and left-justified serial audio up to 24-bit
words. In control-port mode, the DAC also supports right-justified 16-, 18-, 20-, and 24-bit
formats. The ADC/DAC sample-rate ranges are:

- Single speed: `4-50 kHz`.
- Double speed: `50-100 kHz`.
- Quad speed: `100-200 kHz`.

For the practical USB Audio path, the target is every codec bit depth: 16, 18,
20, and 24 valid PCM bits. The default serial transport should remain I2S or
left-justified, with 18/20/24-bit samples carried in 24-bit USB subframes when
the host allows it. Right-justified 16/18/20/24-bit DAC mode is a codec
capability, but it should stay a separate explicit test mode until there is a
concrete host format and bit-perfect verifier for it.

## Standard USB Target Matrix

| USB rate | Family | CS4272 speed | MCLK | MCLK/LRCK | BCLK/LRCK | XO select |
| --- | --- | --- | --- | --- | --- | --- |
| 8 kHz | 48k | single | 6.144 MHz | 768 | 64 | `xo_48_en` |
| 11.025 kHz | 44k1 | single | 8.4672 MHz | 768 | 64 | `xo_44_en` |
| 12 kHz | 48k | single | 9.216 MHz | 768 | 64 | `xo_48_en` |
| 16 kHz | 48k | single | 12.288 MHz | 768 | 64 | `xo_48_en` |
| 22.05 kHz | 44k1 | single | 11.2896 MHz | 512 | 64 | `xo_44_en` |
| 24 kHz | 48k | single | 12.288 MHz | 512 | 64 | `xo_48_en` |
| 32 kHz | 48k | single | 12.288 MHz | 384 | 64 | `xo_48_en` |
| 44.1 kHz | 44k1 | single | 11.2896 MHz | 256 | 64 | `xo_44_en` |
| 48 kHz | 48k | single | 12.288 MHz | 256 | 64 | `xo_48_en` |
| 88.2 kHz | 44k1 | double | 11.2896 MHz | 128 | 64 | `xo_44_en` |
| 96 kHz | 48k | double | 12.288 MHz | 128 | 64 | `xo_48_en` |
| 176.4 kHz | 44k1 | quad | 22.5792 MHz | 128 | 64 | `xo_44_en` |
| 192 kHz | 48k | quad | 24.576 MHz | 128 | 64 | `xo_48_en` |

Expose 16/18/20/24-bit valid PCM widths for every listed rate unless a host
compatibility test proves a narrower set is required. The 8 kHz, 11.025 kHz,
and 12 kHz rows require a programmable low-jitter family clock output; they are
not valid by simply dividing the fixed 11.2896 MHz or 12.288 MHz family clocks
at the codec.

## Full Codec Range Note

The codec's legal range is broader than the standard USB target matrix above.
Supporting every possible sample rate from 4 kHz to 200 kHz is a programmable clock-generator
problem, not just a two-fixed-XO problem. The future hardware
should treat `xo_44_en` and `xo_48_en` as family-select controls for a
low-jitter clock generator or muxed family clock tree that can emit the exact
MCLK required by the selected rate and CS4272 speed mode. It must not allow
both families to drive MCLK at the same time.

The firmware must also grow matching USB descriptors, sample-rate request
handling, feedback calculation, SSC/XDMAC packet sizing, and exact host-side
verifiers before any new rate or bit depth is claimed as supported.

## Current Firmware Boundary

Current firmware descriptors still expose only the already verified stock-board
USB modes:

- `44.1 kHz / 16-bit`.
- `48 kHz / 24-bit`.

The `external-codec-clock-model` target verifies the full standard matrix as a
plan and additionally checks the current firmware feedback bytes for those two
already advertised modes.
