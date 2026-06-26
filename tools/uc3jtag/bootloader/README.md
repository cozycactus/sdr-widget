# AT32UC3A3 USB DFU bootloader

This directory is where a local copy of Atmel's canonical UC3A3 USB DFU/ISP
bootloader can be placed for recovery work:

    at32uc3a3-isp-1.0.3.hex

The bootloader occupies the bottom 8 KB of flash
(`0x80000000-0x80001FFF`). It is not stored in this repository; obtain it from
Microchip/Atmel's UC3 DFU bootloader package or a licensed Atmel EVK1104 kit
source, then verify:

    shasum -a 256 at32uc3a3-isp-1.0.3.hex

Expected SHA-256:

    1b2cd1310676e06a2287bbeac08b06bb454b569bc72e0986e7d0561b6dbb2ed1

Build a flashable bootloader+app image and program it:

    python3 ../make_bootloader_image.py --bootloader at32uc3a3-isp-1.0.3.hex
    python3 ../uc3jtag.py program --erase-all merged_boot_widget.hex

The bootloader hands off to the app's `program_start` at `0x80002000`
(see `SOFTWARE_FRAMEWORK/ASM/trampoline.x`).
