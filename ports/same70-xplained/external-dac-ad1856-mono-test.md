# AD1856 Mono DAC Bring-Up Plan

This is the first DAC-specific experiment selected for the future external
codec path: one AD1856 for a mono output smoke test. It does not replace the
stock-board USB/generated-audio baseline, and firmware must keep reporting
`external_codec=0` until real external hardware is wired and verified.

The source datasheet is:

```text
/Users/cozy/Downloads/ad1856.pdf
```

## Scope

- Start with one AD1856 only. One chip is one DAC channel, so this is mono, not
  stereo/IQ.
- Keep 44.1 kHz / 16-bit as the natural first target. AD1856 is a 16-bit PCM
  DAC; 24-bit USB samples must be truncated, dithered, or kept out of this
  first gate.
- Treat AD1856 as a custom serial DAC, not an I2S DAC. It needs `DATA`, `CLK`,
  and `LE` rather than `MCLK`, `BCLK`, `LRCK`, and `SDATA`.
- Keep the AK5394 ADC path separate. The ADC still needs its own MCLK/BCLK/LRCK
  plan before analog SDR input can be claimed.

## AD1856 Signals

| AD1856 pin | Signal | Meaning | SAME70 candidate |
| --- | --- | --- | --- |
| 7 | `DATA` | 16-bit MSB-first serial data input | `TD` on `PD26`, `J502 pin 1` |
| 5 | `CLK` | Data clock input, rising edge clocks data | `TK` on `PB1`, `J505 pin 8` or `J507 pin 4` |
| 6 | `LE` | Low-going latch-enable pulse after 16 data bits | `TF` on `PB0`, `J505 pin 7` or `J507 pin 5` |
| 9 | `VOUT` | Voltage output | External analog low-pass/output stage |
| 3 / 8 | `+VL` / `-VL` | Logic supply rails | External bipolar supply, at least +/-5 V |
| 16 / 1 | `+VS` / `-VS` | Analog supply rails | External bipolar supply, at least +/-5 V |
| 2 / 12 | `DGND` / `AGND` | Digital/analog grounds | Common ground with careful return routing |

The datasheet states that AD1856 digital inputs are TTL and 5 V CMOS
compatible, so 3.3 V SAME70 outputs are suitable for the logic input thresholds.
Do not power the AD1856 from SAME70 `3V3`; the chip requires bipolar supplies.

## Timing Notes

AD1856 input data is a 16-bit two's-complement word, MSB first. Data is clocked
on the rising edge of `CLK`; after the 16th clock, a low-going `LE` pulse
updates the DAC input latch. The input clock can run up to 10 MHz.

This timing is not the same as normal I2S. For a one-chip mono test, the first
firmware or external-logic target should generate exactly the AD1856 word clock
and latch sequence before trying to share the path with a stereo/IQ formatter.

## Clocking Options

### Option A: SAME70-Generated Mono Smoke Test

SAME70 drives `TD`, `TK`, and `TF` as AD1856 `DATA`, `CLK`, and `LE`. This is
the simplest way to prove the chip, soldering, supply, and analog output path.
It is not the final low-jitter architecture.

### Option B: External Low-Jitter Formatter

An external low-jitter clock/formatter board creates AD1856 `CLK` and `LE`, and
SAME70 provides data in the same clock domain. This is the better path once the
ADC/DAC board is real, because DAC latch timing is the jitter-sensitive event.

Do not assume a normal `LRCK` signal can be wired directly to AD1856 `LE` until
the pulse polarity, width, and alignment after the 16th data bit are verified on
a scope.

## Bring-Up Order

1. Keep `external-codec-preflight` passing on the stock board.
2. Build or wire only the AD1856 mono board: bipolar supplies, ground,
   decoupling, and analog output filter.
3. Power the AD1856 board without SAME70 signal wires; verify rail voltage,
   current, and heating.
4. Wire common ground, then `DATA`, `CLK`, and `LE` through short leads or a
   small header harness.
5. Scope `CLK` and `LE` before connecting headphones, speakers, or downstream
   analog gear.
6. Start with generated silence, then a deterministic low-amplitude tone.
7. Add a new AD1856-specific acceptance gate. It must not be merged with
   `board-ready` until the external DAC output is physically verified.
