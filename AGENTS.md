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
  or silence. Default source is loopback.
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
- After macOS audio enumeration and `widget-control -d`, the serial `usb`
  command has been verified with `stall=0`.
- Feature "NVRAM" on the SAME70 port is currently a volatile RAM
  compatibility table, not persistent SAME70 flash storage.
- The HAL stream probe opens the widget, drives SET_INTERFACE/endpoint traffic,
  selects the requested nominal sample rate, writes exact integer test samples
  through CoreAudio Float32 buffers, aligns the returned stream latency, and
  verifies zero sample mismatches through the loopback path. `--verify pattern`
  aligns captured input against the firmware-generated LCG pattern for exact
  capture-side checks, `--verify tone` matches the firmware square-wave source,
  `--verify sine` matches the deterministic sine lookup source for less harsh
  listening checks, and `--verify silence` proves the selected source returns
  quantized zero samples only. `--verify input` remains a looser nonzero
  activity check. For exact loopback, pattern, tone, sine, and silence modes,
  the probe now prints 64-bit `expected_hash` and `actual_hash` values over the
  quantized samples actually compared, giving an audit-friendly fingerprint for
  bit-perfect checks. With
  `--dump-input-wav FILE --runs 1`, it also writes the quantized captured input
  stream as a PCM WAV artifact for inspection/listening; with
  `--dump-output-wav FILE --runs 1`, it also writes the quantized host output
  stream so loopback can be rechecked later from a WAV pair. Serial `usb`
  counters remain the source of truth for hardware endpoint state.
- `make -C ports/same70-xplained audio-verify` now runs the serial-controlled
  full audio gate: loopback at 48 kHz/24-bit and 44.1 kHz/16-bit, generated
  pattern at both formats, generated tone at both formats, generated sine at
  both formats, silence at both formats, final switch back to `audio loop`,
  final 48 kHz/24-bit loopback, and serial status confirmation for
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
  gate for all generated input sources; it verifies pattern, tone, sine, and
  silence at both formats with silent USB OUT, no final loopback, and no serial
  counter audit. Latest run passed with `output_nonzero=0` on every probe and
  `mismatches=0` for pattern, tone, sine, and silence at both advertised
  formats, with `hog_mode=1` on the host probe; the target ends on
  `audio silence`, then the board was manually set back to `audio sine`.
  Latest full gate status: `make -C ports/same70-xplained audio-verify
  AUDIO_VERIFY_ARGS="--seconds 1"` passes with zero mismatches for exact
  loopback, pattern, tone, sine, and silence at both advertised formats, plus a
  final 48 kHz/24-bit loopback check. Final serial status reported `err=0`,
  `crc=0`, `over=0`, `under=0`, `drop=0`, and `stall=0`. The latest checked run
  reported nonzero OUT payload for loopback (`nonzero=561723` at 48 kHz/24-bit
  and `nonzero=342917` at 44.1 kHz/16-bit) and `nonzero=0` for every pattern,
  tone, sine, and silence probe at both formats. Focused `audio outdiag`
  diagnostics also showed silent host output is zero at the device at both
  advertised formats, while exact loopback passes at both formats. The board was
  manually set back to `audio sine` afterward. Earlier hash-audited default run
  reported
  `audio-verify: pass`, with
  loopback summaries `rate=48000 bits=24 runs=2 passed=2 failed=0
  compared_samples=378448 mismatches=0` and `rate=44100 bits=16 runs=2
  passed=2 failed=0 compared_samples=346648 mismatches=0`, plus pattern
  summaries `rate=48000 bits=24 runs=2 passed=2 failed=0 compared_samples=384000
  mismatches=0` and `rate=44100 bits=16 runs=2 passed=2 failed=0
  compared_samples=353280 mismatches=0`, plus tone summaries
  `rate=48000 bits=24 runs=1 passed=1 failed=0 compared_samples=192512
  mismatches=0` and `rate=44100 bits=16 runs=1 passed=1 failed=0
  compared_samples=176128 mismatches=0`, plus silence summaries
  `rate=48000 bits=24 runs=1 passed=1 failed=0 compared_samples=192512
  mismatches=0` and `rate=44100 bits=16 runs=1 passed=1 failed=0
  compared_samples=177152 mismatches=0`. Representative matching hash pairs
  were `0x4e3aa5b4af22bcb1`/`0xf5c67e41d99dc189` for the two 48 kHz/24-bit
  loopback runs, `0xb8a05d32ba828363` for 44.1 kHz/16-bit loopback,
  `0x551ae4077d09e9ab`/`0xa9c113a2ca2eb3e4` for the two 48 kHz/24-bit pattern
  runs, `0xe1198cc4ca20b3a7`/`0x65e279568d3cb0db` for the two 44.1 kHz/16-bit
  pattern runs, tone hashes `0x3da4df0d98155bc3` at 48 kHz/24-bit plus
  `0xdcb5ab5fe5ce16e3` at 44.1 kHz/16-bit, and zero-stream silence hashes
  `0x4c1b7a9a57ea0383` at 48 kHz/24-bit plus `0x620be961d8eec383` at
  44.1 kHz/16-bit. Final serial status stayed at `source=loop`,
  `fmt=48k24/48k24`, `stall=0`, `err=0`, `crc=0`, `over=0`, `under=0`, and
  `drop=0`.
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
  independently reopens dumped pattern, tone, sine, or silence WAV files and
  compares their PCM samples against the deterministic firmware source stream.
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
- `audio sine` is the preferred listening sanity source; it now emits the same
  sine sample on left and right, then advances once per stereo frame. `audio
  pattern` is intentionally noise-like. Latest post-flash `audio-capture`
  sine checks passed live and from disk at both formats: 48 kHz/24-bit and
  44.1 kHz/16-bit both reported zero mismatches over full one-second captures.
  Sine hashes can vary by run because capture starts at an aligned phase offset.
  If live monitoring still sounds noisy, run
  `make -C ports/same70-xplained audio-listen`; it captures, verifies, plays the
  verified sine WAV through `afplay`, keeps USB OUT silent during capture, and
  leaves the board on `audio sine`.
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
  `audio outdiag reset` and `audio outdiag` expose resettable endpoint-3 OUT
  diagnostics: packets, bytes, nonzero bytes, FNV-1a hash, last packet length,
  first 256 stream bytes, and first 256 bytes starting at the first nonzero OUT
  payload.
- Console commands: `?`, `help`, `status`, `clk`, `usb`, `usb init`,
  `usb attach`, `usb detach`, `audio loop`, `audio pattern`, `audio tone`,
  `audio sine`, `audio silence`, `audio outdiag`, `audio outdiag reset`. Audio
  source commands now print a terse `audio source=<source>` line instead of the
  full `usb` status block, so they are safer while a host audio stream is
  active.

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
audio loop=<level>/<peak> drop=<bytes> silence=<bytes> source=<loop|pattern|tone|sine|silence> fmt=<48k24|44k16>/<48k24|44k16>
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
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 1 --runs 1 --rate 48000 --bits 24 --verify tone --dump-input-wav /tmp/same70-tone-48k24.wav"
make -C ports/same70-xplained stream-probe STREAM_PROBE_ARGS="--seconds 1 --runs 1 --rate 44100 --bits 16 --verify loopback --dump-input-wav /tmp/same70-loop-cd-input.wav --dump-output-wav /tmp/same70-loop-cd-output.wav"
make -C ports/same70-xplained audio-verify
make -C ports/same70-xplained audio-generated-verify
make -C ports/same70-xplained audio-capture
make -C ports/same70-xplained audio-listen
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
tone, generated sine, and silence. The next practical SAME70 milestone is
mapping the original SDR Widget audio pipeline onto SAME70 peripherals if matching hardware is
available, or identifying the missing external codec/ADC path. Add real feature
storage in SAME70 flash only if persistence matters for the next test.
