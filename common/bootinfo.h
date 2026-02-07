// common/bootinfo.h
// Boot information structure passed from bootloader to kernel
//
// This is the clean interface between bootloader and kernel.
// The kernel doesn't need to know if it was loaded by UEFI or legacy BIOS -
// it just receives this structure with all the information it needs.
#pragma once

#include <stdint.h>

//=============================================================================
// Memory Types (our own classification)
//=============================================================================

#define MEMORY_TYPE_USABLE    1  // Free memory, kernel can use
#define MEMORY_TYPE_RESERVED  2  // Reserved, don't touch
#define MEMORY_TYPE_ACPI      3  // ACPI tables, reclaimable after parsing
#define MEMORY_TYPE_MMIO      4  // Memory-mapped I/O

//=============================================================================
// Framebuffer Information
//=============================================================================

struct FramebufferInfo {
    uint64_t base;      // Physical address of framebuffer
    uint32_t width;     // Width in pixels
    uint32_t height;    // Height in pixels
    uint32_t pitch;     // Bytes per row (may be > width * bpp/8 due to padding)
    uint8_t  bpp;       // Bits per pixel (typically 32)
    uint8_t  _pad[3];
};

//=============================================================================
// Memory Map Entry
//=============================================================================

struct MemoryMapEntry {
    uint64_t base;      // Physical base address
    uint64_t length;    // Length in bytes
    uint32_t type;      // MEMORY_TYPE_*
    uint32_t _pad;
};

//=============================================================================
// Boot Information
// This is what the bootloader passes to the kernel
//=============================================================================

#define MAX_MEMORY_MAP_ENTRIES 256

struct BootInfo {
    // Graphics
    struct FramebufferInfo framebuffer;
    
    // Memory map
    struct MemoryMapEntry  memory_map[MAX_MEMORY_MAP_ENTRIES];
    uint32_t               memory_map_count;
    
    // ACPI (for finding hardware info later)
    void                  *rsdp;  // ACPI RSDP pointer
};
