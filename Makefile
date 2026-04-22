TARGET = pad
BUILD_DIR = build

# Toolchain - try riscv64-elf-gcc first, fall back to riscv-none-embed-gcc
ifneq ($(shell which riscv64-elf-gcc 2>/dev/null),)
  PREFIX = riscv64-elf-
else ifneq ($(shell which riscv-none-embed-gcc 2>/dev/null),)
  PREFIX = riscv-none-embed-
else
  $(error No RISC-V toolchain found. Install with: brew install riscv64-elf-gcc)
endif

CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# Architecture flags for CH32V307 (RV32IMAFC)
ARCH_FLAGS = -march=rv32imafc_zifencei -mabi=ilp32f -msmall-data-limit=8 -mno-save-restore

# Source files
C_SRCS  = $(wildcard src/*.c)
C_SRCS += $(wildcard vendor/Core/*.c)
C_SRCS += $(wildcard vendor/Debug/*.c)
C_SRCS += $(wildcard vendor/Peripheral/src/*.c)
C_SRCS += vendor/system_ch32v30x.c
S_SRCS  = vendor/Startup/startup_ch32v30x_D8C.S

OBJS  = $(C_SRCS:%.c=$(BUILD_DIR)/%.o)
OBJS += $(S_SRCS:%.S=$(BUILD_DIR)/%.o)
DEPS  = $(OBJS:.o=.d)

# Include paths
INC  = -Isrc
INC += -Ivendor
INC += -Ivendor/Core
INC += -Ivendor/Debug
INC += -Ivendor/Peripheral/inc

# Compiler flags
CFLAGS  = $(ARCH_FLAGS) -Os -g -ffreestanding
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fsigned-char -fmessage-length=0
CFLAGS += -Wall -Wno-unused
CFLAGS += $(INC)
CFLAGS += -DCH32V30x_D8C
CFLAGS += -MMD -MP

ASFLAGS = $(ARCH_FLAGS) -x assembler-with-cpp $(INC) -DCH32V30x_D8C -MMD -MP

LDFLAGS  = $(ARCH_FLAGS) -ffreestanding
LDFLAGS += -T vendor/Ld/Link.ld
LDFLAGS += -nostartfiles -nostdlib -nodefaultlibs
LDFLAGS += -Xlinker --gc-sections
LDFLAGS += -Wl,-Map,$(BUILD_DIR)/$(TARGET).map
LDFLAGS += -lgcc

# Rules
all: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex size

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) $<

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

flash: $(BUILD_DIR)/$(TARGET).bin
	wchisp flash $<

iap: $(BUILD_DIR)/$(TARGET).bin
	python3 tools/iap_flash.py $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean flash size iap

-include $(DEPS)
