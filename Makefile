ARM  = arm-none-eabi
CC   = $(ARM)-gcc
LD   = $(ARM)-ld
AS   = $(ARM)-as
OD   = $(ARM)-objdump
OCP  = $(ARM)-objcopy

HOST_CC     = gcc
HOST_CFLAGS = -Wall -Wextra -g -Ilibunix -Ilibpi -Ilibpi/boot

MEMMAP = libpi/memmap
INC    = -Ilibpi -Ilibpi/include -Ilibpi/fat32 -Ilibpi/fat32/external-code

OPT_LEVEL ?= -O2
CFLAGS    = $(OPT_LEVEL) -Wall -Wextra -nostdlib -nostartfiles -ffreestanding \
            -fno-builtin -fno-stack-protector -fno-exceptions \
            -marm -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=softfp \
            -mno-unaligned-access -std=gnu99 $(INC)

ASFLAGS = -nostdlib -nostartfiles -ffreestanding -marm -mcpu=cortex-a53 -mfpu=neon-fp-armv8 $(INC)

LIBPI_COMMON = \
	libpi/mem-barrier.o \
	libpi/src/put-get8.o \
	libpi/libc/putk.o \
	libpi/libc/putchar.o \
	libpi/libc/printk.o \
	libpi/libc/strlen.o \
	libpi/libc/strcpy.o \
	libpi/libc/strcat.o \
	libpi/libc/strcmp.o \
	libpi/libc/memcpy.o \
	libpi/libc/memcmp.o \
	libpi/libc/memset.o \
	libpi/libc/memiszero.o \
	libpi/libc/our-crc32.o \
	libpi/libc/fast-hash.o \
	libpi/libc/kmalloc.o \
	libpi/src/gpio.o \
	libpi/src/uart.o \
	libpi/src/timer.o \
	libpi/src/delay-ncycles.o \
	libpi/src/reboot.o \
	libpi/src/clean-reboot.o \
	libpi/src/rpi-wait.o \
	libpi/cstart.o

FAT32_OBJS = \
	libpi/fat32/fat32.o \
	libpi/fat32/fat32-helpers.o \
	libpi/fat32/fat32-lfn-helpers.o \
	libpi/fat32/mbr.o \
	libpi/fat32/mbr-helpers.o \
	libpi/fat32/pi-sd.o \
	libpi/fat32/external-code/emmc.o \
	libpi/fat32/external-code/mbox.o \
	libpi/fat32/external-code/unicode-utf8.o

MULTICORE_OBJS = \
	libpi/multicore/multicore.o \
	libpi/multicore/multicore-start.o

APP_OBJS  = main.o libpi/start.o libpi/src/interrupts-asm.o $(FAT32_OBJS) $(LIBPI_COMMON) $(MULTICORE_OBJS)
BOOT_OBJS = libpi/boot/boot-main.o libpi/boot/boot-start.o $(LIBPI_COMMON)
ALL_OBJS  = $(APP_OBJS) $(BOOT_OBJS)

DEPS     = $(MEMMAP) ./Makefile

LIBUNIX_SRCS      = $(filter-out libunix/put-get.c libunix/pi-cat.c, $(wildcard libunix/*.c))
LIBUNIX_HOST_OBJS = $(LIBUNIX_SRCS:.c=.host.o)

all: boot/kernel.img main.bin pi3-install

$(ALL_OBJS): $(DEPS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(DEPS)
	$(CC) -c $(ASFLAGS) $< -o $@

kernel.elf: $(APP_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -Wl,-T,$(MEMMAP) -o $@ $(APP_OBJS) -lgcc

bootloader.elf: $(BOOT_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -Wl,-T,$(MEMMAP) -o $@ $(BOOT_OBJS)

kernel.list: kernel.elf
	$(OD) -d kernel.elf > kernel.list

boot/kernel.img: bootloader.elf
	$(OCP) bootloader.elf -O binary $@

main.bin: kernel.elf
	$(OCP) kernel.elf -O binary $@

libunix/%.host.o: libunix/%.c
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

pi3-install: $(LIBUNIX_HOST_OBJS)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(LIBUNIX_HOST_OBJS)

clean::
	rm -f $(ALL_OBJS) $(LIBUNIX_HOST_OBJS) libunix/*.host.o \
	      kernel.elf kernel.list main.bin \
	      bootloader.elf boot/kernel.img pi3-install

.PHONY: all clean
.PRECIOUS: kernel.elf boot/kernel.img
