# bootloader/Makefile
# 
# We use:
#   - Our own headers (../efi/)
#   - gnu-efi's crt0, linker script, and uefi_call_wrapper (from libgnuefi)
#
# The result is a PE32+ EFI application that UEFI firmware can load.

CC = gcc
LD = ld
OBJCOPY = objcopy

# Our headers, no gnu-efi headers needed!
CFLAGS = -I.. \
         -ffreestanding \
         -fno-stack-protector \
         -fpic \
         -fshort-wchar \
         -mno-red-zone \
         -Wall -Wextra

# Link with gnu-efi infrastructure
# - crt0-efi-x86_64.o: Entry point that calls efi_main
# - libgnuefi.a: Contains _relocate and uefi_call_wrapper
# - elf_x86_64_efi.lds: Linker script for EFI PE format
LDFLAGS = -nostdlib \
          -T /usr/lib/elf_x86_64_efi.lds \
          -shared \
          -Bsymbolic \
          /usr/lib/crt0-efi-x86_64.o \
          -L /usr/lib \
          -lgnuefi

.PHONY: all clean

all: BOOTX64.EFI

main.o: main.c ../efi/efi.h ../common/bootinfo.h
	$(CC) $(CFLAGS) -c main.c -o main.o

bootloader.so: main.o
	$(LD) -nostdlib \
	      -T /usr/lib/elf_x86_64_efi.lds \
	      -shared \
	      -Bsymbolic \
	      /usr/lib/crt0-efi-x86_64.o \
	      main.o \
	      -L /usr/lib \
	      -lgnuefi \
	      -o bootloader.so

# Convert ELF shared object to PE32+ EFI application
BOOTX64.EFI: bootloader.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .reloc \
	           --target=efi-app-x86_64 \
	           bootloader.so BOOTX64.EFI

clean:
	rm -f *.o *.so *.EFI
