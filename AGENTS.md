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
  channels, 2 output channels, and 48 kHz current sample rate.
- Endpoint 0 handles standard enumeration, interface alternate-setting
  requests, SDR Widget vendor feature requests, and basic UAC1 mute/volume and
  sample-rate control requests.
- USBHS isochronous endpoints 3 OUT, 4 feedback IN, and 5 audio IN are
  configured when the host sets configuration 1. Verified serial status shows
  `audio cfg=1 cfgok=0x00000038 ...`.
- USB audio loopback handlers are present: endpoint 3 stores output packets in
  a small byte ring, endpoint 4 returns fixed 48 kHz high-speed feedback, and
  endpoint 5 sends queued loopback bytes with silence fill only when the ring
  runs dry.
- The macOS CoreAudio HAL stream probe target has verified USB-level active
  streaming against the connected board. Latest host-side stream-probe output
  was `started=1 callbacks=188 input_bytes=770048 output_bytes=770048` with
  `input_nonzero=329571 output_nonzero=767040` on each of two consecutive runs
  with the nonzero output-pattern probe. Serial counters after those runs showed
  `peak_alt=0x0000000c`, `audio ... out=4036/1162368 fb=2/8
  in=4088/1177344 err=0`, `under=0`, `stall=0`, and
  `audio loop=0/288 drop=0 silence=14976`. Current `alt=0x00000000` after
  CoreAudio closed the streams.
- `widget-control -a`, `-d`, `-g`, `-m`, `-l`, and `-r` have been verified
  against the connected SAME70 board. `-r` now performs a real software reset;
  the device re-enumerates afterward and `-d` still returns defaults.
- After macOS audio enumeration and `widget-control -d`, the serial `usb`
  command has been verified with `stall=0`.
- Feature "NVRAM" on the SAME70 port is currently a volatile RAM
  compatibility table, not persistent SAME70 flash storage.
- The HAL stream probe opens the widget, drives SET_INTERFACE/endpoint traffic,
  writes a nonzero output pattern, and verifies nonzero input bytes through the
  loopback path. Serial `usb` counters remain the source of truth for hardware
  endpoint state.
- Audio diagnostics now include byte totals from USBHS BYCT, last/max OUT
  packet sizes, and short/CRC/overflow/underflow counters. Short OUT packets
  are expected for the observed 288-byte packets under the 294-byte endpoint
  maximum. IN underflow counting now ignores inactive alternate settings; the
  last verification saw zero active IN underflows after two consecutive
  stream-probe runs. Endpoint 4 feedback IN and endpoint 5 audio IN are refilled
  from USBHS RWALL/NBUSYBK state, and diagnostics report `fb_busy=<last>/<max>`
  plus `in_busy=<last>/<max>`. Loopback diagnostics report current/peak ring
  fill, dropped OUT bytes, and inserted silence bytes.
- Console commands: `?`, `help`, `status`, `clk`, `usb`, `usb init`, `usb attach`,
  `usb detach`.

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
audio last_out=<bytes> max_out=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
audio loop=<level>/<peak> drop=<bytes> silence=<bytes>
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
```

Typical feature output:

```text
10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms
```

## Porting Direction

The original AVR32 firmware depends on AVR32-specific ASF components including
USBB, PDCA, TWIM, SSC, FLASHC, and the AVR32 FreeRTOS port. The next practical
SAME70 milestone is moving from USB loopback toward useful audio data:
map the original SDR Widget audio pipeline onto SAME70 peripherals if matching
hardware is available, or add board-level test signal generation if no codec
hardware is attached. Add real feature storage in SAME70 flash only if
persistence matters for the next test.
