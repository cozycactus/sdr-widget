# AGENTS.md

Last updated: 2026-06-07

This repo is the legacy SDR Widget firmware tree. The original firmware targets
an AVR32 AT32UC3A3256; the currently connected bring-up board is an Atmel
SAME70 Xplained board with an ATSAME70Q21 Cortex-M7 MCU. Keep the AVR32 build
and the SAME70 port separate unless the user explicitly asks to merge paths.

## Maintenance

- Update this file when the verified board state, build/flash commands, serial
  port, or next bring-up milestone changes.
- Keep updates factual and short. Prefer measured or observed results over
  guesses.
- Do not revert unrelated dirty files or generated build output unless the user
  asks.

## Current Board State

The active hardware is the SAME70 Xplained board connected over the EDBG USB
interface. OpenOCD detects it with CMSIS-DAP/SWD as `atsame70q21`.

The SAME70 proof-of-life port lives in:

```sh
ports/same70-xplained
```

Verified firmware features:

- Green user LED heartbeat on PC8, active-low. The foreground loop is quiet
  after boot so blocking 9600-baud status writes do not interrupt USB polling.
- EDBG virtual COM status console on USART1 at 9600 8N1.
- USART1 RX is interrupt-driven with a small software ring buffer.
- Main clock is the external 12 MHz crystal with PLLA at 300 MHz CPU and
  150 MHz MCK; UPLL is enabled for USBHS.
- Cortex-M SysTick is enabled at 1 kHz.
- USBHS target-port device mode enumerates at high speed as
  `16c0:05dc Yoyodyne SDR-Widget`, serial `1.0.0.0.0.0.0`.
- USB endpoint 0 exposes the UAC1 SDR Widget composite descriptor: interface 0
  is DG8SAQ/vendor control, interfaces 1-3 are USB Audio Control/Streaming.
- macOS lists `Yoyodyne SDR-Widget` as a USB audio device with 2 input
  channels and 2 output channels. The SAME70 descriptor now exposes
  48 kHz/24-bit and 44.1 kHz/16-bit streaming modes.
- Endpoint 0 handles standard enumeration, interface alternate-setting
  requests, SDR Widget vendor feature requests, UAC1 sample-rate requests, and
  mutable UAC1 mute/volume GET_CUR/SET_CUR state for the mic and speaker
  feature units. The control state is intentionally not applied to sample bytes.
- USBHS isochronous endpoints 3 OUT, 4 feedback IN, and 5 audio IN are
  configured when the host sets configuration 1. Verified serial status shows
  `audio cfg=1 cfgok=0x00000038 ...`.
- USB audio loopback handlers are present: endpoint 3 stores output packets in
  a small byte ring, endpoint 4 returns feedback for the active rate, and
  endpoint 5 can send queued loopback bytes, a generated PCM24/PCM16 pattern, a
  deterministic square-wave PCM tone, a deterministic low-harmonic sine source,
  deterministic melody, or silence. Default source is loopback.
- The SAME70 port does not yet drive the original external codec path. The
  `audio hw` serial command, also wrapped by `make -C ports/same70-xplained
  audio-hw`, reports `external_codec=0`, `i2sc=not_configured`, and
  `needs_external_codec_board`. The future ADC target is an original-style
  AK5394 ADC-board interface. The selected first DAC bring-up is one AD1856
  mono output test; ES9023 remains the original DAC reference path. AD1856 is
  not I2S and needs DATA/CLK/LE plus external bipolar supplies, so one chip is
  a mono smoke test only, not stereo/IQ. The preferred AD1856 low-jitter path
  adds an external formatter that owns AD1856 DATA/CLK/LE; SAME70 sends sample
  bits on TD and accepts external TK/TF timing. The future stock-board wiring
  plan still uses an external low-jitter clock board as ADC/DAC or formatter
  master, with SAME70 as an SSC-style RX/TX slave for the final path:
  DA_SDATA/TD on PD26/J502.1, AD_SDATA/RD on PA10/J504.2, AD_LRCK or
  AD_FSYNC/RF on PD24/J504.1, AD_SCLK/RK on PA22/J504.3, and DA_SCLK/TK plus
  DA_LRCK/TF on PB1/PB0 via J505 or J507. Direct TD/TK/TF to AD1856 is only a
  simple mono smoke test after timing is scoped, not the low-jitter target.
  External MCLK is expected to feed ADC/DAC or formatter logic directly; PCK0
  on PB13 is reserved for optional diagnostics, not codec MCLK. Details live in
  `ports/same70-xplained/external-codec-original-uc3a3-map.md`,
  `ports/same70-xplained/external-codec-low-jitter-clock-plan.md`,
  `ports/same70-xplained/external-codec-pin-map.md`, and
  `ports/same70-xplained/external-dac-ad1856-mono-test.md`,
  `ports/same70-xplained/external-dac-ad1856-low-jitter-formatter.md`. The
  status seam is
  `same70_audio_hw.c`; latest `audio-hw` still passes while reporting
  `external_codec=0`,
  `UC3A3_AK5394_ADC_AD1856_MONO_DAC_TEST_ES9023_REFERENCE`,
  `external_low_jitter_formatter_owns_ad1856_data_clk_le_slave_ssc_explicit_fb_tbd`,
  and `needs_external_codec_board`.
- The practical preferred first full ADC+DAC board is now a CS4272-class I2S
  codec route with external low-jitter MCLK/BCLK/LRCK, SAME70 as SSC-style
  slave RX/TX, and explicit USB feedback. This is documented in
  `ports/same70-xplained/external-codec-i2s-cs4272-bitperfect.md`. The strict
  bit-perfect claim for that route is only at the digital I2S/SSC pins before
  codec digital filters; analog output/input is not claimed bit-perfect. This
  is still documentation/preflight only: stock-board firmware must keep
  reporting no external codec until hardware is wired and verified.
- The macOS CoreAudio HAL stream probe target has verified USB-level active
  streaming against the connected board at both advertised formats. Latest
  48 kHz/24-bit check was `summary mode=loopback rate=48000 bits=24 runs=1
  passed=1 failed=0 compared_samples=189736 mismatches=0` with
  `input_offset_samples=2776`. Latest CD-rate check was `summary mode=loopback
  rate=44100 bits=16 runs=5 passed=5 failed=0 compared_samples=866444
  mismatches=0`, with `input_offset_samples=2804` on the first three runs and
  `2892` on the last two.
  Final serial counters after switching back to 48 kHz/24-bit showed
  `peak_alt=0x0000000c`, `audio ... out=14108/2938948 fb=2/8
  in=14284/2975792 err=0`, `under=0`, `stall=0`, `drop=0`,
  `audio loop=0/1152`, and `fmt=48k24/48k24`.
- `audio pattern` plus exact host-side pattern verification now passes at both
  advertised formats. `--seconds 2 --runs 2 --rate 48000 --bits 24 --verify
  pattern` reported `summary mode=pattern rate=48000 bits=24 runs=2 passed=2
  failed=0 compared_samples=384000 mismatches=0`, with expected-pattern offsets
  `3040` and `2864`. `--seconds 2 --runs 2 --rate 44100 --bits 16 --verify
  pattern` reported `summary mode=pattern rate=44100 bits=16 runs=2 passed=2
  failed=0 compared_samples=352256 mismatches=0`, with expected-pattern offsets
  `2688` and `2504`. The board was switched back to `audio loop`, and a final
  1-second 48 kHz/24-bit loopback check passed with zero mismatches. Final
  serial status showed `source=loop`, `fmt=48k24/48k24`, `err=0`, `under=0`,
  `drop=0`, and `stall=0`.
- `widget-control -a`, `-d`, `-g`, `-m`, `-l`, and `-r` have been verified
  against the connected SAME70 board. `-r` now performs a real software reset;
  the device re-enumerates afterward and `-d` still returns defaults.
- `make -C ports/same70-xplained widget-verify` is the repeatable USB control
  gate. The default read-only run builds `widget-control`, verifies enumeration
  as `16c0:05dc 1.0.0.0.0.0.0`, checks matching default/NVRAM/RAM feature
  output, and checks the feature listing. `WIDGET_VERIFY_ARGS="--reset-check"`
  also exercises `widget-control -r` and waits for default readback after
  re-enumeration. `WIDGET_VERIFY_ARGS="--set-current-check"` sends the current
  feature table through `widget-control -s` to cover SET_NVRAM without changing
  values, checks serial `usb` status before/after, and fails if feature-store
  `writes` changes. The host tool now checks that every `-s` response byte
  equals the requested value. Latest guarded run reported `writes=0` before and
  after the no-change set. Shared serial host helpers flush stale console input
  immediately before sending each command, avoiding old reset/status fragments.
- `make -C ports/same70-xplained uac-verify` builds `uac-control`, opens the
  connected `16c0:05dc` device with libusb, verifies mic/speaker mute plus
  left/right volume SET_CUR/GET_CUR round trips, restores all values to zero,
  and checks serial `audio ctl` for `mic_mute=0`, `spk_mute=0`, and zero
  mic/speaker volumes. Latest run passed inside `flash-ready`.
- `make -C ports/same70-xplained board-verify` is the connected-board smoke
  gate. It builds the SAME70 firmware and CoreAudio probe, runs
  `widget-verify` with `--set-current-check`, runs `uac-verify`, runs the quiet
  `audio-generated-verify` gate, then runs `audio-hw` to assert the current
  external-codec boundary. Latest run passed with feature-store `writes=0`
  before/after the no-change set, UAC mute/volume round trips restored to zero,
  zero mismatches for pattern, tone, sine, and melody, silence host
  `input_nonzero=0`, OUT `nonzero=0`, IN `nonzero=0` at both formats, and
  `audio-hw` reporting
  `UC3A3_AK5394_ADC_AD1856_MONO_DAC_TEST_ES9023_REFERENCE`,
  `AD1856=mono_DATA_TD_CLK_TK_LE_TF`, and `needs_external_codec_board`.
- `make -C ports/same70-xplained external-codec-preflight` is the pre-wiring
  guard for future codec work. It checks the UC3A3 original map, the AD1856
  mono DAC plan, and `ports/same70-xplained/external-codec-pin-map.md` for the
  selected header route and 3.3 V/no-generic-USB-I2S guardrails, then runs
  `audio-hw` to prove the stock board still reports no external codec. Latest
  run passed after flashing the updated status text and reported
  `external codec remains disabled`.
- `make -C ports/same70-xplained external-codec-original-map` is the offline
  schematic-source gate for the UC3A3 AK5394 ADC-board plus ES9023 DAC signal
  model. It checks `ports/same70-xplained/external-codec-original-uc3a3-map.md`
  for the local UC3A3 PDF source set, J303/J304 nets, ES9023 I2S nets, the
  AD1856 mono DAC deviation, legacy firmware pin evidence, and the note that
  PCK0 is not the external codec MCLK source for the low-jitter plan. Latest
  run passed.
- `make -C ports/same70-xplained external-codec-clock-plan` is the offline
  low-jitter full-duplex clocking gate. It checks
  `ports/same70-xplained/external-codec-low-jitter-clock-plan.md` for external
  ADC/DAC MCLK, shared external BCLK/LRCK into SAME70 RK/RF and TK/TF, explicit
  USB feedback endpoint 4 as the first firmware mode, no firmware resampling,
  and the preferred AD1856 formatter-owned DATA/CLK/LE timing model. Latest run
  passed.
- `make -C ports/same70-xplained external-dac-ad1856-formatter` is the offline
  gate for the preferred AD1856 low-jitter formatter plan. It checks that
  SAME70 does not generate AD1856 `CLK`/`LE`, that formatter timing comes from
  the external XO domain, that the left channel is used exactly for the first
  mono proof, that explicit USB feedback remains required, and that the HDL
  starter path is documented. Latest run passed.
- `make -C ports/same70-xplained external-codec-i2s-cs4272` is the offline gate
  for the practical CS4272-class I2S codec plan. It checks the no-external-codec
  firmware boundary, external low-jitter clock ownership, SAME70 slave
  RX/TX direction, explicit USB feedback, no DSP/resampling, logic-analyzer
  bit-perfect proof at the I2S/SSC pins, and source links. Latest run passed.
- `make -C ports/same70-xplained ad1856-formatter-sim` runs the generic Verilog
  starter testbench for the AD1856 low-jitter formatter. The HDL lives in
  `ports/same70-xplained/hdl/ad1856_formatter`; it targets an 11.2896 MHz XO,
  generates SAME70 `TK`/`TF`, captures the left 16-bit sample from `TD`, and
  emits AD1856 `DATA`/gated `CLK`/low-going `LE`. Latest simulation passed
  six known sample words plus strict timing checks for `same_tk`, `same_tf`,
  AD1856 `CLK`, `DATA`, and `LE`.
- `make -C ports/same70-xplained ad1856-formatter-vcd` runs the same formatter
  test with waveform dumping and writes
  `ports/same70-xplained/hdl/ad1856_formatter/build/ad1856_formatter.vcd`.
- `make -C ports/same70-xplained ad1856-formatter-synth` runs a
  board-neutral iCE40/Yosys synthesis sanity check for the same HDL. OSS CAD
  Suite is installed locally at `/Users/cozy/cozycactus/oss-cad-suite`; this
  target proves the Verilog lowers into tiny-FPGA primitives but does not make
  a final bitstream until the exact external logic board and pin constraints
  are selected.
- `make -C ports/same70-xplained ad1856-formatter-board-synth` synthesizes the
  no-debug board wrapper in `hdl/ad1856_formatter/ad1856_formatter_board.v`.
  This is the top-level intended for the first real iCE40 bitstream.
- `make -C ports/same70-xplained ad1856-formatter-bitstream-help` prints the
  required iCE40 bitstream variables. The bitstream skeleton requires a
  board-specific PCF made from
  `ports/same70-xplained/hdl/ad1856_formatter/constraints/ad1856_formatter-ice40.pcf.example`.
- The selected first external formatter board is UPduino v3.1. The wiring plan
  lives in `ports/same70-xplained/external-dac-ad1856-upduino-v31.md`, the PCF
  starter is
  `ports/same70-xplained/hdl/ad1856_formatter/constraints/upduino-v3.1-ad1856.pcf.example`,
  `make -C ports/same70-xplained external-dac-ad1856-upduino-v31` checks the
  table, and `make -C ports/same70-xplained
  ad1856-formatter-upduino-v31-bitstream` builds the starter UPduino
  `up5k/sg48` bitstream. Latest UPduino starter bitstream build passed
  `nextpnr-ice40` timing for the 11.2896 MHz `clk_xo` target and wrote
  `hdl/ad1856_formatter/build/ad1856_formatter-ice40.bin`.
- `make -C ports/same70-xplained external-codec-wiring-checklist` is the
  offline checklist gate for future external-codec wiring. It verifies
  `ports/same70-xplained/external-codec-wiring-checklist.md`, which captures
  stop conditions, stock-board baseline, codec-board selection, unpowered
  checks, power-only checks, reset/control, clock-only probe, serial data
  bring-up, AD1856 bipolar-supply and formatter-owned `LE` timing stop
  conditions, and the requirement for a separate external-codec acceptance
  gate. Latest run passed.
- `make -C ports/same70-xplained board-ready` runs `board-verify` and then
  `audio-listen`; use it when the board should finish in the verified
  `audio melody` state instead of the smoke gate's final silence state. Latest
  run passed, exact-verified `/tmp/same70-melody-listen.wav` with
  `compared_samples=353280 mismatches=0` and matching hash
  `0x0c0d418c1965963b`, played it through `afplay`, and ended on
  `audio melody`.
- `make -C ports/same70-xplained flash-ready` flashes the connected SAME70,
  waits briefly for USB re-enumeration, then runs `board-ready`. Latest run
  programmed and verified flash with OpenOCD, passed the widget/UAC control and
  generated-audio gates, verified `/tmp/same70-melody-listen.wav` with
  `compared_samples=353280 mismatches=0` and matching hash
  `0xab4b79884dabf283`, played it through `afplay`, and ended on
  `audio melody`.
- After macOS audio enumeration and `widget-control -d`, the serial `usb`
  command has been verified with `stall=0`.
- Feature "NVRAM" on the SAME70 port is now backed by an append-only 512-byte
  flash page at `0x004ffe00`, reserved out of the linker `FLASH` region.
  Records are 32 bytes, so the page has 16 slots; unchanged `widget-control -s`
  values are skipped to avoid burning slots. The latest persistence test set
  only `log=1sec`, verified `widget-control -g` after reset, restored
  `log=500ms`, reset again, and verified default readback. Serial `usb` reports
  `feature store loaded=1 valid=1`.
- The HAL stream probe opens the widget, drives SET_INTERFACE/endpoint traffic,
  selects the requested nominal sample rate, writes exact integer test samples
  through CoreAudio Float32 buffers, aligns the returned stream latency, and
  verifies zero sample mismatches through the loopback path. `--verify pattern`
  aligns captured input against the firmware-generated LCG pattern for exact
  capture-side checks, `--verify tone` matches the firmware square-wave source,
  `--verify sine` matches the deterministic sine lookup source for less harsh
  listening checks, `--verify melody` matches the generated note phrase, and
  `--verify silence` proves the selected source returns quantized zero samples
  only. `--verify input` remains a looser nonzero activity check. For exact
  loopback, pattern, tone, sine, melody, and silence modes, the probe now prints
  64-bit `expected_hash` and `actual_hash` values over the quantized samples
  actually compared, giving an audit-friendly fingerprint for bit-perfect
  checks. With
  `--dump-input-wav FILE --runs 1`, it also writes the quantized captured input
  stream as a PCM WAV artifact for inspection/listening; with
  `--dump-output-wav FILE --runs 1`, it also writes the quantized host output
  stream so loopback can be rechecked later from a WAV pair. Serial `usb`
  counters remain the source of truth for hardware endpoint state.
- `make -C ports/same70-xplained audio-verify` now runs the serial-controlled
  full audio gate: loopback at 48 kHz/24-bit and 44.1 kHz/16-bit, generated
  pattern at both formats, generated tone at both formats, generated sine at
  both formats, generated melody at both formats, silence at both formats, final
  switch back to `audio loop`, final 48 kHz/24-bit loopback, and serial status
  confirmation for
  `source=loop` plus `fmt=48k24/48k24`. It also captures a starting serial
  `usb` status and audits final counters. `stall`, `crc`, `over`, and `drop`
  increases are hard failures; `under` increases from CoreAudio stream restarts
  are allowed when `err` increases by no more than the same amount. Pass
  `--strict-serial-counter-check` to fail on those restart underflows too.
  The gate also resets `audio outdiag` before each host probe and checks it
  afterward: loopback probes must show nonzero endpoint-3 OUT payload, and
  generated-source probes use `--silent-output` and must show `nonzero=0` on the
  device. Pass `--skip-outdiag-check` to run against older firmware without the
  resettable OUT diagnostics. Use `--skip-final-loopback` for quiet
  generated-source checks that should leave the last selected source active, and
  pair it with `--skip-serial-counter-check` when final counter auditing is not
  useful. Latest quiet sine-only run used `--seconds 1
  --skip-loopback --skip-pattern --skip-tone --skip-silence
  --skip-final-loopback --skip-serial-counter-check --sine-runs 1`; both
  48 kHz/24-bit and 44.1 kHz/16-bit passed with `output_nonzero=0` and
  `mismatches=0`, and final status reported `source=sine fmt=44k16/44k16`.
  `make -C ports/same70-xplained audio-generated-verify` is the shorter quiet
  gate for all generated input sources; it verifies pattern, tone, sine, melody,
  and silence at both formats with silent USB OUT, no final loopback, and no
  serial counter audit. Current generated gate passes: pattern, tone, sine, and
  melody are exact host-captured checks with zero mismatches at both advertised
  formats, and the silence source is checked with `--verify start` plus
  `audio outdiag`/`audio indiag` because macOS can suppress client callbacks for
  sustained all-zero generated input. Latest silence diagnostics showed
  `nonzero=0` for both OUT and IN at both formats, with IN byte counts `901448`
  at 48 kHz/24-bit and `804144` at 44.1 kHz/16-bit.
  Latest full gate status:
  `make -C ports/same70-xplained audio-verify
  AUDIO_VERIFY_ARGS="--seconds 1"` passes with `hog_mode=1` and zero
  mismatches for exact loopback, pattern, tone, sine, melody, and silence
  checks at both advertised formats, plus the final 48 kHz/24-bit loopback.
  Loopback summaries were `rate=48000 bits=24 runs=2 passed=2 failed=0
  compared_samples=186960 mismatches=0` and `rate=44100 bits=16 runs=2
  passed=2 failed=0 compared_samples=170520 mismatches=0`; generated-source
  summaries all reported zero mismatches. Matching melody hashes were
  `0x9ef75feb9a6e2b27` at 48 kHz/24-bit and `0x90c29364843c4d2b` at
  44.1 kHz/16-bit. Silence used `--verify start`; it reported host
  `input_nonzero=0`, device OUT `nonzero=0`, and device IN `nonzero=0` at both
  formats, with IN byte counts `885776` at 48 kHz/24-bit and `794124` at
  44.1 kHz/16-bit. Final serial status stayed at `source=loop`,
  `fmt=48k24/48k24`, `stall=0`, `crc=0`, `over=0`, `drop=0`, and paired
  restart underflow counters `err=118 under=118`, which the gate accepts.
- Latest WAV capture check selected `audio tone`, then ran
  `make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 1
  --runs 1 --rate 48000 --bits 24 --verify tone --dump-input-wav
  /tmp/same70-tone-48k24.wav"`. The exact tone verifier passed with
  `compared_samples=97280 mismatches=0 expected_hash=0xb89840f63a4ab703
  actual_hash=0xb89840f63a4ab703`, and `file` identified the dump as 24-bit
  stereo PCM at 48 kHz with 291884 total bytes.
- `make -C ports/same70-xplained audio-capture` wraps the WAV dump path with
  serial source selection, exact verification, WAV verification from disk, final
  `audio loop` restore unless `--leave-source` is passed, and final serial
  counter checks. As in `audio-verify`, paired `err`/`under` increases from
  CoreAudio stream restarts are accepted unless `--strict-serial-counter-check`
  is passed. For non-loop sources, it passes `--silent-output` to the CoreAudio
  probe so the host USB OUT pattern cannot be heard as monitor noise while the
  generated USB IN stream is being verified. `audio-wav-verify.py` independently
  reopens dumped pattern, tone, sine, melody, or silence WAV files and compares
  their PCM samples against the deterministic firmware source stream.
  For `--source loop`, it compares the captured input WAV against the dumped
  host output WAV after latency alignment. `make -C ports/same70-xplained
  audio-cd-bitperfect` is the named 44.1 kHz/16-bit loopback artifact proof,
  and `make -C ports/same70-xplained audio-cd-ready` runs that proof before
  restoring the verified `audio melody` listening state. Latest `audio-cd-ready`
  run captured `/tmp/same70-loop-cd-input.wav` and
  `/tmp/same70-loop-cd-output.wav`; both the live probe and disk WAV-pair
  verifier passed with `input_offset_samples=2804 compared_samples=174348
  mismatches=0` and matching hash `0x21d4ae0f385bce93`. `file` identified both
  dumps as 16-bit stereo PCM at 44.1 kHz with 354348 total bytes each.
- `audio melody` is the preferred listening sanity source; it steps the existing
  low-harmonic sine lookup through a short original note phrase with a small
  attack/release envelope, emits mono-in-stereo, and loops continuously.
  `audio sine` remains the exact deterministic one-note source; `audio pattern`
  is intentionally noise-like. If live monitoring sounds noisy, run
  `make -C ports/same70-xplained audio-listen`; it captures, exact-verifies the
  melody WAV against the generated note phrase, plays it through `afplay`, keeps
  USB OUT silent during capture, and leaves the board on `audio melody`.
  Latest checked `audio-listen` run wrote `/tmp/same70-melody-listen.wav`,
  passed live exact verification plus disk WAV verification with
  `compared_samples=353280 mismatches=0` and matching hash
  `0x0c0d418c1965963b`, played through `afplay`, and left the board on
  `audio melody`.
  Latest exact melody gate:
  `make -C ports/same70-xplained audio-verify AUDIO_VERIFY_ARGS="--seconds 1
  --skip-loopback --skip-pattern --skip-tone --skip-sine --skip-silence
  --skip-final-loopback --skip-serial-counter-check --melody-runs 1"` passed at
  both formats with matching live hashes: 48 kHz/24-bit
  `expected_hash=0x76e0eff7c151b947 actual_hash=0x76e0eff7c151b947`, and
  44.1 kHz/16-bit
  `expected_hash=0x3186e5c43407cda7 actual_hash=0x3186e5c43407cda7`.
  Latest `audio-capture --source melody --rate 44100 --bits 16` also passed
  live exact verification plus disk WAV verification on
  `/tmp/same70-melody-exact.wav` with hash `0x9d6ac91ff2387a43`.
- Audio diagnostics now include byte totals from USBHS BYCT, last/max OUT
  packet sizes, and short/CRC/overflow/underflow counters. Short OUT packets
  are expected for the observed 288-byte packets under the 294-byte endpoint
  maximum. IN underflow counting now ignores inactive alternate settings; the
  last verification saw zero active IN underflows after two consecutive
  stream-probe runs. Endpoint 4 feedback IN and endpoint 5 audio IN are refilled
  from USBHS RWALL/NBUSYBK state, and diagnostics report `fb_busy=<last>/<max>`
  plus `in_busy=<last>/<max>`. Loopback diagnostics report current/peak ring
  fill, dropped OUT bytes, inserted silence bytes, and selected formats as
  `fmt=<out>/<in>`. The loopback input prebuffers whole packets after stream
  resets so startup silence is skipped cleanly by the bit-perfect verifier.
  `audio outdiag reset`/`audio outdiag` expose resettable endpoint-3 OUT
  diagnostics: packets, bytes, nonzero bytes, FNV-1a hash, last packet length,
  first 256 stream bytes, and first 256 bytes starting at the first nonzero OUT
  payload. `audio indiag reset`/`audio indiag` expose the same shape for
  endpoint-5 IN bytes recorded by the generated silence source.
- Console commands: `?`, `help`, `status`, `clk`, `usb`, `usb init`,
  `usb attach`, `usb detach`, `audio loop`, `audio pattern`, `audio tone`,
  `audio sine`, `audio melody`, `audio silence`, `audio hw`, `audio ctl`,
  `audio outdiag`, `audio outdiag reset`, `audio indiag`, `audio indiag reset`.
  Audio source
  commands now print a terse `audio source=<source>` line instead of the full
  `usb` status block, so they are safer while a host audio stream is active.

Known SAME70/board quirks:

- EDBG VCOM uses PA21/RXD1 and PB4/TXD1.
- PB4 resets as JTAG `TDI`; set `MATRIX_CCFG_SYSIO.SYSIO4` before muxing PB4
  to peripheral D for TXD1.
- The proven macOS serial device in this setup is `/dev/cu.usbmodem1462302`.
- USB enumeration requires a second cable on the SAME70 target USB connector;
  the EDBG/debug USB connector only provides SWD and the serial console.

## Commands

Build:

```sh
make -C ports/same70-xplained
```

Legacy AVR32 build:

```sh
make all
```

`make-widget` now auto-detects `avr32-gcc` from `AVR32BIN`, `PATH`, the
sibling `avr32-toolchain-macos-arm64` source-build tree, or the original Linux
Atmel Studio locations. `widget-control` builds with `pkg-config` libusb flags.
`etc/program-widget` accepts `OBJCOPY`, `DFU_PROGRAMMER`, and `MCU` overrides.
Latest `make clean && make all` passed on macOS using the sibling toolchain.

Probe:

```sh
make -C ports/same70-xplained probe
```

Flash connected SAME70 board:

```sh
make -C ports/same70-xplained flash
```

Open the serial console:

```sh
screen /dev/cu.usbmodem1462302 9600
```

Verified `status` output shape:

```text
status tick=<n> uptime_ms=<n> us_csr=0x00f00000
```

Verified `usb init` output shape:

```text
usb init=1 attached=1
pmc_sr=0x0103ff4b pmc_usb=0x00000901 pcsr1=0x00000004
ctrl=0x02008000 sr=0x00005c03 devctrl=0x0000009d
events reset=1 setup=<n> tx=<n> rxout=<n> stall=0
ctrl address=<n> config=1 ep0_state=0 desc=<n> set_addr=1 set_cfg=1 set_int=<n> alt=0x00000000 peak_alt=0x0000000c last_int=<i>:<alt>
audio cfg=1 set_int=<n> cfgok=0x00000038 out=<n>/<bytes> fb=<n>/<bytes> in=<n>/<bytes> err=<n>
audio last_out=<bytes> max_out=<bytes> outnz=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
audio loop=<level>/<peak> drop=<bytes> silence=<bytes> source=<loop|pattern|tone|sine|melody|silence> fmt=<48k24|44k16>/<48k24|44k16>
```

Verified host checks:

```sh
./widget-control -a
./widget-control -d
./widget-control -g
./widget-control -m
./widget-control -l
./widget-control -r
system_profiler SPAudioDataType
make -C ports/same70-xplained stream-probe
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 10"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --runs 3"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --runs 5 --rate 44100 --bits 16"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --verify input"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --verify pattern"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --verify tone"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --verify sine"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 2 --verify melody --silent-output"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 1 --runs 1 --rate 48000 --bits 24 --verify tone --dump-input-wav /tmp/same70-tone-48k24.wav"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 1 --runs 1 --rate 44100 --bits 16 --verify loopback --dump-input-wav /tmp/same70-loop-cd-input.wav --dump-output-wav /tmp/same70-loop-cd-output.wav"
make -C ports/same70-xplained audio-verify
make -C ports/same70-xplained audio-generated-verify
make -C ports/same70-xplained audio-capture
make -C ports/same70-xplained audio-cd-bitperfect
make -C ports/same70-xplained audio-cd-ready
make -C ports/same70-xplained audio-listen
make -C ports/same70-xplained audio-hw
make -C ports/same70-xplained external-codec-original-map
make -C ports/same70-xplained external-codec-clock-plan
make -C ports/same70-xplained external-codec-i2s-cs4272
make -C ports/same70-xplained external-codec-wiring-checklist
make -C ports/same70-xplained external-codec-preflight
make -C ports/same70-xplained external-dac-ad1856-upduino-v31
make -C ports/same70-xplained ad1856-formatter-sim
make -C ports/same70-xplained ad1856-formatter-vcd
make -C ports/same70-xplained ad1856-formatter-synth
make -C ports/same70-xplained ad1856-formatter-board-synth
make -C ports/same70-xplained ad1856-formatter-bitstream-help
make -C ports/same70-xplained ad1856-formatter-upduino-v31-bitstream
make -C ports/same70-xplained uac-verify
make -C ports/same70-xplained board-verify
make -C ports/same70-xplained board-ready
make -C ports/same70-xplained flash-ready
make -C ports/same70-xplained widget-verify
make -C ports/same70-xplained widget-verify WIDGET_VERIFY_ARGS="--set-current-check"
make -C ports/same70-xplained widget-verify WIDGET_VERIFY_ARGS="--reset-check"
make -C ports/same70-xplained audio-capture AUDIO_CAPTURE_ARGS="--source sine --rate 44100 --bits 16 --seconds 1 --output /tmp/same70-sine-cd-capture.wav --skip-serial-counter-check"
make -C ports/same70-xplained audio-wav-verify AUDIO_WAV_VERIFY_ARGS="/tmp/same70-loop-cd-input.wav --source loop --rate 44100 --bits 16 --expected-wav /tmp/same70-loop-cd-output.wav"
```

Typical feature output:

```text
10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms
```

## Porting Direction

The original AVR32 firmware depends on AVR32-specific ASF components including
USBB, PDCA, TWIM, SSC, FLASHC, and the AVR32 FreeRTOS port. The SAME70 USB audio
source gate now covers loopback, generated LCG pattern, generated square-wave
tone, generated sine, generated melody, and silence. The next practical SAME70
milestone is keeping the stock-board USB path verified while preparing the
header-based, full-duplex, external-low-jitter-clock projection of the UC3A3
AK5394 ADC-board plus the selected AD1856 mono-first DAC test. The
`external-codec-original-map` target checks the schematic-source map,
`external-codec-clock-plan` checks the external MCLK/direct-to-codecs and
shared BCLK/LRCK slave-SSC plan plus AD1856 timing caveat,
`external-codec-i2s-cs4272` checks the practical CS4272-class I2S codec
bit-perfect digital-boundary plan,
`external-dac-ad1856-mono` checks the mono DAC planning doc,
`external-dac-ad1856-formatter` checks the preferred AD1856 low-jitter
formatter plan,
`external-codec-wiring-checklist` is an offline staged wiring hold list, and
`external-codec-preflight` is a live pre-wiring boundary check only. Do not
enable or claim analog SDR input/output until external hardware is connected
and a separate external-codec acceptance gate proves real ADC/DAC movement.
