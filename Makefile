# Top-level Makefile

.PHONY: all run clean

all:
	$(MAKE) -C bootloader
	$(MAKE) -C kernel
	mkdir -p esp/EFI/BOOT
	cp bootloader/BOOTX64.EFI esp/EFI/BOOT/
	cp kernel/kernel.bin esp/EFI/BOOT/

run: all
	qemu-system-x86_64 \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive format=raw,file=fat:rw:esp \
		-m 256M \
		-net none

clean:
	$(MAKE) -C bootloader clean
	$(MAKE) -C kernel clean
	rm -rf esp
