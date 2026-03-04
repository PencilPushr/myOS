# MyOS Project Context
## Use this to continue development in a new chat

Paste this document at the start of a new chat along with uploading the latest tarball.

---

## Project Overview

Custom x86-64 operating system with UEFI bootloader. Written from scratch with custom UEFI headers (not using EDK2, only gnu-efi for crt0/linker infrastructure).

**Repository Structure:**
```
myos/
├── efi/                    # Custom UEFI headers (from UEFI spec)
│   ├── types.h             # UINT64, EFI_STATUS, EFIAPI macro
│   ├── tables.h            # EFI_SYSTEM_TABLE, EFI_BOOT_SERVICES
│   └── protocols/          # GOP, filesystem, text protocols
├── common/
│   └── bootinfo.h          # Shared bootloader→kernel interface
├── bootloader/
│   └── main.c              # UEFI bootloader (loads kernel, exits boot services)
├── kernel/
│   ├── main.c              # Entry, console, main loop
│   ├── gdt.c/h, gdt_asm.S  # Global Descriptor Table
│   ├── idt.c/h, idt_asm.S  # Interrupt Descriptor Table (48 stubs)
│   ├── pic.c/h             # 8259 PIC driver
│   ├── exceptions.c/h      # CPU exception handlers
│   ├── keyboard.c/h        # PS/2 keyboard driver
│   └── linker.ld           # Load kernel at 0x100000 (1MB)
└── Makefile
```

---

## Completed Phases

### ✅ Phase 1: Boot (100%)
- UEFI bootloader with custom headers
- Loads kernel.bin from ESP filesystem
- Gets framebuffer via GOP protocol
- Finds ACPI RSDP
- Gets memory map, exits boot services
- Jumps to kernel at 1MB

**Key Decision:** efi_main() must NOT have EFIAPI attribute (gnu-efi's crt0 calls it with System V ABI, not MS ABI)

### ✅ Phase 2: CPU Setup (100%)
- GDT with kernel/user segments + TSS
- IDT with 256 entries (exceptions 0-31, IRQs 32-47)
- PIC remapped (IRQ 0-15 → INT 32-47)
- Exception handlers with visual debug output
- Timer interrupt (IRQ0, ~18.2 Hz)
- Keyboard driver (IRQ1, US QWERTY, scan code translation)
- 8x8 bitmap font console

---

## Current Technical State

**Memory Layout:**
- Kernel loaded at 0x100000 (1MB)
- Framebuffer at address from GOP (typically 0x80000000+)
- Stack: wherever UEFI left it (needs proper setup in Phase 3)

**Interrupts:**
- GDT loaded with lgdt, segments reloaded
- IDT loaded with lidt
- PIC initialized and remapped
- Timer and keyboard IRQs unmasked
- Interrupts enabled (sti)

**Console:**
- 8x8 hardcoded bitmap font
- Simple cursor tracking
- Prints to framebuffer
- No scrolling yet (wraps to top)

---

## Build & Run

```bash
# Dependencies (Ubuntu/Debian)
sudo apt install build-essential gnu-efi qemu-system-x86 ovmf

# Build
cd myos && make

# Run
make run
# Or manually:
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
    -drive format=raw,file=fat:rw:esp -m 256M
```

---

## Next Phases (Not Started)

### Phase 3: Memory Management
- [ ] Physical memory allocator (bitmap from UEFI memory map)
- [ ] Page table setup (4-level, identity map + higher-half)
- [ ] Kernel heap (kmalloc/kfree)

### Phase 4: Processes
- [ ] Process/thread structures
- [ ] Context switching
- [ ] Scheduler
- [ ] User mode (ring 3)
- [ ] System calls

### Phase 5: Drivers
- [ ] Full keyboard driver (extended keys)
- [ ] Storage (AHCI or virtio)
- [ ] PCI enumeration

### Phase 6: Filesystem
- [ ] VFS layer
- [ ] FAT32 or ext2 driver

### Phase 7: User Space
- [ ] ELF loader
- [ ] Basic libc
- [ ] Shell

---

## Key Code Patterns

**Port I/O:**
```c
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

**Interrupt Handler Signature:**
```c
typedef void (*interrupt_handler_t)(struct InterruptFrame *frame);
void idt_set_handler(uint8_t vector, interrupt_handler_t handler);
```

**Drawing to Framebuffer:**
```c
extern struct FramebufferInfo *g_fb;  // Set in kernel_main
uint32_t *pixels = (uint32_t *)g_fb->base;
uint32_t pitch = g_fb->pitch / 4;  // pixels per scanline
pixels[y * pitch + x] = 0x00RRGGBB;
```

---

## Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| Triple fault on boot | Check efi_main doesn't have EFIAPI attribute |
| No interrupts firing | Check PIC remapped, IRQ unmasked, sti executed |
| Keyboard not working | Unmask IRQ1: `pic_unmask_irq(1)` |
| Crash in exception | Check ISR stub pushes correct error code (some exceptions don't push one) |

---

## How to Continue

1. Upload the tarball (myos-phase2.tar.gz)
2. Paste this context document
3. Say "Continue with Phase 3: Memory Management" (or whichever phase)
4. Claude will extract the tarball and continue from where we left off

---

*Last updated: Phase 2 complete (GDT, IDT, PIC, exceptions, timer, keyboard)*
