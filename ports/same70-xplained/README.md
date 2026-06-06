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
ctrl address=<n> config=1 ep0_state=0 desc=<n> set_addr=1 set_cfg=1
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
```

Typical `widget-control` feature output:

```text
10 37 widget uac1_dg8saq normal normal ak5394a cs4344 hd44780 500ms
```

## Porting Notes

The original SDR Widget firmware depends on AVR32-specific peripherals and
registers, including USBB, PDCA, TWIM, SSC, FLASHC, and the AVR32 FreeRTOS port.
Those need SAME70 equivalents before the full application can run here.

The USBHS bring-up code is intentionally tiny and separate from the original
AVR32 USBB driver. It currently proves clocks, device mode, endpoint 0
enumeration, DG8SAQ feature requests, and basic UAC1 class-control requests.
Audio streaming endpoints are descriptor-visible only; endpoint 3 OUT,
endpoint 4 feedback IN, and endpoint 5 audio IN are not yet configured for real
isochronous traffic.

Feature "NVRAM" is currently an in-RAM compatibility table. It is not persisted
to SAME70 flash.

The EDBG virtual COM port uses the target's USART1 pins: PA21/RXD1 and
PB4/TXD1. PB4 resets as the JTAG TDI system pin, so the firmware must set
`MATRIX_CCFG_SYSIO.SYSIO4` before routing PB4 to peripheral D for TXD1.
