# CS4272 Control Port Plan

This is the control-port contract for the future CS4272-class external codec
board. It is not an enablement claim for the stock SAME70 Xplained board.
Until the codec board is connected and probed, firmware must keep reporting
`external_codec=0`, `i2sc=not_configured`, and `needs_external_codec_board`.

## Selected Interface

Use the CS4272 I2C control port first, not SPI.

Reasons:

- I2C uses only `SCL`, `SDA`, reset, and one fixed `AD0` strap.
- I2C registers are readable, so the first hardware probe can verify chip ID
  and register state.
- SPI control registers are write-only on CS4272.
- SPI mode is selected if `AD0/CS` sees a high-to-low transition after power
  up, so I2C is less fragile for early hand wiring.

## SAME70 Header Route

| CS4272 signal | SAME70 route | Board header | Notes |
| --- | --- | --- | --- |
| `SDA/CDIN` pin 12 | `PA3/TWD0` | `EXT1/EXT2 pin 11` or `J500 pin 9` | I2C data, shared with board devices |
| `SCL/CCLK` pin 11 | `PA4/TWCK0` | `EXT1/EXT2 pin 12` or `J500 pin 10` | I2C clock, shared with board devices |
| `AD0/CS` pin 13 | Strap low by default | Codec board local strap | 7-bit I2C address `0x10`; strap high gives `0x11` |
| `RST` pin 14 | `PC17` GPIO | `EXT1 pin 10` | Hold low until MCLK and supplies are valid |

Use pull-ups only to CS4272 `VL` / SAME70-compatible `3V3`. Do not use 5 V
pull-ups or any 5 V logic on SAME70 headers.

The default bus speed for bring-up is 100 kHz. A later 400 kHz mode is fine
after the first board passes readback and scope checks.

## Power-Up And Register Sequence

1. Hold CS4272 `RST` low.
2. Enable the selected external low-jitter MCLK family and wait for it to
   settle.
3. Configure SAME70 `TWIHS0` on `PA3/PA4`, but do not claim the codec is
   present until an I2C transaction succeeds.
4. Release `RST`.
5. Before the CS4272 10 ms start-up wait ends, write register `0x07 = 0x03`
   so `CPEN=1` and `PDN=1`. This enters control-port mode while keeping the
   codec powered down.
6. Program mode, format, and safe DAC/ADC defaults while `PDN=1`.
7. Write register `0x07 = 0x02` so `CPEN=1` and `PDN=0`.
8. Leave I2C static during active audio streaming except for explicit
   stop/mute/reconfigure/start transitions.

## Baseline Register Writes

The first mode uses CS4272 master serial clocks and SAME70 as the SSC-style
slave. The audio serial format is I2S for both DAC and ADC.

| Register | Value | Meaning |
| --- | --- | --- |
| `0x07` | `0x03` | Enter control-port mode and stay powered down |
| `0x01` | rate-dependent | Functional mode, MCLK/LRCK ratio, master mode, DAC I2S |
| `0x02` | `0x80` | Keep DAC auto-mute default, no de-emphasis, normal polarity |
| `0x03` | `0x50` | Keep default soft/zero-cross and normal stereo routing |
| `0x04` | `0x00` | DAC A unmuted, 0 dB digital volume |
| `0x05` | `0x00` | DAC B unmuted, 0 dB digital volume |
| `0x06` | `0x10` | ADC I2S, no ADC mute, high-pass filter enabled, no 16-bit dither |
| `0x07` | `0x02` | Release power-down with control-port mode enabled |

Do not map USB mute/volume controls to CS4272 registers for the bit-perfect
path. The current USB control state remains a host compatibility feature and is
not applied to sample bytes.

## Mode Control 1 Values

`Mode Control 1` is register `0x01`:

```text
bit 7:6  M1:M0       speed mode
bit 5:4  Ratio1:0    MCLK/LRCK ratio
bit 3    M/S         1 = CS4272 serial master
bit 2:0  DAC_DIF     001 = I2S up to 24-bit
```

Target values for the planned standard matrix:

| Rate | Speed | MCLK/LRCK | Register `0x01` |
| --- | --- | --- | --- |
| 8 kHz | single | 768 | `0x39` |
| 11.025 kHz | single | 768 | `0x39` |
| 12 kHz | single | 768 | `0x39` |
| 16 kHz | single | 768 | `0x39` |
| 22.05 kHz | single | 512 | `0x29` |
| 24 kHz | single | 512 | `0x29` |
| 32 kHz | single | 384 | `0x19` |
| 44.1 kHz | single | 256 | `0x09` |
| 48 kHz | single | 256 | `0x09` |
| 88.2 kHz | double | 128 | `0x89` |
| 96 kHz | double | 128 | `0x89` |
| 176.4 kHz | quad | 128 | `0xe9` |
| 192 kHz | quad | 128 | `0xe9` |

The 8 kHz, 11.025 kHz, and 12 kHz entries require the programmable low-jitter
clock source documented in `external-codec-cs4272-rate-matrix.md`; they are not
valid with only fixed 11.2896 MHz and 12.288 MHz oscillators.

## First Hardware Probe

The first connected-hardware firmware gate should be separate from `board-ready`
and should prove only control-plane access:

1. Keep `external_codec=0` until readback succeeds.
2. Hold `RST` low and verify the codec board draws safe current.
3. Release `RST`, write `0x07 = 0x03`, then read back register `0x07`.
4. Read chip ID register `0x08`; the useful first check is transaction success,
   not a unique part code.
5. Write and read back the baseline registers while the codec is still muted or
   powered down.
6. Only after this passes should clock-only and serial-audio gates be added.

## Sources

- CS4272 datasheet PDF: https://statics.cirrus.com/pubs/proDatasheet/CS4272_F2.pdf
