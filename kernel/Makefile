# kernel/Makefile

CC = gcc
CFLAGS = -ffreestanding -fno-stack-protector -mno-red-zone -nostdlib \
         -Wall -Wextra -I..
LDFLAGS = -T linker.ld -nostdlib

.PHONY: all clean

all: kernel.bin

main.o: main.c ../common/bootinfo.h
	$(CC) $(CFLAGS) -c main.c -o main.o

kernel.elf: main.o linker.ld
	$(CC) $(LDFLAGS) main.o -o kernel.elf

kernel.bin: kernel.elf
	objcopy -O binary kernel.elf kernel.bin

clean:
	rm -f *.o *.elf *.bin
