# SAME70 Xplained Port

This directory is a first proof-of-life target for the Atmel SAME70 Xplained
board. It is intentionally separate from the AVR32 SDR Widget firmware because
the original project targets an AT32UC3A3256, while this board uses an ARM
Cortex-M7 SAME70-class MCU.

Current milestones:

- Green user LED heartbeat on PC8. The LED is active-low, and the foreground
  loop stays quiet after boot so USB polling is not paused by periodic
  9600-baud status writes.
- Status console through the EDBG virtual COM port on USART1 at 9600 8N1. On
  the tested macOS host the port enumerates as `/dev/cu.usbmodem1462302`.
- Main clock from the external 12 MHz crystal, with PLLA at 300 MHz CPU,
  150 MHz MCK, and UPLL enabled for USBHS.
- Cortex-M SysTick at 1 kHz.
- USBHS target-port device mode at high speed.
- UAC1 SDR Widget composite descriptor on endpoint 0. macOS lists
  `Yoyodyne SDR-Widget` as a USB audio device with 2 input channels, 2 output
  channels, with selectable 48 kHz/24-bit and 44.1 kHz/16-bit streaming modes.
- Mutable UAC1 mic/speaker mute and left/right volume class-control state,
  verified by the SAME70-only `uac-control` host probe. The state is not
  applied to sample bytes.
- USBHS isochronous endpoints 3 OUT, 4 feedback IN, and 5 audio IN are
  configured when the host sets configuration 1. The current handlers store
  output packets in a small byte ring, return feedback for the active rate, and
  send queued loopback bytes, generated PCM24/PCM16 pattern bytes, a
  deterministic square-wave PCM tone, a deterministic low-harmonic sine source,
  deterministic melody, or silence on input packets. Loopback is the default
  source.
- DG8SAQ/vendor feature control compatibility for the existing
  `widget-control` host tool.

## Build

```sh
make -C ports/same70-xplained
```

## Probe

```sh
make -C ports/same70-xplained probe
```

## Flash

This erases/programs the application flash on the connected SAME70 board.

```sh
make -C ports/same70-xplained flash
```

## Serial Status

After flashing, open the EDBG virtual COM port at 9600 8N1:

```sh
screen /dev/cu.usbmodem1462302 9600
```

Expected boot output:

```text
SAME70 Xplained SDR Widget bring-up
USART1 via EDBG VCOM: 9600 8N1 status output
USBHS target port auto-attach enabled
>
```

Console commands:

```text
?
status
usb
usb init
usb attach
usb detach
audio loop
audio pattern
audio tone
audio sine
audio melody
audio silence
audio hw
audio ctl
audio outdiag
audio outdiag reset
audio indiag
audio indiag reset
```

`status` reports the heartbeat counter, millisecond uptime, and USART status
register.

`usb init` enables the USBHS peripheral clock path, starts the UTMI PLL, forces
device mode, and configures endpoint 0 as a 64-byte control endpoint.
`usb attach` clears the USBHS device detach bit. The firmware auto-attaches at
boot. Enumeration requires a second USB cable on the SAME70 target USB
connector; the EDBG/debug USB connector only provides SWD and the serial
console.

Expected USB status after macOS enumeration:

```text
usb init=1 attached=1
events reset=1 setup=<n> tx=<n> rxout=<n> stall=0
ctrl address=<n> config=1 ep0_state=0 desc=<n> set_addr=1 set_cfg=1 set_int=<n> alt=0x00000000 peak_alt=0x0000000c last_int=<i>:<alt>
audio cfg=1 set_int=<n> cfgok=0x00000038 out=<n>/<bytes> fb=<n>/<bytes> in=<n>/<bytes> err=<n>
audio last_out=<bytes> max_out=<bytes> outnz=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
audio loop=<level>/<peak> drop=<bytes> silence=<bytes> source=<loop|pattern|tone|sine|melody|silence> fmt=<48k24|44k16>/<48k24|44k16>
```

`audio ctl` reports the remembered UAC1 feature-unit state:

```text
audio ctl mic_mute=<0|1> spk_mute=<0|1> mic_vol=<hex>/<hex> spk_vol=<hex>/<hex>
```

Mute and volume GET_CUR/SET_CUR values are stored for control-plane
compatibility and are not applied to the sample bytes.

Host checks:

```sh
./widget-control -a
./widget-control -d
./widget-control -g
./widget-control -m
./widget-control -l
./widget-control -r
make -C ports/same70-xplained widget-verify
make -C ports/same70-xplained widget-verify WIDGET_VERIFY_ARGS="--set-current-check"
make -C ports/same70-xplained widget-verify WIDGET_VERIFY_ARGS="--reset-check"
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
make -C ports/same70-xplained audio-cd-bitperfect
make -C ports/same70-xplained audio-cd-ready
make -C ports/same70-xplained audio-listen
make -C ports/same70-xplained audio-hw
make -C ports/same70-xplained external-codec-preflight
make -C ports/same70-xplained uac-verify
make -C ports/same70-xplained board-verify
make -C ports/same70-xplained board-ready
make -C ports/same70-xplained flash-ready
make -C ports/same70-xplained audio-wav-verify AUDIO_WAV_VERIFY_ARGS="/tmp/same70-loop-cd-input.wav --source loop --rate 44100 --bits 16 --expected-wav /tmp/same70-loop-cd-output.wav"
```

Typical `widget-control` feature output:

```text
10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms
```

The full read-only USB control gate builds `widget-control`, verifies device
enumeration, confirms that default, flash-backed NVRAM, and RAM feature values
all match, and checks the advertised feature listing:

```sh
make -C ports/same70-xplained widget-verify
```

Add `WIDGET_VERIFY_ARGS="--reset-check"` to also exercise `widget-control -r`
and wait for default readback after USB re-enumeration. This reset check does
not write flash-backed NVRAM.
Add `WIDGET_VERIFY_ARGS="--set-current-check"` to send the current feature
table through `widget-control -s`; unchanged values are accepted by the device
without consuming a flash record. The verifier checks serial `usb` status
before/after and fails if feature-store `writes` changes. The host tool checks
that every `-s` response byte equals the requested value.
Shared serial host helpers flush stale console input immediately before sending
each command, which avoids old reset/status fragments after flashing or resets.

The connected-board smoke gate combines the guarded USB control check, the UAC
feature-unit control check, the quiet generated-source audio gate, and the
hardware-audio boundary check:

```sh
make -C ports/same70-xplained board-verify
```

It builds the SAME70 firmware, CoreAudio probe, and libusb UAC probe, runs
`widget-verify` with `--set-current-check`, runs `uac-verify`, runs
`audio-generated-verify`, then runs `audio-hw`. The latest run passed with
feature-store `writes=0` before/after the no-change set, UAC mic/speaker mute
and left/right volume round trips restored to zero, and zero mismatches for
pattern, tone, sine, and melody. Silence reported host `input_nonzero=0`,
device OUT `nonzero=0`, and device IN `nonzero=0` at both formats; `audio-hw`
reported `needs_external_codec_board`. Because the generated-source gate ends
on `audio silence`, run `audio-listen` afterward when you want the board back
on melody.

Use `board-ready` when you want both the connected-board smoke gate and the
verified listening state in one command:

```sh
make -C ports/same70-xplained board-ready
```

It runs `board-verify`, then runs `audio-listen` and leaves the board on
`audio melody`.

Use `flash-ready` when you want to program the connected SAME70 first, then
prove and restore the ready state:

```sh
make -C ports/same70-xplained flash-ready
```

It runs `flash`, waits briefly for USB re-enumeration, then runs `board-ready`.
The latest run programmed and verified flash with OpenOCD, passed the widget
and UAC control gates plus the generated-audio gate, exact-verified
`/tmp/same70-melody-listen.wav` with `compared_samples=353280 mismatches=0`
and matching hash `0xab4b79884dabf283`, played it with `afplay`, and ended on
`audio melody`.

The `stream-probe` target builds and runs a macOS CoreAudio HAL probe that
opens the `Yoyodyne SDR-Widget` device directly. It requests hog mode, unmuted
output, and unity output volume before starting streams, and prints
`hog_mode=1` when exclusive ownership was granted. It should leave the serial
`usb` counters with nonzero audio packet counts and a peak alternate-setting
mask showing playback and capture streams were opened. The probe selects the
requested nominal sample rate, writes exact integer test samples through
CoreAudio Float32 buffers, captures the returned input samples, quantizes them
back to the selected bit depth, aligns the loopback latency, and reports whether
the aligned sample stream is bit-perfect. In exact loopback, pattern, tone,
sine, melody, and silence modes, the verify line also includes 64-bit
`expected_hash` and `actual_hash` fingerprints over the quantized samples
actually compared. Use
`--verify pattern` when the firmware source is `audio pattern`; it aligns the
captured samples against the firmware's deterministic LCG pattern and compares
them sample-for-sample. Use `--verify tone` when the firmware source is
`audio tone`; it aligns captured samples against the deterministic square-wave
PCM source. Use `--verify sine` when the firmware source is `audio sine`; it
aligns captured samples against a deterministic low-harmonic sine lookup source
for less harsh listening checks. Use `--verify melody` when the firmware source
is `audio melody`; it aligns captured samples against the generated note phrase.
Use `--verify silence` when the firmware source is `audio silence`; it requires
all quantized input samples to be zero.
Use `--verify input` for a looser nonzero input activity check. The serial `usb`
counters remain the source of truth for USBHS endpoint state. Add
`--dump-input-wav FILE --runs 1` to write
the quantized captured input stream as a PCM WAV file for inspection or
listening. Add `--dump-output-wav FILE --runs 1` to write the quantized host
output stream too, which lets loopback be rechecked later from a WAV pair.
Multi-run WAV dumps are rejected so output files are unambiguous.

The `audio-verify` target wraps the same probe with serial source switching. It
selects `audio loop`, verifies loopback at both advertised formats, selects
`audio pattern`, verifies the generated input pattern at both formats, selects
`audio tone`, verifies the generated tone at both formats, selects
`audio sine`, verifies the generated sine at both formats, selects
`audio melody`, verifies the generated melody at both formats, selects
`audio silence`, verifies zero input at both formats, switches back to
`audio loop`, runs a final 48 kHz/24-bit loopback check, and confirms the serial
`usb` status reports `source=loop` and `fmt=48k24/48k24`. It also captures a
starting `usb` status and audits the final counters. `stall`, `crc`, `over`,
and `drop` increases are hard failures; `under` increases from CoreAudio stream
restarts are allowed only when `err` increases by no more than the same amount.
Generated-source runs use `--silent-output` so only loopback verification drives
the host USB OUT test pattern. Use `AUDIO_VERIFY_ARGS="--seconds N
--loopback-runs N --pattern-runs N --tone-runs N --sine-runs N --melody-runs N
--silence-runs N --skip-final-loopback --skip-serial-counter-check
--strict-serial-counter-check
--serial /dev/cu.usbmodem..."` to tune the run length, per-source run counts,
final loopback reset, strict serial counter audit, or serial port.

The `audio-generated-verify` target is the quick quiet gate for generated input
sources only. It runs `audio-verify` with `--seconds 1 --skip-loopback
--skip-final-loopback --skip-serial-counter-check`, so pattern, tone, sine,
melody, and silence are verified at both formats with silent USB OUT and no
final loopback burst. The dedicated `audio silence` source is checked with a
stream-start probe plus device-side IN diagnostics because macOS can suppress
client callbacks for sustained all-zero generated input. Because the sequence
ends with the silence source, run `audio-listen` or set `audio melody` afterward
when you want an audible monitor signal again.

The `audio-capture` target is the repeatable WAV-dump wrapper. It selects a
serial audio source, runs one exact probe pass with `--dump-input-wav`, verifies
WAV artifacts again from disk with `audio-wav-verify.py`, restores `audio loop`
unless `--leave-source` is passed, checks the final serial `usb` status against
the starting error counters, and prints `audio-capture: pass output=<path>` on
success. As with `audio-verify`, paired `err`/`under` increases from CoreAudio
stream restarts are accepted; add `--strict-serial-counter-check` to fail on
those too. Non-loop sources pass `--silent-output` to the CoreAudio probe so
host USB OUT does not add a noisy monitor signal while generated USB IN is
being verified. Defaults capture the 48 kHz/24-bit generated tone to
`/tmp/same70-tone-48k24.wav`. Set
`AUDIO_CAPTURE_ARGS` to choose source, format, duration, or output path. Add
`--skip-serial-counter-check` for listening or monitoring sessions where slow
9600-baud diagnostic status output could disturb an already-active stream:

```sh
make -C ports/same70-xplained audio-capture AUDIO_CAPTURE_ARGS="--source tone --rate 48000 --bits 24 --seconds 1 --output /tmp/file.wav"
```

For a less harsh listening test than `audio pattern` or the square-wave
`audio tone`, use the deterministic mono-in-stereo sine or melody sources:

```sh
make -C ports/same70-xplained audio-capture AUDIO_CAPTURE_ARGS="--source sine --rate 44100 --bits 16 --seconds 1 --output /tmp/same70-sine-cd-capture.wav --skip-serial-counter-check"
make -C ports/same70-xplained audio-capture AUDIO_CAPTURE_ARGS="--source melody --rate 44100 --bits 16 --seconds 4 --output /tmp/same70-melody-cd-capture.wav --skip-serial-counter-check --leave-source"
```

To check the listening path without a live monitor app, use `audio-listen`. It
captures the generated 44.1 kHz/16-bit melody input, verifies the dumped WAV
from disk against the deterministic note phrase, plays it through the Mac
default output with `afplay`, keeps the probe's USB OUT stream silent during
capture, and leaves the board source set to `audio melody`:

```sh
make -C ports/same70-xplained audio-listen
```

Latest checked `audio-listen` run captured four seconds to
`/tmp/same70-melody-listen.wav`, exact-verified the live stream and WAV with
`compared_samples=353280 mismatches=0` and matching hash
`0xab4b79884dabf283`, played it with `afplay`, and left the board on
`audio melody`.

Latest exact melody verification used
`audio-verify --skip-loopback --skip-pattern --skip-tone --skip-sine
--skip-silence --skip-final-loopback --skip-serial-counter-check --melody-runs
1` and passed at both formats with matching live hashes:
`0x76e0eff7c151b947` at 48 kHz/24-bit and `0x3186e5c43407cda7` at
44.1 kHz/16-bit. A one-second `audio-capture --source melody --rate 44100
--bits 16` run also passed live exact verification and disk WAV verification on
`/tmp/same70-melody-exact.wav` with hash `0x9d6ac91ff2387a43`.

For loopback, `audio-capture --source loop` also dumps the quantized host output
WAV and verifies the captured input against it after latency alignment:

```sh
make -C ports/same70-xplained audio-capture AUDIO_CAPTURE_ARGS="--source loop --rate 44100 --bits 16 --seconds 1 --output /tmp/same70-loop-cd-input.wav --output-wav /tmp/same70-loop-cd-output.wav"
```

Use `audio-cd-bitperfect` for the named 44.1 kHz/16-bit loopback proof. It
captures `/tmp/same70-loop-cd-input.wav` from the device, dumps the host output
reference to `/tmp/same70-loop-cd-output.wav`, and verifies the WAV pair live
and again from disk:

```sh
make -C ports/same70-xplained audio-cd-bitperfect
```

Use `audio-cd-ready` to run that CD-rate proof and then restore the verified
44.1 kHz/16-bit melody listening state:

```sh
make -C ports/same70-xplained audio-cd-ready
```

Use `audio-wav-verify` to recheck dumped generated-source WAVs or loopback WAV
pairs later without the board:

```sh
make -C ports/same70-xplained audio-wav-verify AUDIO_WAV_VERIFY_ARGS="/tmp/same70-loop-cd-input.wav --source loop --rate 44100 --bits 16 --expected-wav /tmp/same70-loop-cd-output.wav"
```

```text
nominal_sample_rate=<44100|48000>
run=<n> stream_started=1
run=<n> started=1 seconds=<n> rate=<44100|48000> bits=<16|24> callbacks=<n> input_bytes=<n> output_bytes=<n> input_nonzero=<n> output_nonzero=<n> input_checksum=<n> output_checksum=<n>
dump_input_wav=<path> samples=<n> channels=<n> rate=<44100|48000> bits=<16|24> bytes=<n>
dump_output_wav=<path> samples=<n> channels=<n> rate=<44100|48000> bits=<16|24> bytes=<n>
run=<n> verify=<pass|fail> mode=<loopback|input|pattern|tone|sine|melody|silence|start> aligned=<0|1> input_offset_samples=<n> expected_offset_samples=<n> compared_samples=<n> mismatches=<n> expected_hash=<hex> actual_hash=<hex> first_mismatch=<n> expected=<n> actual=<n> input_samples=<n> output_samples=<n> input_overflow=<n> output_overflow=<n>
summary mode=<loopback|input|pattern|tone|sine|melody|silence|start> rate=<44100|48000> bits=<16|24> runs=<n> passed=<n> failed=<n> compared_samples=<n> mismatches=<n>
ctrl address=<n> config=1 ep0_state=0 desc=<n> set_addr=1 set_cfg=1 set_int=<n> alt=0x00000000 peak_alt=0x0000000c last_int=<i>:<alt>
audio cfg=1 set_int=<n> cfgok=0x00000038 out=<n>/<bytes> fb=<n>/<bytes> in=<n>/<bytes> err=<n>
audio last_out=<bytes> max_out=<bytes> outnz=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
audio loop=<level>/<peak> drop=<bytes> silence=<bytes> source=<loop|pattern|tone|sine|melody|silence> fmt=<48k24|44k16>/<48k24|44k16>
```

Latest measured 48 kHz/24-bit check on the connected board: `--seconds 2
--runs 1 --rate 48000 --bits 24` reported `summary mode=loopback rate=48000
bits=24 runs=1 passed=1 failed=0 compared_samples=189736 mismatches=0`, with
`input_offset_samples=2776`.

Latest measured CD-rate check on the connected board: `--seconds 2 --runs 5
--rate 44100 --bits 16` reported `summary mode=loopback rate=44100 bits=16
runs=5 passed=5 failed=0 compared_samples=866444 mismatches=0`, with
`input_offset_samples=2804` on the first three runs and `2892` on the last two.
Final serial `usb` after switching back to 48 kHz/24-bit reported
`out=14108/2938948 fb=2/8 in=14284/2975792 err=0`, `under=0`, `drop=0`,
`stall=0`, `audio loop=0/1152`, and `fmt=48k24/48k24`.

Generated-pattern exact source checks: `audio pattern` followed by `--seconds 2
--runs 2 --rate 48000 --bits 24 --verify pattern` reported `summary
mode=pattern rate=48000 bits=24 runs=2 passed=2 failed=0
compared_samples=384000 mismatches=0`, with expected-pattern offsets `3040` and
`2864`. The same check at `--rate 44100 --bits 16` reported `summary
mode=pattern rate=44100 bits=16 runs=2 passed=2 failed=0
compared_samples=352256 mismatches=0`, with expected-pattern offsets `2688` and
`2504`. The board was switched back to `audio loop` afterward, and a final
1-second 48 kHz/24-bit loopback check passed with zero mismatches. Final serial
status showed `source=loop`, `fmt=48k24/48k24`, `err=0`, `under=0`, `drop=0`,
and `stall=0`.

Earlier full automated gate with strict serial-diagnostic delta checks and
hash-audited sample comparisons:
`make -C ports/same70-xplained audio-verify` reported `audio-verify: pass`. It
measured 48 kHz/24-bit loopback `runs=2 passed=2 failed=0
compared_samples=378448 mismatches=0`, 44.1 kHz/16-bit
loopback `runs=2 passed=2 failed=0 compared_samples=346648 mismatches=0`,
48 kHz/24-bit pattern `runs=2 passed=2 failed=0 compared_samples=384000
mismatches=0`, and 44.1 kHz/16-bit pattern `runs=2 passed=2 failed=0
compared_samples=353280 mismatches=0`. It also measured 48 kHz/24-bit tone
`runs=1 passed=1 failed=0 compared_samples=192512 mismatches=0`,
44.1 kHz/16-bit tone `runs=1 passed=1 failed=0 compared_samples=176128
mismatches=0`, 48 kHz/24-bit silence `runs=1 passed=1 failed=0
compared_samples=192512 mismatches=0`, and 44.1 kHz/16-bit silence
`runs=1 passed=1 failed=0 compared_samples=177152 mismatches=0`, with silence
`input_nonzero=0`. Matching
`expected_hash`/`actual_hash` pairs were `0x4e3aa5b4af22bcb1` and
`0xf5c67e41d99dc189` for the two 48 kHz/24-bit loopback runs,
`0xb8a05d32ba828363` for 44.1 kHz/16-bit loopback, `0x551ae4077d09e9ab` and
`0xa9c113a2ca2eb3e4` for the two 48 kHz/24-bit pattern runs,
`0xe1198cc4ca20b3a7` and `0x65e279568d3cb0db` for the two 44.1 kHz/16-bit
pattern runs, tone hashes `0x3da4df0d98155bc3` at 48 kHz/24-bit and
`0xdcb5ab5fe5ce16e3` at 44.1 kHz/16-bit, and zero-stream silence hashes
`0x4c1b7a9a57ea0383` at 48 kHz/24-bit plus `0x620be961d8eec383` at
44.1 kHz/16-bit. Final serial status reported `source=loop`, `fmt=48k24/48k24`,
`err=0`, `crc=0`, `over=0`, `under=0`, `drop=0`, and `stall=0`, with no
increase from the starting baseline.

Latest full gate status:
`make -C ports/same70-xplained audio-verify
AUDIO_VERIFY_ARGS="--seconds 1"` passes with `hog_mode=1` and zero mismatches
for exact loopback, pattern, tone, sine, melody, and silence checks at both
advertised formats. The final gate restores `audio loop`, verifies
48 kHz/24-bit loopback again, and the final serial status reports `source=loop`,
`fmt=48k24/48k24`, `stall=0`, `crc=0`, `over=0`, `drop=0`, and paired restart
underflow counters `err=118 under=118`, which the gate accepts. Loopback
summaries were `rate=48000 bits=24 runs=2 passed=2 failed=0
compared_samples=186960 mismatches=0` and `rate=44100 bits=16 runs=2 passed=2
failed=0 compared_samples=170520 mismatches=0`; generated-source summaries all
reported zero mismatches. Matching melody hashes were `0x9ef75feb9a6e2b27` at
48 kHz/24-bit and `0x90c29364843c4d2b` at 44.1 kHz/16-bit. The silence source
used `--verify start`; it reported host `input_nonzero=0`, device OUT
`nonzero=0`, and device IN `nonzero=0` at both formats, with IN byte counts
`885776` at 48 kHz/24-bit and `794124` at 44.1 kHz/16-bit.

Current generated-source gate status: `make -C ports/same70-xplained
audio-generated-verify` passes. Pattern, tone, sine, and melody are exact
host-captured checks with zero mismatches at both advertised formats. The
dedicated silence source uses `--verify start` plus `audio outdiag` and
`audio indiag`; the latest run reported host all-zero input, OUT `nonzero=0`,
and IN `nonzero=0` at both formats, including IN diagnostic byte counts
`901448` at 48 kHz/24-bit and `804144` at 44.1 kHz/16-bit.

Use `audio outdiag reset` before a host probe, then `audio outdiag` afterward to
dump raw endpoint-3 OUT diagnostics since reset: packet count, byte count,
nonzero byte count, FNV-1a hash, last packet length, the first 256 stream bytes,
and a second 256-byte window starting at the first nonzero OUT byte.
Use `audio indiag reset` and `audio indiag` the same way for endpoint-5 IN
diagnostics recorded by the generated silence source.
Use `audio hw`, or `make -C ports/same70-xplained audio-hw`, to print the
current hardware-audio boundary. The expected output reports
`external_codec=0`, `i2sc=not_configured`, and
`needs_external_codec_board`; the port is still using only USB loopback or
generated audio sources. The future stock-board wiring plan uses header-exposed
digital-audio/SSC-style pins for the original AK5394A/CS4344-style codec path:
TD on PD26/J502.1, RD/RF/RK on PA10/J504.2, PD24/J504.1, PA22/J504.3, TK/TF on
PB1/PB0 via J505 or J507, and PCK0 on PB13 via J504.5 or J507.19. See
`external-codec-pin-map.md` before wiring or enabling codec firmware.
Use `make -C ports/same70-xplained external-codec-preflight` before wiring or
codec-firmware work; it checks the pin-map guardrails and then runs `audio-hw`
to prove the stock board still reports no external codec. The latest run passed
and reported `external codec remains disabled`.

Latest CD bit-perfect check used `make -C ports/same70-xplained
audio-cd-ready`. The `audio-cd-bitperfect` phase captured input
`/tmp/same70-loop-cd-input.wav` and host output
`/tmp/same70-loop-cd-output.wav`; the live probe and independent
`audio-wav-verify.py` disk check both reported `input_offset_samples=2804`,
`compared_samples=174348`, `mismatches=0`, and matching hash
`0x21d4ae0f385bce93`. The dump lines reported
`samples=177152 channels=2 rate=44100 bits=16 bytes=354304` for both WAVs;
`file` identified both as 16-bit stereo PCM at 44.1 kHz with 354348-byte
RIFF/WAVE containers. The wrapper then exact-verified
`/tmp/same70-melody-listen.wav` with hash `0xab4b79884dabf283`, played it with
`afplay`, and left the board on `audio melody`.

Latest sine listening-source checks after flashing passed at both advertised
formats. The sine source emits matching left/right samples and advances once per
stereo frame. `audio-capture --source sine --rate 48000 --bits 24` and
`audio-capture --source sine --rate 44100 --bits 16` both reported zero
mismatches over full one-second captures. The exact sine hash can vary by run
because capture starts at an aligned phase offset.

## Porting Notes

The original SDR Widget firmware depends on AVR32-specific peripherals and
registers, including USBB, PDCA, TWIM, SSC, FLASHC, and the AVR32 FreeRTOS port.
Those need SAME70 equivalents before the full application can run here.
The original audio path is the AK5394A/CS4344 external codec path driven by
AVR32 SSC plus PDCA double buffers. On this SAME70 Xplained port, the codec
side is deliberately not claimed yet: no external codec is configured, the
SAME70 digital-audio peripheral path is not configured, and the missing signals are
MCLK, BCLK, LRCK, ADC serial data, DAC serial data, codec reset, and codec
control. The first stock-board wiring map intentionally uses exposed headers
instead of non-header SDRAM/I2SC0 package pins; it is documented in
`external-codec-pin-map.md`, with XDMAC double buffering still to be designed.
The current board can prove USB timing and bit-perfect sample movement, but not
analog SDR input/output until that external codec path exists.
`same70_audio_hw.c` is the status seam to extend when codec hardware is added.

The USBHS bring-up code is intentionally tiny and separate from the original
AVR32 USBB driver. It currently proves clocks, device mode, endpoint 0
enumeration, DG8SAQ feature requests, and basic UAC1 class-control requests.
Audio streaming endpoints are now hardware-configured and loopback-serviced:
endpoint 3 OUT stores received packets in an 8192-byte ring, endpoint 4
feedback IN reports the active high-speed feedback value, and endpoint 5 audio
IN sends queued loopback bytes, generated PCM24/PCM16 pattern bytes, a generated
square-wave tone, generated sine, generated melody, or silence depending on the
serial-selected source. The macOS
HAL stream probe writes
deterministic integer sample values through CoreAudio Float32 buffers, quantizes
returned input back to the selected bit depth, aligns stream latency, and fails
if any aligned sample differs in loopback mode. The firmware now reads the USBHS
isochronous BYCT field for actual OUT byte accounting and reports
short/CRC/overflow/underflow
diagnostics. Error flags are counted only while the corresponding alternate
setting is active, and stale flags are cleared while streams are inactive.
Short OUT packets are expected because 48 kHz/24-bit uses 288-byte packets below
the 294-byte endpoint maximum, and 44.1 kHz/16-bit uses a 176/180-byte packet
cadence. Feedback and audio IN refills now use USBHS RWALL/NBUSYBK state so the
firmware can keep up to two banks queued; the serial `usb` command reports those
depths as `fb_busy=<last>/<max>` and `in_busy=<last>/<max>`. The loopback input
prebuffers whole packets after stream resets so startup silence is skipped
cleanly by the bit-perfect verifier.
This is not yet the real SDR Widget audio pipeline.

Feature "NVRAM" is backed by an append-only flash page reserved at
`0x004ffe00`. Each persisted record is 32 bytes, giving 16 slots in the
512-byte page; unchanged `widget-control -s` values are skipped so a normal
full-table write consumes a slot only for features that actually change. The
latest persistence check set `log=1sec`, verified `widget-control -g` after
reset, restored `log=500ms`, reset again, and verified default readback. Serial
`usb` reports the storage state as `feature store loaded=<n> valid=<n>
writes=<n> err=<n> fsr=<hex>`.

The EDBG virtual COM port uses the target's USART1 pins: PA21/RXD1 and
PB4/TXD1. PB4 resets as the JTAG TDI system pin, so the firmware must set
`MATRIX_CCFG_SYSIO.SYSIO4` before routing PB4 to peripheral D for TXD1.
