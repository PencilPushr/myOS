// kernel/idt.h
// Interrupt Descriptor Table for x86-64
//
// The IDT holds 256 entries that tell the CPU where to jump when:
// - CPU exceptions occur (divide by zero, page fault, etc.)
// - Hardware interrupts arrive (keyboard, timer, etc.)
// - Software interrupts are triggered (int instruction)
#pragma once

#include <stdint.h>

//=============================================================================
// IDT Entry (16 bytes in 64-bit mode)
//
// When an interrupt occurs, CPU:
// 1. Looks up entry in IDT using interrupt number
// 2. Loads CS:RIP from the entry
// 3. Optionally switches stacks (using IST)
// 4. Pushes return info and error code (if any) onto stack
//=============================================================================

struct IDTEntry {
    uint16_t offset_low;     // Offset bits 0-15
    uint16_t selector;       // Code segment selector (GDT_KERNEL_CODE)
    uint8_t  ist;            // Interrupt Stack Table offset (0 = don't switch)
    uint8_t  type_attr;      // Type and attributes
    uint16_t offset_mid;     // Offset bits 16-31
    uint32_t offset_high;    // Offset bits 32-63
    uint32_t reserved;       // Must be zero
} __attribute__((packed));

//=============================================================================
// IDT Pointer (for LIDT instruction)
//=============================================================================

struct IDTPointer {
    uint16_t limit;          // Size of IDT minus 1
    uint64_t base;           // Linear address of IDT
} __attribute__((packed));

//=============================================================================
// IDT Type/Attribute Flags
//=============================================================================

#define IDT_PRESENT       (1 << 7)    // Entry is valid
#define IDT_DPL_RING0     (0 << 5)    // Ring 0 only
#define IDT_DPL_RING3     (3 << 5)    // Ring 3 can use (for syscalls)

// Gate types
#define IDT_INTERRUPT_GATE  0x0E      // Clears IF (interrupts disabled)
#define IDT_TRAP_GATE       0x0F      // IF unchanged (for syscalls, breakpoints)

//=============================================================================
// Interrupt Numbers
//=============================================================================

// CPU Exceptions (0-31)
#define INT_DIVIDE_ERROR        0     // #DE - Divide by zero
#define INT_DEBUG               1     // #DB - Debug exception
#define INT_NMI                 2     // Non-maskable interrupt
#define INT_BREAKPOINT          3     // #BP - Breakpoint (int 3)
#define INT_OVERFLOW            4     // #OF - Overflow (into instruction)
#define INT_BOUND_RANGE         5     // #BR - BOUND range exceeded
#define INT_INVALID_OPCODE      6     // #UD - Invalid/undefined opcode
#define INT_DEVICE_NOT_AVAIL    7     // #NM - FPU not available
#define INT_DOUBLE_FAULT        8     // #DF - Double fault (with error code)
#define INT_COPROCESSOR         9     // Coprocessor segment overrun (legacy)
#define INT_INVALID_TSS         10    // #TS - Invalid TSS (with error code)
#define INT_SEGMENT_NOT_PRESENT 11    // #NP - Segment not present (with error code)
#define INT_STACK_FAULT         12    // #SS - Stack-segment fault (with error code)
#define INT_GENERAL_PROTECTION  13    // #GP - General protection (with error code)
#define INT_PAGE_FAULT          14    // #PF - Page fault (with error code)
// 15 is reserved
#define INT_X87_FPU             16    // #MF - x87 FPU error
#define INT_ALIGNMENT_CHECK     17    // #AC - Alignment check (with error code)
#define INT_MACHINE_CHECK       18    // #MC - Machine check
#define INT_SIMD_FPU            19    // #XM/#XF - SIMD floating-point
#define INT_VIRTUALIZATION      20    // #VE - Virtualization exception
#define INT_CONTROL_PROTECTION  21    // #CP - Control protection (with error code)
// 22-31 reserved

// Hardware Interrupts (remapped to 32-47 via PIC)
#define INT_IRQ0    32    // PIT timer
#define INT_IRQ1    33    // Keyboard
#define INT_IRQ2    34    // Cascade (used internally)
#define INT_IRQ3    35    // COM2
#define INT_IRQ4    36    // COM1
#define INT_IRQ5    37    // LPT2
#define INT_IRQ6    38    // Floppy
#define INT_IRQ7    39    // LPT1 / spurious
#define INT_IRQ8    40    // CMOS RTC
#define INT_IRQ9    41    // Free
#define INT_IRQ10   42    // Free
#define INT_IRQ11   43    // Free
#define INT_IRQ12   44    // PS/2 Mouse
#define INT_IRQ13   45    // FPU
#define INT_IRQ14   46    // Primary ATA
#define INT_IRQ15   47    // Secondary ATA

// System call (we'll use int 0x80 like Linux for now)
#define INT_SYSCALL 0x80

//=============================================================================
// Interrupt Frame
// This is what the CPU pushes on the stack before calling our handler
//=============================================================================

struct InterruptFrame {
    // Pushed by our stub
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, error_code;
    
    // Pushed by CPU
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

//=============================================================================
// Functions
//=============================================================================

// Initialize IDT and load it
void idt_init(void);

// Register a handler for a specific interrupt
// handler signature: void handler(struct InterruptFrame *frame)
typedef void (*interrupt_handler_t)(struct InterruptFrame *frame);
void idt_set_handler(uint8_t vector, interrupt_handler_t handler);
