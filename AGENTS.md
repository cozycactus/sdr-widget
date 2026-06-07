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
  requests, SDR Widget vendor feature requests, and basic UAC1 mute/volume and
  sample-rate control requests.
- USBHS isochronous endpoints 3 OUT, 4 feedback IN, and 5 audio IN are
  configured when the host sets configuration 1. Verified serial status shows
  `audio cfg=1 cfgok=0x00000038 ...`.
- USB audio loopback handlers are present: endpoint 3 stores output packets in
  a small byte ring, endpoint 4 returns feedback for the active rate, and
  endpoint 5 can send queued loopback bytes, a generated PCM24/PCM16 pattern, a
  deterministic square-wave PCM tone, a deterministic low-harmonic sine source,
  deterministic melody, or silence. Default source is loopback.
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
  after the no-change set.
- `make -C ports/same70-xplained board-verify` is the connected-board smoke
  gate. It builds the SAME70 firmware and CoreAudio probe, runs
  `widget-verify` with `--set-current-check`, then runs the quiet
  `audio-generated-verify` gate. Latest run passed with feature-store
  `writes=0` before/after the no-change set and zero mismatches for pattern,
  tone, sine, and melody; silence reported host `input_nonzero=0`, OUT
  `nonzero=0`, and IN `nonzero=0` at both formats.
- `make -C ports/same70-xplained board-ready` runs `board-verify` and then
  `audio-listen`; use it when the board should finish in the verified
  `audio melody` state instead of the smoke gate's final silence state. Latest
  run passed and ended on `audio melody`.
- `make -C ports/same70-xplained flash-ready` flashes the connected SAME70,
  waits briefly for USB re-enumeration, then runs `board-ready`. Latest run
  programmed and verified flash with OpenOCD, passed the control and generated
  audio gates, and ended on `audio melody`.
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
  counter checks. For non-loop sources, it passes `--silent-output` to the
  CoreAudio probe so the host USB OUT pattern cannot be heard as monitor noise
  while the generated USB IN stream is being verified. `audio-wav-verify.py`
  independently reopens dumped pattern, tone, sine, melody, or silence WAV files
  and compares their PCM samples against the deterministic firmware source
  stream.
  For `--source loop`, it compares the captured input WAV against the dumped
  host output WAV after latency alignment. Latest CD-rate loopback artifact run used
  `--source loop --rate 44100 --bits 16 --seconds 1 --output
  /tmp/same70-loop-cd-input.wav --output-wav /tmp/same70-loop-cd-output.wav`;
  both the live probe and disk WAV-pair verifier passed with
  `input_offset_samples=2804 compared_samples=85260 mismatches=0` and matching
  hash `0x9cf8dd957a3b65c2`. It printed `audio-capture: pass
  output=/tmp/same70-loop-cd-input.wav output_wav=/tmp/same70-loop-cd-output.wav`,
  and `file` identified both dumps as 16-bit stereo PCM at 44.1 kHz with
  176172 total bytes each.
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
  `0x400fe355405808e7`, played through `afplay`, and left the board on
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
  `audio sine`, `audio melody`, `audio silence`, `audio outdiag`,
  `audio outdiag reset`, `audio indiag`, `audio indiag reset`. Audio source
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
make -C ports/same70-xplained audio-listen
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
milestone is mapping the original SDR Widget audio pipeline onto SAME70
peripherals if matching hardware is available, or identifying the missing
external codec/ADC path.
