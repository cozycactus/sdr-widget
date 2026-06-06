TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size

BUILD_DIR := build
TARGET := $(BUILD_DIR)/same70-blinky

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

.PHONY: all clean flash probe

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

probe:
	openocd -f board/atmel_same70_xplained.cfg -c "init; targets; flash info 0; shutdown"

flash: all
	openocd -f board/atmel_same70_xplained.cfg -c "program $(TARGET).elf verify reset exit"

clean:
	rm -rf $(BUILD_DIR)
