################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

AVR32_COMPILER ?= gcc
AVR32_GCC ?= avr32-gcc
AVR32_CLANG ?= clang
AVR32_SYSROOT ?= $(patsubst %/bin,%/avr32,$(AVR32BIN))
LINKFLAGS ?= -mpart=uc3a3256

AVR32_CC := $(AVR32_GCC)
AVR32_AS := $(AVR32_GCC)
AVR32_LD := $(AVR32_GCC)
AVR32_CCFLAGS :=
AVR32_ASFLAGS_EXTRA :=
AVR32_AS_DEBUG_FLAG := -Wa,-g
AVR32_LDFLAGS :=

ifeq ($(AVR32_COMPILER),clang)
AVR32_CC := $(AVR32_CLANG)
AVR32_AS := $(AVR32_CLANG)
AVR32_LD := $(AVR32_CLANG)
AVR32_TARGET_FLAGS := --target=avr32 --sysroot=$(AVR32_SYSROOT) -B$(AVR32BIN)
AVR32_CCFLAGS := $(AVR32_TARGET_FLAGS) -fgnu89-inline -Wno-expansion-to-defined
AVR32_ASFLAGS_EXTRA := $(AVR32_TARGET_FLAGS)
AVR32_AS_DEBUG_FLAG :=
AVR32_LDFLAGS := $(AVR32_TARGET_FLAGS) -fuse-ld=lld
endif

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include src/subdir.mk
-include src/SOFTWARE_FRAMEWORK/UTILS/STARTUP_FILES/GCC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/UTILS/DEBUG/subdir.mk
-include src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/HID/subdir.mk
-include src/SOFTWARE_FRAMEWORK/SERVICES/USB/CLASS/AUDIO/subdir.mk
-include src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/subdir.mk
-include src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/MemMang/subdir.mk
-include src/SOFTWARE_FRAMEWORK/SERVICES/FREERTOS/Source/portable/GCC/AVR32_UC3/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/WDT/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/USBB/ENUM/DEVICE/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/USART/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/TWIM/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/TC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/SSC/I2S/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/RTC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/PM/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/INTC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/DRIVERS/EIC/subdir.mk
-include src/SOFTWARE_FRAMEWORK/BOARDS/SDRwdgtLite/subdir.mk
-include src/SOFTWARE_FRAMEWORK/ASM/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables

# All Target
all: widget.elf

# Tool invocations
widget.elf: $(OBJS) $(USER_OBJS)
	@echo Link $@
	@$(AVR32_LD) $(AVR32_LDFLAGS) -nostartfiles -Wl,--gc-sections -Wl,-e,_trampoline $(LINKFLAGS) -Wl,--gc-sections --rodata-writable -Wl,--direct-data -o"widget.elf" $(OBJS) $(USER_OBJS) $(LIBS)
# Other Targets
clean:
	-$(RM) $(OBJS)$(USER_OBJS)$(C_DEPS)$(EXECUTABLES) widget.elf

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
