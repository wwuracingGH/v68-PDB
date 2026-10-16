PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

TARGET    = v68pdb
BUILD_DIR = build
LDSCRIPT  = STM32H533xx_FLASH.ld

FREERTOS      = Drivers/FreeRTOS-Kernel
FREERTOS_PORT = $(FREERTOS)/portable/GCC/ARM_CM33_NTZ/non_secure

C_SOURCES = \
	$(wildcard Core/Src/*.c) \
	Drivers/printf/printf.c \
	$(FREERTOS)/tasks.c \
	$(FREERTOS)/list.c \
	$(FREERTOS)/queue.c \
	$(FREERTOS)/timers.c \
	$(FREERTOS)/portable/MemMang/heap_1.c \
	$(FREERTOS_PORT)/port.c \
	$(FREERTOS_PORT)/portasm.c

ASM_SOURCES = Core/Startup/startup_stm32h533xx.s

MCU = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

INCLUDES = \
	-ICore/Inc \
	-IDrivers/CMSIS/Core/Include \
	-IDrivers/CMSIS/Device/ST/STM32H5/Include \
	-IDrivers/printf \
	-I$(FREERTOS)/include \
	-I$(FREERTOS_PORT)

CFLAGS  = $(MCU) -DSTM32H533xx $(INCLUDES) -Og -g3 -Wall -Wextra \
          -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS = $(MCU) -g3 -x assembler-with-cpp
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--print-memory-usage

OBJECTS = $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o) $(ASM_SOURCES:.s=.o))

all: $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

clean:
	rm -rf $(BUILD_DIR)

program: $(BUILD_DIR)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32h5x.cfg -c "program $< verify reset exit"

-include $(OBJECTS:.o=.d)

.PHONY: all clean program
