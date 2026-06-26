# AT32UC3A3 USB DFU bootloader

`at32uc3a3-isp-1.0.3.hex` — Atmel's canonical UC3A3 USB DFU/ISP bootloader
(v1.0.3), occupies the bottom 8 KB of flash (`0x80000000–0x80001FFF`).
Sourced from the Atmel EVK1104 kit (github `gcielniak/evk1104`); identical to
the file Atmel ships in the UC3 DFU bootloader package.
sha256 prefix: `1b2cd1310676e06a2287`.

Build a flashable bootloader+app image and program it:

    python3 ../make_bootloader_image.py            # -> merged_boot_widget.hex
    python3 ../uc3jtag.py program merged_boot_widget.hex

The bootloader hands off to the app's `program_start` at `0x80002000`
(see `SOFTWARE_FRAMEWORK/ASM/trampoline.x`).
