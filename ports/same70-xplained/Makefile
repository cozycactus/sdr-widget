TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size
HOST_CC ?= clang
PYTHON ?= python3

BUILD_DIR := build
TARGET := $(BUILD_DIR)/same70-blinky
STREAM_PROBE := $(BUILD_DIR)/coreaudio-stream-probe
UAC_CONTROL := $(BUILD_DIR)/uac-control
STREAM_PROBE_ARGS ?=
UAC_VERIFY_ARGS ?= --verify
AUDIO_VERIFY_ARGS ?=
AUDIO_GENERATED_VERIFY_ARGS ?= --seconds 1 --skip-loopback --skip-final-loopback --skip-serial-counter-check
AUDIO_CAPTURE_ARGS ?=
AUDIO_WAV_VERIFY_ARGS ?=
SERIAL_COMMAND_ARGS ?=
WIDGET_VERIFY_ARGS ?=
WIDGET_CONTROL ?= ../../widget-control
BOARD_WIDGET_VERIFY_ARGS ?= --set-current-check
BOARD_UAC_VERIFY_ARGS ?= $(UAC_VERIFY_ARGS)
BOARD_AUDIO_VERIFY_ARGS ?= $(AUDIO_GENERATED_VERIFY_ARGS)
BOARD_AUDIO_HW_ARGS ?= $(SERIAL_COMMAND_ARGS)
BOARD_READY_AUDIO_LISTEN_ARGS ?= $(AUDIO_LISTEN_ARGS)
FLASH_READY_DELAY ?= 2
EXTERNAL_CODEC_PIN_MAP ?= external-codec-pin-map.md
EXTERNAL_CODEC_WIRING_CHECKLIST ?= external-codec-wiring-checklist.md
EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP ?= external-codec-original-uc3a3-map.md
EXTERNAL_CODEC_CLOCK_PLAN ?= external-codec-low-jitter-clock-plan.md
EXTERNAL_CODEC_I2S_CS4272 ?= external-codec-i2s-cs4272-bitperfect.md
EXTERNAL_CODEC_CLOCK_MODEL ?= external-codec-clock-model.py
EXTERNAL_DAC_AD1856_MONO ?= external-dac-ad1856-mono-test.md
EXTERNAL_DAC_AD1856_FORMATTER ?= external-dac-ad1856-low-jitter-formatter.md
EXTERNAL_DAC_AD1856_UPDUINO_V31 ?= external-dac-ad1856-upduino-v31.md
AD1856_FORMATTER_HDL_DIR ?= hdl/ad1856_formatter
AUDIO_CD_INPUT_WAV ?= /tmp/same70-loop-cd-input.wav
AUDIO_CD_OUTPUT_WAV ?= /tmp/same70-loop-cd-output.wav
AUDIO_CD_BITPERFECT_ARGS ?= --source loop --rate 44100 --bits 16 --seconds 2 --output $(AUDIO_CD_INPUT_WAV) --output-wav $(AUDIO_CD_OUTPUT_WAV)
AUDIO_CD_READY_AUDIO_LISTEN_ARGS ?= $(AUDIO_LISTEN_ARGS)
AUDIO_LISTEN_OUTPUT ?= /tmp/same70-melody-listen.wav
AUDIO_LISTEN_ARGS ?= --source melody --rate 44100 --bits 16 --seconds 4 --output $(AUDIO_LISTEN_OUTPUT) --skip-serial-counter-check --leave-source
AUDIO_PLAYER ?= afplay
LIBUSB_CFLAGS ?= $(shell pkg-config --cflags libusb-1.0 2>/dev/null)
LIBUSB_LIBS ?= $(shell pkg-config --libs libusb-1.0 2>/dev/null || echo -lusb-1.0)

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

SRCS := startup_same70.c main.c same70_audio_hw.c same70_clock.c same70_usb.c
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean flash flash-ready probe stream-probe uac-verify audio-verify audio-generated-verify audio-capture audio-cd-bitperfect audio-cd-ready audio-listen audio-hw audio-wav-verify ad1856-formatter-sim ad1856-formatter-vcd ad1856-formatter-synth ad1856-formatter-board-synth ad1856-formatter-bitstream-help ad1856-formatter-bitstream ad1856-formatter-upduino-v31-bitstream external-dac-ad1856-mono external-dac-ad1856-formatter external-dac-ad1856-upduino-v31 external-codec-clock-model external-codec-i2s-cs4272 external-codec-original-map external-codec-clock-plan external-codec-wiring-checklist external-codec-preflight widget-verify board-verify board-ready

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

$(UAC_CONTROL): uac-control.c | $(BUILD_DIR)
	$(HOST_CC) -Wall -Wextra -std=c11 $(LIBUSB_CFLAGS) $< -o $@ $(LIBUSB_LIBS)

stream-probe: $(STREAM_PROBE)
	$(STREAM_PROBE) $(STREAM_PROBE_ARGS)

uac-verify: $(UAC_CONTROL)
	$(UAC_CONTROL) $(UAC_VERIFY_ARGS)
	$(PYTHON) serial-command.py "audio ctl" --expect "mic_mute=0" --expect "spk_mute=0" --expect "mic_vol=0x00000000/0x00000000" --expect "spk_vol=0x00000000/0x00000000" $(SERIAL_COMMAND_ARGS)

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
	$(PYTHON) serial-command.py "audio hw" --timeout 4.0 \
		--expect "external_codec=0" \
		--expect "i2sc=not_configured" \
		--expect "candidate=header_digital_audio_ssc_style" \
		--expect "TD=PD26/J502.1" \
		--expect "headers=J502,J504,J505,J507,EXT1" \
		--expect "voltage=3v3_only_no_5v_logic" \
		--expect "external-codec-original-uc3a3-map.md" \
		--expect "external-codec-low-jitter-clock-plan.md" \
		--expect "external-codec-pin-map.md" \
		--expect "external-codec-i2s-cs4272-bitperfect.md" \
		--expect "external-codec-clock-model.py" \
		--expect "external_low_jitter" \
		--expect "CS4272=I2S_TD_RD_BCLK_LRCK" \
		--expect "CS4272_PRACTICAL_I2S_ADC_DAC_PLAN" \
		--expect "cs4272_master_256fs" \
		--expect "44k16_mclk=11289600" \
		--expect "feedback_44k1=0x00058333" \
		--expect "48k24_mclk=12288000" \
		--expect "feedback_48k=0x00060000" \
		--expect "MCLK=external_to_codecs" \
		--expect "AD1856=mono_DATA_TD_CLK_TK_LE_TF" \
		--expect "external-dac-ad1856-mono-test.md" \
		--expect "external-dac-ad1856-low-jitter-formatter.md" \
		--expect "formatter_owns_ad1856_data_clk_le" \
		--expect "UC3A3_AK5394_ADC_AD1856_MONO_DAC_TEST_ES9023_REFERENCE" \
		--expect "cs4272_status_only_then_clock_only_probe" \
		--expect "needs_external_codec_board" \
		$(SERIAL_COMMAND_ARGS)

external-dac-ad1856-mono:
	@test -f "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "one AD1856" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "mono" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "16-bit PCM" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "not an I2S DAC" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "DATA.*CLK.*LE" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "TD.*PD26.*J502 pin 1" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "TK.*PB1.*J505 pin 8.*J507 pin 4" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "TF.*PB0.*J505 pin 7.*J507 pin 5" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "bipolar supplies" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "44.1 kHz / 16-bit" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "low-going.*LE.*pulse" "$(EXTERNAL_DAC_AD1856_MONO)"
	@grep -q "external-dac-ad1856-low-jitter-formatter.md" "$(EXTERNAL_DAC_AD1856_MONO)"
	@printf '%s\n' "external-dac-ad1856-mono: pass"

external-dac-ad1856-formatter:
	@test -f "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "preferred AD1856 architecture" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "external XO" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "formatter DATA/CLK/LE" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "SAME70 TD" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "SAME70 must not generate AD1856.*CLK.*LE" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "TD.*PD26.*J502 pin 1" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "TK.*PB1.*J505 pin 8.*J507 pin 4" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "TF.*PB0.*J505 pin 7.*J507 pin 5" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "left channel sample exactly" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "gated 16-pulse AD1856 clock" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "explicit USB feedback" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "hdl/ad1856_formatter" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@grep -q "ad1856-formatter-sim" "$(EXTERNAL_DAC_AD1856_FORMATTER)"
	@printf '%s\n' "external-dac-ad1856-formatter: pass"

external-dac-ad1856-upduino-v31:
	@test -f "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@test -f "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "UPduino v3.1" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "iCE40UP5K" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "clk_xo.*gpio_35" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "same_td.*gpio_23" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "same_tk.*gpio_25" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "same_tf.*gpio_26" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "ad1856_data.*gpio_27" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "ad1856_clk.*gpio_32" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "ad1856_le.*gpio_31" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "reset_n.*gpio_37" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "TD.*PD26.*J502 pin 1" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "TK.*PB1.*J505 pin 8.*J507 pin 4" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "TF.*PB0.*J505 pin 7.*J507 pin 5" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "AD1856 pin 7" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "AD1856 pin 5" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "AD1856 pin 6" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "3.3 V logic only" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "ad1856_formatter_board.v" "$(EXTERNAL_DAC_AD1856_UPDUINO_V31)"
	@grep -q "set_io clk_xo 35" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io same_td 23" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io same_tk 25" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io same_tf 26" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io ad1856_data 27" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io ad1856_clk 32" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io ad1856_le 31" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@grep -q "set_io reset_n 37" "$(AD1856_FORMATTER_HDL_DIR)/constraints/upduino-v3.1-ad1856.pcf.example"
	@printf '%s\n' "external-dac-ad1856-upduino-v31: pass"

external-codec-clock-model:
	$(PYTHON) "$(EXTERNAL_CODEC_CLOCK_MODEL)" --firmware same70_usb.c

external-codec-i2s-cs4272: external-codec-clock-model
	@test -f "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "CS4272-class" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "CS4272 master-mode 256fs" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "I2S serial audio" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "external_codec=0" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "i2sc=not_configured" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "needs_external_codec_board" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "external low-jitter" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "SAME70 is an SSC-style" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "no DSP" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "no resampling" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "explicit USB feedback" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "logic analyzer" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "TD.*PD26.*J502 pin 1" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "RD.*PA10.*J504 pin 2" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "PCK0.*diagnostic" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "external-codec-clock-model" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@grep -q "CS4272_F2.pdf" "$(EXTERNAL_CODEC_I2S_CS4272)"
	@printf '%s\n' "external-codec-i2s-cs4272: pass"

external-codec-wiring-checklist: external-codec-original-map external-codec-clock-plan external-dac-ad1856-mono external-dac-ad1856-formatter external-dac-ad1856-upduino-v31 external-codec-i2s-cs4272
	@test -f "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external_codec=0" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "i2sc=not_configured" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "needs_external_codec_board" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "CS4272-class I2S codec" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external-codec-i2s-cs4272-bitperfect.md" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "UC3A3 original AK5394" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "ES9023" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "AD1856" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external-dac-ad1856-low-jitter-formatter.md" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external-codec-low-jitter-clock-plan.md" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "generic USB/I2S audio module" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "The codec board drives any SAME70 I/O above 3.3 V logic" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "instead of feeding MCLK directly from the external low-jitter clock board" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external-codec-preflight" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "board-ready" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "external-codec acceptance gate" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "TD.*PD26.*J502 pin 1" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "RD.*PA10.*J504 pin 2" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "RF.*PD24.*J504 pin 1" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "RK.*PA22.*J504 pin 3" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "TK.*PB1.*J505 pin 8 or J507 pin 4" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "TF.*PB0.*J505 pin 7 or J507 pin 5" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "AD_MCLK.*DA_MCLK.*direct to ADC/DAC" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "PCK0.*not codec MCLK.*PB13.*J504 pin 5 or J507 pin 19" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "explicit feedback endpoint 4" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@grep -q "PC17.*EXT1 pin 10" "$(EXTERNAL_CODEC_WIRING_CHECKLIST)"
	@printf '%s\n' "external-codec-wiring-checklist: pass"

external-codec-preflight: external-codec-wiring-checklist external-dac-ad1856-mono external-dac-ad1856-formatter external-codec-clock-model external-codec-i2s-cs4272
	@test -f "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "external_codec=0" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "i2sc=not_configured" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "needs_external_codec_board" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "3.3 V logic" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "UC3A3" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "ES9023" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "AD1856" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "AD1856 Low-Jitter Formatter Signals" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "external-codec-low-jitter-clock-plan.md" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "not a generic USB/I2S audio module" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PD26 | J502 pin 1" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PA10 | J504 pin 2" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PD24 | J504 pin 1" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PA22 | J504 pin 3" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PB1 | J505 pin 8 or J507 pin 4" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PB0 | J505 pin 7 or J507 pin 5" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "No preferred SAME70 route" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PB13 | J504 pin 5 or J507 pin 19" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "not external MCLK" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "PC17 | EXT1 pin 10" "$(EXTERNAL_CODEC_PIN_MAP)"
	@grep -q "external-dac-ad1856-low-jitter-formatter.md" "$(EXTERNAL_CODEC_PIN_MAP)"
	$(MAKE) audio-hw SERIAL_COMMAND_ARGS="$(SERIAL_COMMAND_ARGS)"
	@printf '%s\n' "external-codec-preflight: pass; external codec remains disabled"

external-codec-clock-plan: external-codec-clock-model
	@test -f "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "external low-jitter clock board" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "audio slave for both receive and transmit" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "CS4272-class I2S codec" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "external-codec-i2s-cs4272-bitperfect.md" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "ADC DATA.*SAME70 RD" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "SAME70 TD.*DAC DATA" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "AD1856" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "external-dac-ad1856-low-jitter-formatter.md" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "formatter owns AD1856" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "BCLK.*RK.*TK" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "LRCK.*RF.*TF" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "MCLK.*Feed codecs directly" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "preferred MCLK source for the external ADC/DAC board" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "22.5792 MHz" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "24.576 MHz" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "explicit USB feedback endpoint 4" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@grep -q "firmware must not resample audio" "$(EXTERNAL_CODEC_CLOCK_PLAN)"
	@printf '%s\n' "external-codec-clock-plan: pass"

external-codec-original-map:
	@test -f "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "DOCS-SDR-Widget-kit/UC3A3" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "AT32UC3A3256-2.12.pdf" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "MISC_IO-3.10.pdf" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "ES9023-DAC.pdf" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "AD_MCLK.*12.288 MHz" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "DA_MCLK.*24.576 MHz" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "1 | .*AD_SDATA" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "1 | .*DA_MCLK" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "ES9023" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "AD1856" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "external-dac-ad1856-low-jitter-formatter.md" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "AK5394_AD_MCLK.*PC04" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "DA_SDATA.*TD.*PD26" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "AD_SDATA.*RD.*PA10" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "external-codec-low-jitter-clock-plan.md" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@grep -q "Do not treat .*PCK0.*external codec MCLK source" "$(EXTERNAL_CODEC_ORIGINAL_UC3A3_MAP)"
	@printf '%s\n' "external-codec-original-map: pass"

audio-wav-verify:
	$(PYTHON) audio-wav-verify.py $(AUDIO_WAV_VERIFY_ARGS)

ad1856-formatter-sim:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) sim

ad1856-formatter-vcd:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) sim-vcd

ad1856-formatter-synth:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) synth-ice40

ad1856-formatter-board-synth:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) synth-ice40-board

ad1856-formatter-bitstream-help:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) bitstream-help

ad1856-formatter-bitstream:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) bitstream-ice40

ad1856-formatter-upduino-v31-bitstream:
	$(MAKE) -C $(AD1856_FORMATTER_HDL_DIR) upduino-v31-bitstream

widget-verify:
	$(MAKE) -C ../.. widget-control
	$(PYTHON) widget-verify.py --widget-control $(WIDGET_CONTROL) $(WIDGET_VERIFY_ARGS)

board-verify: all $(STREAM_PROBE) $(UAC_CONTROL)
	$(MAKE) widget-verify WIDGET_VERIFY_ARGS="$(BOARD_WIDGET_VERIFY_ARGS)"
	$(MAKE) uac-verify UAC_VERIFY_ARGS="$(BOARD_UAC_VERIFY_ARGS)"
	$(MAKE) audio-generated-verify AUDIO_GENERATED_VERIFY_ARGS="$(BOARD_AUDIO_VERIFY_ARGS)"
	$(MAKE) audio-hw SERIAL_COMMAND_ARGS="$(BOARD_AUDIO_HW_ARGS)"

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
