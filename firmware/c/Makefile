# TinyKey C firmware (CH32V003F4P6) - ch32v003fun build
#
#   make                 build tinykey_led_test.bin / .hex
#   make flash           write it with minichlink (WCH-Link on SWIO)
#   make clean
#
# Needs riscv-none-elf-gcc (xPack) on PATH and ch32v003fun cloned as a sibling
# of this repository:
#
#   git clone https://github.com/cnlohr/ch32v003fun.git ../../../ch32v003fun
#
# Override with:  make CH32FUN=/path/to/ch32v003fun/ch32v003fun
#                 make MINICHLINK=/path/to/minichlink-dir
#                 make PREFIX=/path/to/bin/riscv-none-elf-

TARGET     := tinykey_led_test
TARGET_MCU := CH32V003

PREFIX     ?= riscv-none-elf-
CH32FUN    ?= ../../../ch32v003fun/ch32v003fun
MINICHLINK ?= ../../../ch32v003fun/minichlink

CFLAGS += -g -Os -flto -ffunction-sections -static-libgcc \
          -march=rv32ec -mabi=ilp32e -nostdlib -I. -Wall -I$(CH32FUN)

LDFLAGS += -T $(CH32FUN)/ch32v003fun.ld -Wl,--gc-sections \
           -L $(CH32FUN)/../misc -lgcc

$(TARGET).elf : $(CH32FUN)/ch32v003fun.c $(TARGET).c
	$(PREFIX)gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(TARGET).bin : $(TARGET).elf
	$(PREFIX)size $^
	$(PREFIX)objcopy -O binary $< $(TARGET).bin
	$(PREFIX)objcopy -O ihex   $< $(TARGET).hex

build : $(TARGET).bin

flash : $(TARGET).bin
	$(MINICHLINK)/minichlink -w $(TARGET).bin flash -b

clean :
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).hex

.PHONY : build flash clean
