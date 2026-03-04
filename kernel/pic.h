// kernel/pic.h
// 8259 Programmable Interrupt Controller driver
//
// The PIC handles hardware interrupts (IRQs) from devices.
// There are two PICs in a PC: master (IRQ 0-7) and slave (IRQ 8-15).
// The slave is cascaded through IRQ 2 of the master.
//
// By default, the BIOS maps:
//   IRQ 0-7  → INT 0x08-0x0F (conflicts with CPU exceptions!)
//   IRQ 8-15 → INT 0x70-0x77
//
// We remap them to:
//   IRQ 0-7  → INT 0x20-0x27 (32-39)
//   IRQ 8-15 → INT 0x28-0x2F (40-47)
#pragma once

#include <stdint.h>

//=============================================================================
// PIC Ports
//=============================================================================

#define PIC1_COMMAND    0x20    // Master PIC command port
#define PIC1_DATA       0x21    // Master PIC data port
#define PIC2_COMMAND    0xA0    // Slave PIC command port
#define PIC2_DATA       0xA1    // Slave PIC data port

//=============================================================================
// PIC Commands
//=============================================================================

#define PIC_EOI         0x20    // End of Interrupt command

//=============================================================================
// Functions
//=============================================================================

// Initialize and remap the PICs
void pic_init(void);

// Send End of Interrupt signal
// Call this at the end of every IRQ handler
void pic_send_eoi(uint8_t irq);

// Mask (disable) a specific IRQ
void pic_mask_irq(uint8_t irq);

// Unmask (enable) a specific IRQ
void pic_unmask_irq(uint8_t irq);

// Disable all IRQs (mask all)
void pic_disable(void);
