# SAME70 Xplained Port

This directory is a first proof-of-life target for the Atmel SAME70 Xplained
board. It is intentionally separate from the AVR32 SDR Widget firmware because
the original project targets an AT32UC3A3256, while this board uses an ARM
Cortex-M7 SAME70-class MCU.

Current milestones:

- Green user LED heartbeat on PC8. The LED is active-low.
- Status console through the EDBG virtual COM port on USART1 at 9600 8N1. On
  the tested macOS host the port enumerates as `/dev/cu.usbmodem1462302`.
- Main clock from the external 12 MHz crystal, with PLLA at 300 MHz CPU,
  150 MHz MCK, and UPLL enabled for USBHS.
- Cortex-M SysTick at 1 kHz.
- USBHS target-port device mode at high speed.
- UAC1 SDR Widget composite descriptor on endpoint 0. macOS lists
  `Yoyodyne SDR-Widget` as a USB audio device with 2 input channels, 2 output
  channels, and 48 kHz current sample rate.
- USBHS isochronous endpoints 3 OUT, 4 feedback IN, and 5 audio IN are
  configured when the host sets configuration 1. The current handlers drain
  output packets, return fixed 48 kHz feedback, and send silence for input
  packets when macOS opens the streaming alternate settings.
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

Expected output:

```text
tick 3
tick 4
```

Console commands:

```text
?
status
usb
usb init
usb attach
usb detach
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
audio last_out=<bytes> max_out=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
```

Host checks:

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

Typical `widget-control` feature output:

```text
10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms
```

The `stream-probe` target builds and runs a macOS CoreAudio HAL probe that
opens the `Yoyodyne SDR-Widget` device directly. It should leave the serial
`usb` counters with nonzero audio packet counts and a peak alternate-setting
mask showing playback and capture streams were opened. The host-side IOProc
callback counters may be zero on this placeholder firmware; the serial `usb`
counters are the source of truth for USB-level stream activity.

```text
started=1 callbacks=<n> input_bytes=<n> output_bytes=<n>
ctrl address=<n> config=1 ep0_state=0 desc=<n> set_addr=1 set_cfg=1 set_int=<n> alt=0x00000000 peak_alt=0x0000000c last_int=<i>:<alt>
audio cfg=1 set_int=<n> cfgok=0x00000038 out=<n>/<bytes> fb=<n>/<bytes> in=<n>/<bytes> err=<n>
audio last_out=<bytes> max_out=<bytes> short=<n> crc=0 over=0 under=<n> fb_busy=<n>/<n> in_busy=<n>/<n>
```

Latest measured two-run check on the connected board: host output was
`started=1 callbacks=0 input_bytes=0 output_bytes=0`, while serial `usb`
reported `out=402/115776 fb=2/8 in=5361/1543968 err=6`,
`under=6`, `fb_busy=2/2`, `in_busy=2/2`, `peak_alt=0x0000000c`, and
`stall=0`.

## Porting Notes

The original SDR Widget firmware depends on AVR32-specific peripherals and
registers, including USBB, PDCA, TWIM, SSC, FLASHC, and the AVR32 FreeRTOS port.
Those need SAME70 equivalents before the full application can run here.

The USBHS bring-up code is intentionally tiny and separate from the original
AVR32 USBB driver. It currently proves clocks, device mode, endpoint 0
enumeration, DG8SAQ feature requests, and basic UAC1 class-control requests.
Audio streaming endpoints are now hardware-configured and placeholder-serviced:
endpoint 3 OUT drains received packets, endpoint 4 feedback IN reports the
fixed 48 kHz high-speed feedback value, and endpoint 5 audio IN sends silence.
The macOS HAL stream probe has verified USB-level SET_INTERFACE traffic and
nonzero OUT/IN packet counters. The firmware now reads the USBHS isochronous
BYCT field for actual OUT byte accounting and reports short/CRC/overflow/
underflow diagnostics. Error flags are counted only while the corresponding
alternate setting is active, and stale flags are cleared while streams are
inactive. Short OUT packets are expected because the observed 288-byte audio
packets are below the 294-byte endpoint maximum. Feedback and audio IN refills
now use USBHS RWALL/NBUSYBK state so the firmware can keep up to two banks
queued; the serial `usb` command reports those depths as `fb_busy=<last>/<max>`
and `in_busy=<last>/<max>`. The latest placeholder stream check still sees six
active IN underflows after two consecutive probe runs. This is not yet the real
SDR Widget audio pipeline.

Feature "NVRAM" is currently an in-RAM compatibility table. It is not persisted
to SAME70 flash.

The EDBG virtual COM port uses the target's USART1 pins: PA21/RXD1 and
PB4/TXD1. PB4 resets as the JTAG TDI system pin, so the firmware must set
`MATRIX_CCFG_SYSIO.SYSIO4` before routing PB4 to peripheral D for TXD1.
