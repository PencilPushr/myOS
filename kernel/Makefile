# kernel/Makefile

CC = gcc
AS = gcc
CFLAGS = -ffreestanding -fno-stack-protector -mno-red-zone -nostdlib \
         -Wall -Wextra -I..
ASFLAGS = -ffreestanding -nostdlib
LDFLAGS = -T linker.ld -nostdlib -z noexecstack

# Object files
OBJS = main.o gdt.o gdt_asm.o idt.o idt_asm.o pic.o exceptions.o keyboard.o

.PHONY: all clean

all: kernel.bin

# C source files
main.o: main.c ../common/bootinfo.h gdt.h idt.h pic.h exceptions.h keyboard.h
	$(CC) $(CFLAGS) -c main.c -o main.o

gdt.o: gdt.c gdt.h
	$(CC) $(CFLAGS) -c gdt.c -o gdt.o

idt.o: idt.c idt.h gdt.h pic.h
	$(CC) $(CFLAGS) -c idt.c -o idt.o

pic.o: pic.c pic.h
	$(CC) $(CFLAGS) -c pic.c -o pic.o

exceptions.o: exceptions.c exceptions.h idt.h ../common/bootinfo.h
	$(CC) $(CFLAGS) -c exceptions.c -o exceptions.o

keyboard.o: keyboard.c keyboard.h idt.h pic.h
	$(CC) $(CFLAGS) -c keyboard.c -o keyboard.o

# Assembly files
gdt_asm.o: gdt_asm.S
	$(AS) $(ASFLAGS) -c gdt_asm.S -o gdt_asm.o

idt_asm.o: idt_asm.S
	$(AS) $(ASFLAGS) -c idt_asm.S -o idt_asm.o

# Link everything
kernel.elf: $(OBJS) linker.ld
	$(CC) $(LDFLAGS) $(OBJS) -o kernel.elf

# Create flat binary
kernel.bin: kernel.elf
	objcopy -O binary kernel.elf kernel.bin

clean:
	rm -f *.o *.elf *.bin
