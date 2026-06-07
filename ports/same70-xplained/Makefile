TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size
HOST_CC ?= clang
PYTHON ?= python3

BUILD_DIR := build
TARGET := $(BUILD_DIR)/same70-blinky
STREAM_PROBE := $(BUILD_DIR)/coreaudio-stream-probe
STREAM_PROBE_ARGS ?=
AUDIO_VERIFY_ARGS ?=
AUDIO_GENERATED_VERIFY_ARGS ?= --seconds 1 --skip-loopback --skip-final-loopback --skip-serial-counter-check
AUDIO_CAPTURE_ARGS ?=
AUDIO_WAV_VERIFY_ARGS ?=
SERIAL_COMMAND_ARGS ?=
WIDGET_VERIFY_ARGS ?=
WIDGET_CONTROL ?= ../../widget-control
BOARD_WIDGET_VERIFY_ARGS ?= --set-current-check
BOARD_AUDIO_VERIFY_ARGS ?= $(AUDIO_GENERATED_VERIFY_ARGS)
BOARD_READY_AUDIO_LISTEN_ARGS ?= $(AUDIO_LISTEN_ARGS)
FLASH_READY_DELAY ?= 2
AUDIO_CD_INPUT_WAV ?= /tmp/same70-loop-cd-input.wav
AUDIO_CD_OUTPUT_WAV ?= /tmp/same70-loop-cd-output.wav
AUDIO_CD_BITPERFECT_ARGS ?= --source loop --rate 44100 --bits 16 --seconds 2 --output $(AUDIO_CD_INPUT_WAV) --output-wav $(AUDIO_CD_OUTPUT_WAV)
AUDIO_CD_READY_AUDIO_LISTEN_ARGS ?= $(AUDIO_LISTEN_ARGS)
AUDIO_LISTEN_OUTPUT ?= /tmp/same70-melody-listen.wav
AUDIO_LISTEN_ARGS ?= --source melody --rate 44100 --bits 16 --seconds 4 --output $(AUDIO_LISTEN_OUTPUT) --skip-serial-counter-check --leave-source
AUDIO_PLAYER ?= afplay

CFLAGS := \
	-mcpu=cortex-m7 \
	-mthumb \
	-mfpu=fpv5-sp-d16 \
	-mfloat-abi=hard \
	-Os \
	-g3 \
	-ffunction-sections \
	-fdata-sections \
	-fno-builtin \
	-Wall \
	-Wextra \
	-std=c11

LDFLAGS := \
	-T linker.ld \
	-nostartfiles \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,-Map,$(TARGET).map

SRCS := startup_same70.c main.c same70_clock.c same70_usb.c
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean flash flash-ready probe stream-probe audio-verify audio-generated-verify audio-capture audio-cd-bitperfect audio-cd-ready audio-listen audio-hw audio-wav-verify widget-verify board-verify board-ready

all: $(TARGET).elf $(TARGET).bin

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(STREAM_PROBE): coreaudio-stream-probe.c | $(BUILD_DIR)
	$(HOST_CC) -Wall -Wextra -std=c11 $< -o $@ -framework CoreAudio -framework CoreFoundation

stream-probe: $(STREAM_PROBE)
	$(STREAM_PROBE) $(STREAM_PROBE_ARGS)

audio-verify: $(STREAM_PROBE)
	$(PYTHON) audio-verify.py --probe $(STREAM_PROBE) $(AUDIO_VERIFY_ARGS)

audio-generated-verify: $(STREAM_PROBE)
	$(PYTHON) audio-verify.py --probe $(STREAM_PROBE) $(AUDIO_GENERATED_VERIFY_ARGS)

audio-capture: $(STREAM_PROBE)
	$(PYTHON) audio-capture.py --probe $(STREAM_PROBE) $(AUDIO_CAPTURE_ARGS)

audio-cd-bitperfect: $(STREAM_PROBE)
	$(PYTHON) audio-capture.py --probe $(STREAM_PROBE) $(AUDIO_CD_BITPERFECT_ARGS)

audio-cd-ready:
	$(MAKE) audio-cd-bitperfect
	$(MAKE) audio-listen AUDIO_LISTEN_ARGS="$(AUDIO_CD_READY_AUDIO_LISTEN_ARGS)"

audio-listen: $(STREAM_PROBE)
	$(PYTHON) audio-capture.py --probe $(STREAM_PROBE) $(AUDIO_LISTEN_ARGS)
	$(AUDIO_PLAYER) $(AUDIO_LISTEN_OUTPUT)

audio-hw:
	$(PYTHON) serial-command.py "audio hw" --expect "external_codec=0" --expect "i2sc=not_configured" --expect "needs_external_codec_board" $(SERIAL_COMMAND_ARGS)

audio-wav-verify:
	$(PYTHON) audio-wav-verify.py $(AUDIO_WAV_VERIFY_ARGS)

widget-verify:
	$(MAKE) -C ../.. widget-control
	$(PYTHON) widget-verify.py --widget-control $(WIDGET_CONTROL) $(WIDGET_VERIFY_ARGS)

board-verify: all $(STREAM_PROBE)
	$(MAKE) widget-verify WIDGET_VERIFY_ARGS="$(BOARD_WIDGET_VERIFY_ARGS)"
	$(MAKE) audio-generated-verify AUDIO_GENERATED_VERIFY_ARGS="$(BOARD_AUDIO_VERIFY_ARGS)"

board-ready:
	$(MAKE) board-verify
	$(MAKE) audio-listen AUDIO_LISTEN_ARGS="$(BOARD_READY_AUDIO_LISTEN_ARGS)"

flash-ready: flash
	sleep $(FLASH_READY_DELAY)
	$(MAKE) board-ready

probe:
	openocd -f board/atmel_same70_xplained.cfg -c "init; targets; flash info 0; shutdown"

flash: all
	openocd -f board/atmel_same70_xplained.cfg -c "program $(TARGET).elf verify reset exit"

clean:
	rm -rf $(BUILD_DIR)
