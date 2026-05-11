#
# About:
#   Makefile building script for Hitagi RAMDLD project.
#
# Author:
#   EXL, ChatGPT-4.1 (GitHub Copilot)
#
# License:
#   MIT
#

# Toolchain prefix (change if using a different toolchain).
CROSS_COMPILE ?= arm-none-eabi-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size
PYTHON  ?= python3

# Parameters.
FLASH_TYPE ?= intel16
PLATFORM ?= LTE1

DEFINES_LTE1      = -DFTR_NEPTUNE_LTE1
ORIGIN_LTE1       = 0x03FD0010
LENGTH_LTE1       = 0x0002FFF0
SIGN_OFFSET_LTE1  = 0x0000F800

DEFINES_LTE2      = -DFTR_NEPTUNE_LTE2
ORIGIN_LTE2       = 0x03FC8014
LENGTH_LTE2       = 0x00037FEC
SIGN_OFFSET_LTE2  = 0x0000F800

DEFINES_LTE1C     = -DFTR_NEPTUNE_LTE1 -DFTR_COMPACT -Wno-unused-function
ORIGIN_LTE1C      = 0x03FD0010
LENGTH_LTE1C      = 0x0002FFF0
SIGN_OFFSET_LTE1C = 0x00001800

DEFINES_LTE2C     = -DFTR_NEPTUNE_LTE2 -DFTR_COMPACT -Wno-unused-function
ORIGIN_LTE2C      = 0x03FC8014
LENGTH_LTE2C      = 0x00037FEC
SIGN_OFFSET_LTE2C = 0x00001800

DEFINES_ARGONLV      = -DFTR_ARGONLV -Wno-unused-function
ORIGIN_ARGONLV       = 0x80000038
LENGTH_ARGONLV       = 0x0001FFC8
OUTPUT_ARGONLV       = $(BIN)
OUTPUT_FORMAT_ARGONLV = elf32-bigarm

OUTPUT_LTE1       = $(LDR)
OUTPUT_LTE2       = $(LDR)
OUTPUT_LTE1C      = $(LDR)
OUTPUT_LTE2C      = $(LDR)

OUTPUT_FORMAT_LTE1  = elf32-bigarm
OUTPUT_FORMAT_LTE2  = elf32-bigarm
OUTPUT_FORMAT_LTE1C = elf32-bigarm
OUTPUT_FORMAT_LTE2C = elf32-bigarm

ARCH_FLAGS_LTE1   = -marm -mbig-endian -march=armv4t -mtune=arm7tdmi-s
ARCH_FLAGS_LTE2   = -marm -mbig-endian -march=armv4t -mtune=arm7tdmi-s
ARCH_FLAGS_LTE1C  = -marm -mbig-endian -march=armv4t -mtune=arm7tdmi-s
ARCH_FLAGS_LTE2C  = -marm -mbig-endian -march=armv4t -mtune=arm7tdmi-s
ARCH_FLAGS_ARGONLV = -marm -mbig-endian -mbe32 -mcpu=arm1136jf-s

CODE_FLAGS_LTE1    = -ffreestanding -fPIE
CODE_FLAGS_LTE2    = -ffreestanding -fPIE
CODE_FLAGS_LTE1C   = -ffreestanding -fPIE
CODE_FLAGS_LTE2C   = -ffreestanding -fPIE
CODE_FLAGS_ARGONLV = -ffreestanding -fno-pic -fno-pie

LINK_FLAGS_LTE1    = -pie -nostdlib
LINK_FLAGS_LTE2    = -pie -nostdlib
LINK_FLAGS_LTE1C   = -pie -nostdlib
LINK_FLAGS_LTE2C   = -pie -nostdlib
LINK_FLAGS_ARGONLV = -EB -static -nostdlib

LINK_ARCH_FLAGS_LTE1    = $(ARCH_FLAGS_LTE1)
LINK_ARCH_FLAGS_LTE2    = $(ARCH_FLAGS_LTE2)
LINK_ARCH_FLAGS_LTE1C   = $(ARCH_FLAGS_LTE1C)
LINK_ARCH_FLAGS_LTE2C   = $(ARCH_FLAGS_LTE2C)

LINKER_LTE1    = $(CC)
LINKER_LTE2    = $(CC)
LINKER_LTE1C   = $(CC)
LINKER_LTE2C   = $(CC)
LINKER_ARGONLV = $(LD)

# Source and objects.
PLATFORM_SRCS_LTE1    = hitagi_lte.c hitagi_lte_protocol.c
PLATFORM_SRCS_LTE2    = hitagi_lte.c hitagi_lte_protocol.c
PLATFORM_SRCS_LTE1C   = hitagi_lte.c hitagi_lte_protocol.c
PLATFORM_SRCS_LTE2C   = hitagi_lte.c hitagi_lte_protocol.c
PLATFORM_SRCS_ARGONLV = hitagi_argon.c hitagi_argon_protocol.c

SRCS  = hitagi.c
SRCS += $(PLATFORM_SRCS_$(PLATFORM))
SRCS += flash_$(FLASH_TYPE).c
OBJS  = $(SRCS:.c=.o)
CLEAN_OBJS = hitagi.o hitagi_lte.o hitagi_lte_protocol.o hitagi_argon.o hitagi_argon_protocol.o flash_intel16.o flash_amd16.o

# Output files.
TARGET = hitagi
ELF    = $(TARGET).elf
BIN    = $(TARGET).bin
RAW_BIN = $(TARGET).payload.bin
LDR    = $(TARGET).ldr

# Flags.
CFLAGS       = $(DEFINES_$(PLATFORM))
CFLAGS      += -Wall -Wextra -pedantic
CFLAGS      += -nostdlib -nostdinc
CFLAGS      += -O2 $(ARCH_FLAGS_$(PLATFORM))
CFLAGS      += $(CODE_FLAGS_$(PLATFORM))
LDFLAGS      = $(LINK_FLAGS_$(PLATFORM)) $(LINK_ARCH_FLAGS_$(PLATFORM))
LDSCRIPT     = hitagi.ld
LIBS         = -T $(LDSCRIPT)

.PHONY: all clean

all: $(OUTPUT_$(PLATFORM))

ifeq ($(PLATFORM),ARGONLV)
$(BIN): $(RAW_BIN)
	$(PYTHON) postlink.py bin/$(PLATFORM)_head.bin $< $@

$(RAW_BIN): $(ELF)
	# $(OBJDUMP) -d $(ELF)
	$(SIZE) $(ELF)
	$(OBJCOPY) -O binary $< $@
else
$(LDR): $(BIN)
	$(PYTHON) postlink.py bin/$(PLATFORM)_head.bin $< bin/$(PLATFORM)_sign.bin $(SIGN_OFFSET_$(PLATFORM)) $@

$(BIN): $(ELF)
	# $(OBJDUMP) -d $(ELF)
	$(SIZE) $(ELF)
	$(OBJCOPY) -O binary $< $@
endif

$(LDSCRIPT): hitagi.lds
	$(PYTHON) prelink.py hitagi.lds $(LDSCRIPT) $(ORIGIN_$(PLATFORM)) $(LENGTH_$(PLATFORM)) $(OUTPUT_FORMAT_$(PLATFORM))

$(ELF): $(OBJS) $(LDSCRIPT)
	$(LINKER_$(PLATFORM)) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLEAN_OBJS) $(ELF) $(BIN) $(RAW_BIN) $(MAP) $(LDSCRIPT) $(LDR)
