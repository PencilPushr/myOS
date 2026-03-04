// kernel/gdt.h
// Global Descriptor Table for x86-64
//
// In 64-bit long mode, segmentation is mostly disabled - the CPU ignores
// base and limit for code/data segments. However, we still need a GDT for:
// 1. Defining kernel vs user privilege levels (ring 0 vs ring 3)
// 2. The TSS (Task State Segment) for interrupt stack switching
// 3. Setting the L (long mode) bit for code segments
#pragma once

#include <stdint.h>

//=============================================================================
// GDT Entry (8 bytes)
//
// In 64-bit mode, most fields are ignored. Only these matter:
// - Present bit (must be 1)
// - DPL (Descriptor Privilege Level: 0=kernel, 3=user)
// - Type (code vs data, readable/writable)
// - L bit (1=64-bit code segment)
//=============================================================================

struct GDTEntry {
    uint16_t limit_low;      // Ignored in 64-bit mode
    uint16_t base_low;       // Ignored in 64-bit mode
    uint8_t  base_mid;       // Ignored in 64-bit mode
    uint8_t  access;         // Access byte (type, DPL, present)
    uint8_t  flags_limit;    // Flags (4 bits) + limit high (4 bits)
    uint8_t  base_high;      // Ignored in 64-bit mode
} __attribute__((packed));

//=============================================================================
// TSS Entry (16 bytes - spans two GDT slots)
//
// The TSS descriptor is 16 bytes in 64-bit mode because the base address
// is 64 bits. It takes up two consecutive GDT entries.
//=============================================================================

struct TSSEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

//=============================================================================
// Task State Segment (TSS)
//
// In 64-bit mode, the TSS is used primarily for:
// - RSP0: Stack pointer to use when transitioning to ring 0
// - IST1-7: Interrupt Stack Table pointers for specific interrupts
// - I/O Permission Bitmap base
//=============================================================================

struct TSS {
    uint32_t reserved0;
    uint64_t rsp0;           // Stack pointer for ring 0
    uint64_t rsp1;           // Stack pointer for ring 1 (unused)
    uint64_t rsp2;           // Stack pointer for ring 2 (unused)
    uint64_t reserved1;
    uint64_t ist1;           // Interrupt Stack Table entry 1
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;    // I/O Permission Bitmap offset
} __attribute__((packed));

//=============================================================================
// GDT Pointer (for LGDT instruction)
//=============================================================================

struct GDTPointer {
    uint16_t limit;          // Size of GDT minus 1
    uint64_t base;           // Linear address of GDT
} __attribute__((packed));

//=============================================================================
// Segment Selectors
//
// A segment selector is a 16-bit value:
//   Bits 0-1: RPL (Requested Privilege Level)
//   Bit 2: TI (Table Indicator: 0=GDT, 1=LDT)
//   Bits 3-15: Index into descriptor table
//
// So selector = (index << 3) | RPL for GDT entries
//=============================================================================

#define GDT_KERNEL_CODE  0x08    // Index 1, RPL 0
#define GDT_KERNEL_DATA  0x10    // Index 2, RPL 0
#define GDT_USER_CODE    0x1B    // Index 3, RPL 3 (0x18 | 3)
#define GDT_USER_DATA    0x23    // Index 4, RPL 3 (0x20 | 3)
#define GDT_TSS          0x28    // Index 5, RPL 0

//=============================================================================
// Access Byte Flags
//=============================================================================

#define GDT_ACCESS_PRESENT     (1 << 7)  // Segment is present
#define GDT_ACCESS_RING0       (0 << 5)  // Ring 0 (kernel)
#define GDT_ACCESS_RING3       (3 << 5)  // Ring 3 (user)
#define GDT_ACCESS_DESCRIPTOR  (1 << 4)  // Code/data (not system)
#define GDT_ACCESS_EXECUTABLE  (1 << 3)  // Code segment
#define GDT_ACCESS_RW          (1 << 1)  // Readable (code) / Writable (data)
#define GDT_ACCESS_ACCESSED    (1 << 0)  // CPU sets this on access

// TSS access (system segment)
#define GDT_ACCESS_TSS         0x89      // Present, 64-bit TSS (available)

//=============================================================================
// Flags (upper 4 bits of flags_limit byte)
//=============================================================================

#define GDT_FLAG_GRANULARITY   (1 << 7)  // Limit in 4KB units (ignored in 64-bit)
#define GDT_FLAG_SIZE          (1 << 6)  // 32-bit segment (0 for 64-bit code)
#define GDT_FLAG_LONG          (1 << 5)  // 64-bit code segment

//=============================================================================
// Functions
//=============================================================================

// Initialize GDT and TSS, then load them
void gdt_init(void);

// Set the kernel stack pointer in TSS (called when switching tasks)
void gdt_set_kernel_stack(uint64_t stack);
