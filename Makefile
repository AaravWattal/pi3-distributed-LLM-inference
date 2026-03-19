ARM  = arm-none-eabi
CC   = $(ARM)-gcc
LD   = $(ARM)-ld
AS   = $(ARM)-as
OD   = $(ARM)-objdump
OCP  = $(ARM)-objcopy

HOST_CC     = gcc
HOST_CFLAGS = -Wall -Wextra -g -Ilibunix -Ilibpi -Ilibpi/boot

MEMMAP = libpi/memmap
INC    = -Ilibpi -Ilibpi/include -Ilibpi/vm -Ilibpi/fat32 -Ilibpi/fat32/external-code

# O0 compile --> 2.16 tok/s, O2 compile --> 10.08 tok/s, O3 doesnt work rn
# stats taken at time of NEON, caching enabled, multicore exists but no speedup rn
# NEON brought from 7.82 -> 10.08 tok/s
# precompute RoPE freqs brought from 10.08 -> 10.11 tok/s

OPT_LEVEL ?= -O3
CFLAGS    = $(OPT_LEVEL) -Wall -Wextra -nostdlib -nostartfiles -ffreestanding -ffast-math \
            -fno-builtin -fno-stack-protector -fno-exceptions \
            -marm -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=softfp \
            -mno-unaligned-access -std=gnu99 -DRUN_INFERENCE=$(RUN_INFERENCE) $(INC)

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

VM_OBJS = \
	libpi/vm/mmu.o \
	libpi/vm/mmu-helpers.o \
	libpi/vm/pt-vm.o \
	libpi/vm/your-mmu-asm.o \
	libpi/vm/cache-support.o

COMM_OBJS = \
	libpi/src/pi-comm.o

APP_OBJS_BASE = main.o libpi/start.o libpi/src/interrupts-asm.o $(VM_OBJS) $(LIBPI_COMMON)

APP_OBJS = $(APP_OBJS_BASE) libpi/src/run.o $(FAT32_OBJS) $(MULTICORE_OBJS)
# Distributed build adds the comm module
APP_OBJS_DIST = $(APP_OBJS) $(COMM_OBJS)

BOOT_OBJS = libpi/boot/boot-main.o libpi/boot/boot-start.o $(LIBPI_COMMON)
ALL_OBJS  = $(APP_OBJS_DIST) $(BOOT_OBJS)

DEPS     = $(MEMMAP) ./Makefile

LIBUNIX_SRCS      = $(filter-out libunix/put-get.c libunix/pi-cat.c, $(wildcard libunix/*.c))
LIBUNIX_HOST_OBJS = $(LIBUNIX_SRCS:.c=.host.o)

# Default: single-board build (unchanged behavior)
all: boot/kernel.img main.bin pi3-install

# Distributed build: same binary, auto-detects role at boot via GPIO 7
distributed: boot/kernel.img distributed.bin pi3-install

$(ALL_OBJS): $(DEPS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

libpi/cstart.o: libpi/cstart.c $(DEPS)
	$(CC) $(CFLAGS) -fno-tree-vectorize -c $< -o $@

%.o: %.S $(DEPS)
	$(CC) -c $(ASFLAGS) $< -o $@

kernel.elf: $(APP_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -Wl,-T,$(MEMMAP) -o $@ $(APP_OBJS) -lgcc

# Distributed ELF: adds -DPI_DISTRIBUTED and links comm module.
# We rebuild run.o with the flag using a separate target.
dist-run.o: libpi/src/run.c $(DEPS)
	$(CC) $(CFLAGS) -DPI_DISTRIBUTED -c $< -o $@

kernel-dist.elf: $(APP_OBJS_BASE) dist-run.o $(COMM_OBJS) $(FAT32_OBJS) $(MULTICORE_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -DPI_DISTRIBUTED -Wl,-T,$(MEMMAP) -o $@ \
		$(APP_OBJS_BASE) dist-run.o $(COMM_OBJS) $(FAT32_OBJS) $(MULTICORE_OBJS) -lgcc

bootloader.elf: $(BOOT_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -Wl,-T,$(MEMMAP) -o $@ $(BOOT_OBJS)

kernel.list: kernel.elf
	$(OD) -d kernel.elf > kernel.list

boot/kernel.img: bootloader.elf
	$(OCP) bootloader.elf -O binary $@

main.bin: kernel.elf
	$(OCP) kernel.elf -O binary $@

distributed.bin: kernel-dist.elf
	$(OCP) kernel-dist.elf -O binary $@

libunix/%.host.o: libunix/%.c
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

pi3-install: $(LIBUNIX_HOST_OBJS)
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $(LIBUNIX_HOST_OBJS)

clean::
	rm -f $(ALL_OBJS) $(LIBUNIX_HOST_OBJS) libunix/*.host.o \
	      kernel.elf kernel.list main.bin \
	      kernel-dist.elf distributed.bin dist-run.o \
	      bootloader.elf boot/kernel.img pi3-install

.PHONY: all clean distributed
.PRECIOUS: kernel.elf kernel-dist.elf boot/kernel.img
