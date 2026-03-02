ARM  = arm-none-eabi
CC   = $(ARM)-gcc
LD   = $(ARM)-ld
AS   = $(ARM)-as
OD   = $(ARM)-objdump
OCP  = $(ARM)-objcopy

MEMMAP = libpi/memmap
INC    = -Ilibpi -Ilibpi/include

OPT_LEVEL ?= -O2
CFLAGS    = $(OPT_LEVEL) -Wall -Wextra -nostdlib -nostartfiles -ffreestanding \
            -fno-builtin -fno-stack-protector -fno-exceptions \
            -marm -march=armv6zk -mfpu=vfp -mfloat-abi=hard -std=gnu99 $(INC)

ASFLAGS = -nostdlib -nostartfiles -ffreestanding -marm -march=armv6zk $(INC)

LIBPI_OBJS = \
	libpi/start.o \
	libpi/mem-barrier.o \
	libpi/src/put-get8.o \
	libpi/libc/putk.o \
	libpi/libc/putchar.o \
	libpi/src/gpio.o \
	libpi/src/uart.o \
	libpi/src/timer.o \
	libpi/src/delay-ncycles.o \
	libpi/src/reboot.o \
	libpi/src/clean-reboot.o \
	libpi/src/rpi-wait.o \
	libpi/cstart.o

ALL_OBJS = main.o $(LIBPI_OBJS)
DEPS     = $(MEMMAP) ./Makefile

all: boot/kernel.img

$(ALL_OBJS): $(DEPS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S $(DEPS)
	$(CC) -c $(ASFLAGS) $< -o $@

kernel.elf: $(ALL_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -Wl,-T,$(MEMMAP) -o $@ $(ALL_OBJS)

kernel.list: kernel.elf
	$(OD) -d kernel.elf > kernel.list

boot/kernel.img: kernel.elf
	$(OCP) kernel.elf -O binary $@

clean::
	rm -f $(ALL_OBJS) kernel.elf kernel.list boot/kernel.img

.PHONY: all clean
.PRECIOUS: kernel.elf boot/kernel.img
