# MyOS Tutorial: Building an x86-64 Operating System

## A Hands-On Guide to Phase 1 (Boot) and Phase 2 (CPU Setup)

This tutorial teaches you to build an operating system from scratch. It's designed to be self-contained - you can paste this into a new chat for learning, or follow along and implement yourself.

---

# Part 1: Foundations

Before writing code, you need to understand the environment your OS lives in.

---

## Chapter 1: The Boot Process

### What Happens When You Press Power?

```
┌─────────────────────────────────────────────────────────────────┐
│                    THE BOOT SEQUENCE                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. POWER ON                                                     │
│     └─► CPU starts at reset vector (0xFFFFFFF0)                 │
│                                                                  │
│  2. FIRMWARE (UEFI)                                              │
│     ├─► Initialize hardware (RAM, PCIe, USB)                    │
│     ├─► Find boot device (disk, USB, network)                   │
│     └─► Load bootloader from EFI System Partition               │
│                                                                  │
│  3. BOOTLOADER (our code!)                                       │
│     ├─► Get memory map from firmware                            │
│     ├─► Get framebuffer for graphics                            │
│     ├─► Load kernel from disk                                   │
│     ├─► Exit boot services (take over machine)                  │
│     └─► Jump to kernel                                          │
│                                                                  │
│  4. KERNEL                                                       │
│     ├─► Set up CPU tables (GDT, IDT)                           │
│     ├─► Initialize memory management                            │
│     ├─► Start scheduler                                         │
│     └─► Launch user programs                                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### BIOS vs UEFI

**BIOS (Legacy - 1981):**
- 16-bit real mode
- 1MB address limit initially
- Master Boot Record (MBR) - 512 bytes for bootloader!
- No standard interface for graphics, just VGA

**UEFI (Modern - 2005+):**
- Starts in 64-bit mode (on x86-64)
- Full memory access
- Standardized protocols for everything (disk, graphics, network)
- FAT32 filesystem for boot partition
- We use this!

### Exercise 1.1: Understand the Memory Map

When the computer starts, different memory regions serve different purposes:

```
Physical Memory Layout (typical):

0x00000000 ┌─────────────────────────┐
           │ Real Mode IVT           │ 1KB - Legacy interrupt vectors
0x00000400 ├─────────────────────────┤
           │ BIOS Data Area          │ 256 bytes
0x00000500 ├─────────────────────────┤
           │ Conventional Memory     │ ~640KB usable
0x00080000 ├─────────────────────────┤
           │ EBDA (Extended BIOS)    │ Variable
0x000A0000 ├─────────────────────────┤
           │ Video Memory            │ 128KB (legacy VGA)
0x000C0000 ├─────────────────────────┤
           │ BIOS ROM                │ 256KB
0x00100000 ├─────────────────────────┤ ← 1MB mark
           │                         │
           │ Extended Memory         │ The rest of RAM!
           │ (Usable for OS)         │
           │                         │
0x???????? ├─────────────────────────┤
           │ Memory-mapped devices   │ APIC, PCIe, framebuffer
           └─────────────────────────┘
```

**Question:** Why do we load our kernel at 1MB (0x100000)?

**Answer:** Below 1MB is fragmented with legacy regions. The memory starting at 1MB is typically the first large contiguous usable region.

---

## Chapter 2: Understanding UEFI

### What is UEFI?

UEFI (Unified Extensible Firmware Interface) is a specification that defines:
- How firmware initializes hardware
- How bootloaders are loaded
- Standard interfaces (protocols) for accessing hardware

### The EFI System Partition (ESP)

UEFI looks for bootloaders in a special FAT32 partition:

```
ESP Structure:
/EFI/
  /BOOT/
    BOOTX64.EFI    ← Default bootloader for x86-64
  /Microsoft/
    /Boot/
      bootmgfw.efi  ← Windows bootloader
  /ubuntu/
    grubx64.efi     ← Linux bootloader
```

Our bootloader will be `BOOTX64.EFI`.

### UEFI Protocols

UEFI provides functionality through "protocols" - essentially interfaces/APIs:

| Protocol | Purpose |
|----------|---------|
| Simple Text Output | Print text to screen |
| Graphics Output (GOP) | Framebuffer for graphics |
| Simple File System | Read files from disk |
| Boot Services | Memory allocation, protocol lookup |
| Runtime Services | Clock, variables (persist after boot) |

### Exercise 1.2: UEFI Calling Convention

UEFI uses the Microsoft x64 calling convention, NOT the System V convention used by Linux:

```
Microsoft x64 (UEFI):          System V AMD64 (Linux):
  Arg 1: RCX                     Arg 1: RDI
  Arg 2: RDX                     Arg 2: RSI
  Arg 3: R8                      Arg 3: RDX
  Arg 4: R9                      Arg 4: RCX
  Rest: Stack                    Arg 5: R8
  Return: RAX                    Arg 6: R9
                                 Rest: Stack
                                 Return: RAX
```

**This is critical!** GCC on Linux generates System V code by default. To call UEFI functions, we need:

```c
// Mark function pointer types with MS ABI
#define EFIAPI __attribute__((ms_abi))

// Example: UEFI function pointer type
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);
```

**Question:** What happens if you call a UEFI function without the ms_abi attribute?

**Answer:** Arguments end up in wrong registers. RDI (your arg1) isn't where UEFI expects arg1 (RCX). Crash or undefined behavior.

---

## Chapter 3: Building the Toolchain

### What You Need

```bash
# Ubuntu/Debian
sudo apt install build-essential    # GCC, make, etc.
sudo apt install gnu-efi            # UEFI development files
sudo apt install qemu-system-x86    # Emulator
sudo apt install ovmf               # UEFI firmware for QEMU

# Verify installation
ls /usr/lib/crt0-efi-x86_64.o       # UEFI entry point
ls /usr/lib/elf_x86_64_efi.lds      # Linker script
ls /usr/share/ovmf/OVMF.fd          # UEFI firmware
```

### Understanding gnu-efi

gnu-efi provides infrastructure for building UEFI applications with GCC:

| File | Purpose |
|------|---------|
| `crt0-efi-x86_64.o` | Entry point, calls your efi_main |
| `elf_x86_64_efi.lds` | Linker script for PE32+ format |
| `libgnuefi.a` | Relocation handling |
| Header files | UEFI type definitions |

**Important:** We'll write our own UEFI headers from the spec! This teaches you what's really happening instead of using magic headers.

### Exercise 1.3: PE32+ Format

UEFI executables use the PE32+ format (same as Windows .exe/.dll):

```
PE32+ Structure:
┌─────────────────────┐
│ DOS Header          │ ← "MZ" magic, points to PE header
├─────────────────────┤
│ DOS Stub            │ ← "This program cannot be run in DOS mode"
├─────────────────────┤
│ PE Header           │ ← "PE\0\0" magic
├─────────────────────┤
│ Optional Header     │ ← Entry point, section info
├─────────────────────┤
│ Section Headers     │ ← .text, .data, .reloc
├─────────────────────┤
│ .text section       │ ← Code
├─────────────────────┤
│ .data section       │ ← Initialized data
├─────────────────────┤
│ .reloc section      │ ← Relocation table (UEFI can load anywhere)
└─────────────────────┘
```

**Question:** Why does UEFI use PE32+ instead of ELF?

**Answer:** UEFI was designed by Intel with Microsoft. PE32+ is the Windows executable format. This also makes it easier for Windows to participate in UEFI development.

---

## Chapter 4: UEFI Types and Structures

### Basic Types

UEFI defines its own types for portability:

```c
// Unsigned integers
typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;

// Pointer-sized types
typedef uint64_t  UINTN;     // Natural size for architecture
typedef int64_t   INTN;

// Boolean
typedef uint8_t   BOOLEAN;
#define TRUE  1
#define FALSE 0

// Characters
typedef uint16_t  CHAR16;    // UCS-2 (like UTF-16)
typedef uint8_t   CHAR8;

// Handles (opaque pointers)
typedef void *EFI_HANDLE;

// Status codes
typedef UINTN EFI_STATUS;
```

### Exercise 1.4: Status Codes

UEFI functions return `EFI_STATUS`. The high bit indicates error:

```c
// Success
#define EFI_SUCCESS  0

// Errors (high bit set)
#define EFI_ERROR_BIT           0x8000000000000000ULL
#define EFI_LOAD_ERROR          (EFI_ERROR_BIT | 1)
#define EFI_INVALID_PARAMETER   (EFI_ERROR_BIT | 2)
#define EFI_UNSUPPORTED         (EFI_ERROR_BIT | 3)
#define EFI_BUFFER_TOO_SMALL    (EFI_ERROR_BIT | 5)
#define EFI_NOT_FOUND           (EFI_ERROR_BIT | 14)

// Check for error
#define EFI_ERROR(status)  ((status) & EFI_ERROR_BIT)

// Usage:
EFI_STATUS status = SomeUefiFunction();
if (EFI_ERROR(status)) {
    // Handle error
}
```

**Question:** Why use a high bit for errors instead of negative numbers?

**Answer:** EFI_STATUS is unsigned. Using the high bit means you can use simple bit tests, and the remaining 63 bits can encode specific error information.

### The System Table

When UEFI calls your bootloader, it passes a pointer to the System Table - your gateway to all UEFI services:

```c
typedef struct {
    EFI_TABLE_HEADER                  Hdr;
    CHAR16                           *FirmwareVendor;
    UINT32                            FirmwareRevision;
    EFI_HANDLE                        ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *ConIn;       // Keyboard input
    EFI_HANDLE                        ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *ConOut;      // Text output
    EFI_HANDLE                        StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *StdErr;
    EFI_RUNTIME_SERVICES             *RuntimeServices;  // Clock, variables
    EFI_BOOT_SERVICES                *BootServices;     // The important one!
    UINTN                             NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE          *ConfigurationTable;  // ACPI tables here
} EFI_SYSTEM_TABLE;
```

### Exercise 1.5: Tracing the Path

To print "Hello" to the screen, trace the path:

```
Your code
    │
    ▼
SystemTable (passed to efi_main)
    │
    ├─► ConOut (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)
    │       │
    │       └─► OutputString (function pointer)
    │               │
    │               ▼
    │           UEFI firmware draws text
    │
    ▼
Screen shows "Hello"
```

**Code:**
```c
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    // Get the text output protocol
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut = SystemTable->ConOut;
    
    // Call OutputString (note: UCS-2 string with L prefix)
    ConOut->OutputString(ConOut, L"Hello, UEFI!\r\n");
    
    // Infinite loop
    while (1);
    
    return EFI_SUCCESS;
}
```

---

## Chapter 5: Boot Services

Boot Services are available until you call `ExitBootServices()`. After that, you own the machine.

### Key Boot Services

```c
typedef struct {
    EFI_TABLE_HEADER  Hdr;
    
    // Memory Services
    EFI_ALLOCATE_PAGES     AllocatePages;
    EFI_FREE_PAGES         FreePages;
    EFI_GET_MEMORY_MAP     GetMemoryMap;
    EFI_ALLOCATE_POOL      AllocatePool;      // Like malloc
    EFI_FREE_POOL          FreePool;          // Like free
    
    // Protocol Services
    EFI_HANDLE_PROTOCOL    HandleProtocol;    // Get protocol from handle
    EFI_LOCATE_PROTOCOL    LocateProtocol;    // Find any instance
    
    // Image Services
    EFI_LOAD_IMAGE         LoadImage;
    EFI_START_IMAGE        StartImage;
    EFI_EXIT               Exit;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;  // Point of no return!
    
    // ... many more
} EFI_BOOT_SERVICES;
```

### Exercise 1.6: Getting the Memory Map

The memory map tells you what RAM is usable:

```c
// Memory types
#define EfiReservedMemoryType       0   // Don't touch
#define EfiLoaderCode               1   // Our bootloader code
#define EfiLoaderData               2   // Our bootloader data
#define EfiBootServicesCode         3   // UEFI code (free after ExitBootServices)
#define EfiBootServicesData         4   // UEFI data (free after ExitBootServices)
#define EfiRuntimeServicesCode      5   // Keep mapped (UEFI runtime)
#define EfiRuntimeServicesData      6   // Keep mapped
#define EfiConventionalMemory       7   // FREE! Use this!
#define EfiACPIReclaimMemory        9   // Free after parsing ACPI
#define EfiACPIMemoryNVS           10   // Don't touch

// Memory descriptor
typedef struct {
    UINT32    Type;           // EfiConventionalMemory = usable
    UINT64    PhysicalStart;  // Starting address
    UINT64    VirtualStart;   // For runtime services
    UINT64    NumberOfPages;  // Size in 4KB pages
    UINT64    Attribute;      // Cacheability, etc.
} EFI_MEMORY_DESCRIPTOR;
```

**Getting the memory map (two-call pattern):**
```c
UINTN map_size = 0;
UINTN map_key;
UINTN desc_size;
UINT32 desc_version;
EFI_MEMORY_DESCRIPTOR *map = NULL;

// First call: get required size
EFI_STATUS status = BootServices->GetMemoryMap(
    &map_size, map, &map_key, &desc_size, &desc_version);
// Returns EFI_BUFFER_TOO_SMALL, but map_size now has required size

// Allocate buffer (add extra space - map grows when we allocate!)
map_size += 2 * desc_size;
BootServices->AllocatePool(EfiLoaderData, map_size, (void **)&map);

// Second call: actually get the map
status = BootServices->GetMemoryMap(
    &map_size, map, &map_key, &desc_size, &desc_version);
```

**Question:** Why does allocating memory for the map change the map?

**Answer:** AllocatePool creates a new memory region. The memory map must include this new region, so it grows. Always allocate extra space!

---

## Chapter 6: Graphics with GOP

### What is GOP?

Graphics Output Protocol (GOP) gives you direct framebuffer access:

```c
typedef struct {
    UINT32  Version;
    UINT32  HorizontalResolution;
    UINT32  VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    EFI_PIXEL_BITMASK  PixelInformation;
    UINT32  PixelsPerScanLine;           // May differ from width!
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32                                 MaxMode;
    UINT32                                 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
    UINTN                                  SizeOfInfo;
    EFI_PHYSICAL_ADDRESS                   FrameBufferBase;  // Write here!
    UINTN                                  FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE  QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE    SetMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT         Blt;      // Block transfer
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE       *Mode;     // Current mode info
} EFI_GRAPHICS_OUTPUT_PROTOCOL;
```

### Exercise 1.7: Drawing a Pixel

```c
// Get GOP protocol
EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
BootServices->LocateProtocol(&gop_guid, NULL, (void **)&gop);

// Get framebuffer info
UINT64 fb_base = gop->Mode->FrameBufferBase;
UINT32 width = gop->Mode->Info->HorizontalResolution;
UINT32 height = gop->Mode->Info->VerticalResolution;
UINT32 pitch = gop->Mode->Info->PixelsPerScanLine * 4;  // Bytes per row

// Draw a pixel at (x, y)
uint32_t *framebuffer = (uint32_t *)fb_base;
uint32_t pixels_per_row = pitch / 4;

// Pixel format is typically 0x00RRGGBB (BGR in memory)
framebuffer[y * pixels_per_row + x] = 0x00FF0000;  // Red
```

**Question:** Why is `PixelsPerScanLine` sometimes larger than `HorizontalResolution`?

**Answer:** Memory alignment. GPUs often pad rows to power-of-2 or cache-line boundaries for performance. A 1920-pixel wide display might have 2048 pixels per scanline internally.

---

## Chapter 7: Loading the Kernel

### File System Protocol

To load our kernel, we need to read from disk:

```c
// Open the volume containing our bootloader
EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
BootServices->HandleProtocol(ImageHandle, 
    &EFI_LOADED_IMAGE_PROTOCOL_GUID, (void **)&loaded_image);

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
BootServices->HandleProtocol(loaded_image->DeviceHandle,
    &EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID, (void **)&fs);

// Open root directory
EFI_FILE_PROTOCOL *root;
fs->OpenVolume(fs, &root);

// Open kernel file
EFI_FILE_PROTOCOL *kernel_file;
root->Open(root, &kernel_file, L"\\EFI\\BOOT\\kernel.bin",
    EFI_FILE_MODE_READ, 0);

// Get file size
EFI_FILE_INFO file_info;
UINTN info_size = sizeof(file_info);
kernel_file->GetInfo(kernel_file, &EFI_FILE_INFO_GUID, 
    &info_size, &file_info);
UINT64 kernel_size = file_info.FileSize;

// Allocate memory for kernel at 1MB
EFI_PHYSICAL_ADDRESS kernel_addr = 0x100000;
BootServices->AllocatePages(AllocateAddress, EfiLoaderData,
    (kernel_size + 4095) / 4096, &kernel_addr);

// Read kernel
kernel_file->Read(kernel_file, &kernel_size, (void *)kernel_addr);
```

### Exercise 1.8: The BootInfo Structure

We need to pass information from bootloader to kernel:

```c
// Shared between bootloader and kernel
struct FramebufferInfo {
    uint64_t base;        // Physical address
    uint32_t width;       // Pixels
    uint32_t height;
    uint32_t pitch;       // Bytes per row
    uint32_t bpp;         // Bits per pixel (usually 32)
};

struct MemoryMapEntry {
    uint64_t base;
    uint64_t length;
    uint32_t type;        // 1 = usable, 2 = reserved, etc.
};

struct BootInfo {
    struct FramebufferInfo framebuffer;
    struct MemoryMapEntry  memory_map[256];
    uint32_t               memory_map_count;
    uint64_t               acpi_rsdp;
};
```

**Question:** Why create our own BootInfo instead of passing UEFI structures directly?

**Answer:** After ExitBootServices, UEFI memory might be reclaimed. We copy what we need into our own structure in safe memory. Also, our kernel shouldn't depend on UEFI headers.

---

## Chapter 8: ExitBootServices

### The Point of No Return

```c
// Get final memory map (must be IMMEDIATELY before ExitBootServices)
UINTN map_key;
// ... get memory map, save map_key ...

// EXIT BOOT SERVICES - no more UEFI after this!
EFI_STATUS status = BootServices->ExitBootServices(ImageHandle, map_key);

if (EFI_ERROR(status)) {
    // Memory map changed! Try again.
    // Get new memory map
    // Call ExitBootServices again with new map_key
}

// NOW:
// - Cannot call BootServices (invalid)
// - Cannot call ConOut->OutputString (gone)
// - Cannot allocate memory via UEFI
// - Timer interrupts disabled
// - We own the machine!
```

### Exercise 1.9: Why map_key?

**Question:** Why does ExitBootServices need the map_key?

**Answer:** UEFI needs to verify the memory map hasn't changed since you retrieved it. If you call any boot service after GetMemoryMap (which might allocate memory), the map changes and the key becomes invalid. This ensures you have accurate information about memory before UEFI releases it.

### Jumping to the Kernel

```c
// Kernel entry point type
typedef void (*KernelEntry)(struct BootInfo *);

// Jump to kernel!
KernelEntry kernel_main = (KernelEntry)0x100000;
kernel_main(&boot_info);

// Never returns
while (1);
```

---

## Chapter 9: Your First Kernel

### Kernel Entry Point

```c
// kernel/main.c
#include "bootinfo.h"

// MUST be the first function (linker puts it at 0x100000)
void kernel_main(struct BootInfo *boot_info) {
    // Get framebuffer
    uint32_t *fb = (uint32_t *)boot_info->framebuffer.base;
    uint32_t width = boot_info->framebuffer.width;
    uint32_t height = boot_info->framebuffer.height;
    uint32_t pitch = boot_info->framebuffer.pitch / 4;
    
    // Fill screen with blue
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fb[y * pitch + x] = 0x000000FF;  // Blue
        }
    }
    
    // Halt
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

### Exercise 1.10: The Linker Script

```ld
/* kernel/linker.ld */
ENTRY(kernel_main)

SECTIONS {
    . = 0x100000;           /* Load address = 1MB */
    
    .text : {
        *(.text.kernel_main) /* Entry point first! */
        *(.text*)
    }
    
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
    }
}
```

**Question:** Why use `.text.kernel_main` section?

**Answer:** GCC might not put `kernel_main` first in the `.text` section. By putting it in a special section and listing that first in the linker script, we guarantee it's at exactly 0x100000.

```c
// In kernel code:
__attribute__((section(".text.kernel_main")))
void kernel_main(struct BootInfo *boot_info) {
    // ...
}
```

---

## Chapter 10: Common Pitfalls

### Pitfall 1: The ms_abi Trap

```c
// WRONG - gnu-efi's crt0 calls this with System V ABI
EFI_STATUS EFIAPI efi_main(...) { }

// RIGHT - no EFIAPI on efi_main itself
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) { }

// BUT - UEFI protocol functions DO need EFIAPI
typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(...);
```

### Pitfall 2: Memory Map Changes

```c
// WRONG - allocating after GetMemoryMap
GetMemoryMap(&size, buffer, &key, ...);
AllocatePool(...);  // Changes memory map!
ExitBootServices(ImageHandle, key);  // FAILS - key invalid

// RIGHT - get map immediately before exit
GetMemoryMap(...);
ExitBootServices(...);  // Same key
```

### Pitfall 3: String Encoding

```c
// WRONG - ASCII string
ConOut->OutputString(ConOut, "Hello");

// RIGHT - Wide string (UCS-2)
ConOut->OutputString(ConOut, L"Hello");
```

### Pitfall 4: Framebuffer Pitch

```c
// WRONG - assumes width == pitch
fb[y * width + x] = color;

// RIGHT - use pitch (bytes per row / bytes per pixel)
uint32_t pixels_per_row = pitch / 4;
fb[y * pixels_per_row + x] = color;
```

### Pitfall 5: Kernel Not First

```c
// If helper functions appear before kernel_main in source,
// they might end up at 0x100000 instead!

// Solution 1: Put kernel_main first in source file
// Solution 2: Use section attribute + linker script
```

---

## Summary: Phase 1 Checklist

After completing Phase 1, you should have:

- [ ] Custom UEFI type definitions (types.h)
- [ ] UEFI table structures (tables.h)
- [ ] Protocol definitions (text.h, graphics.h, file.h)
- [ ] Bootloader that:
  - [ ] Prints text to screen (ConOut)
  - [ ] Gets Graphics Output Protocol
  - [ ] Reads kernel from disk
  - [ ] Gets memory map
  - [ ] Exits boot services
  - [ ] Jumps to kernel
- [ ] Kernel that:
  - [ ] Receives BootInfo structure
  - [ ] Draws to framebuffer
  - [ ] Halts without crashing
- [ ] Build system (Makefiles)

---

# Exercises for Phase 1

## Exercise Set A: UEFI Basics

**A1.** Write out the complete path to call `OutputString` starting from `efi_main`'s parameters.

**A2.** Why is the first argument to `OutputString` the protocol pointer itself? (Hint: How does C simulate object-oriented method calls?)

**A3.** If the framebuffer is at physical address 0x80000000 and pitch is 5120 bytes for a 1280-pixel wide display, what is the memory address of pixel (100, 50)?

**A4.** The memory map shows a region of type EfiBootServicesData from 0x7000000 to 0x7FFFFFF. Can your kernel use this memory? When?

## Exercise Set B: Building

**B1.** What is the purpose of `crt0-efi-x86_64.o`? What does it do before calling your `efi_main`?

**B2.** Why do we use `objcopy` to convert from ELF to PE32+ format?

**B3.** Write a Makefile rule that builds a .EFI file from a .c file.

## Exercise Set C: Debugging

**C1.** Your bootloader crashes immediately on QEMU. List 5 things to check.

**C2.** The screen shows garbage colors instead of your expected output. What might be wrong?

**C3.** ExitBootServices keeps failing. What's the likely cause and fix?

---

*Continue to Part 2 for Phase 2: CPU Setup (GDT, IDT, Interrupts)*
