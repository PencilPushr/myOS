// kernel/idt.c
// Interrupt Descriptor Table setup for x86-64
#include "idt.h"
#include "gdt.h"
#include "pic.h"

//=============================================================================
// IDT Table (256 entries)
//=============================================================================

static struct IDTEntry idt[256];
static struct IDTPointer idt_ptr;

// Handler table - one function pointer per interrupt
static interrupt_handler_t handlers[256];

//=============================================================================
// Assembly stubs (defined in idt_asm.S)
// These are the actual entry points that save registers before calling C
//=============================================================================

// Each of these pushes the interrupt number and jumps to common handler
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQs (32-47)
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// Load IDT (defined in idt_asm.S)
extern void idt_load(struct IDTPointer *idt_ptr);

//=============================================================================
// Helper: Set an IDT entry
//=============================================================================

static void idt_set_entry(int index, uint64_t handler, uint8_t ist, uint8_t type_attr) {
    idt[index].offset_low = handler & 0xFFFF;
    idt[index].selector = GDT_KERNEL_CODE;
    idt[index].ist = ist;
    idt[index].type_attr = type_attr;
    idt[index].offset_mid = (handler >> 16) & 0xFFFF;
    idt[index].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[index].reserved = 0;
}

//=============================================================================
// Common interrupt handler (called from assembly stubs)
// This is the C entry point for all interrupts
//=============================================================================

void interrupt_handler(struct InterruptFrame *frame) {
    uint64_t int_no = frame->int_no;
    
    // Call registered handler if one exists
    if (handlers[int_no]) {
        handlers[int_no](frame);
    } else {
        // Unhandled interrupt - for now just ignore IRQs, halt on exceptions
        if (int_no < 32) {
            // CPU exception with no handler - this is bad
            // We'll implement proper panic later, for now just halt
            __asm__ volatile("cli; hlt");
        }
    }
    
    // For IRQs (32-47), send End of Interrupt to PIC
    if (int_no >= 32 && int_no < 48) {
        pic_send_eoi(int_no - 32);
    }
}

//=============================================================================
// Register a handler for an interrupt
//=============================================================================

void idt_set_handler(uint8_t vector, interrupt_handler_t handler) {
    handlers[vector] = handler;
}

//=============================================================================
// Initialize IDT
//=============================================================================

void idt_init(void) {
    // Clear handler table
    for (int i = 0; i < 256; i++) {
        handlers[i] = 0;
    }
    
    // CPU Exceptions (0-31)
    // These use interrupt gates (IF cleared = interrupts disabled)
    uint8_t exc_attr = IDT_PRESENT | IDT_DPL_RING0 | IDT_INTERRUPT_GATE;
    
    idt_set_entry(0, (uint64_t)isr0, 0, exc_attr);
    idt_set_entry(1, (uint64_t)isr1, 0, exc_attr);
    idt_set_entry(2, (uint64_t)isr2, 0, exc_attr);
    idt_set_entry(3, (uint64_t)isr3, 0, exc_attr);
    idt_set_entry(4, (uint64_t)isr4, 0, exc_attr);
    idt_set_entry(5, (uint64_t)isr5, 0, exc_attr);
    idt_set_entry(6, (uint64_t)isr6, 0, exc_attr);
    idt_set_entry(7, (uint64_t)isr7, 0, exc_attr);
    idt_set_entry(8, (uint64_t)isr8, 0, exc_attr);   // Double fault - consider using IST
    idt_set_entry(9, (uint64_t)isr9, 0, exc_attr);
    idt_set_entry(10, (uint64_t)isr10, 0, exc_attr);
    idt_set_entry(11, (uint64_t)isr11, 0, exc_attr);
    idt_set_entry(12, (uint64_t)isr12, 0, exc_attr);
    idt_set_entry(13, (uint64_t)isr13, 0, exc_attr);
    idt_set_entry(14, (uint64_t)isr14, 0, exc_attr);
    idt_set_entry(15, (uint64_t)isr15, 0, exc_attr);
    idt_set_entry(16, (uint64_t)isr16, 0, exc_attr);
    idt_set_entry(17, (uint64_t)isr17, 0, exc_attr);
    idt_set_entry(18, (uint64_t)isr18, 0, exc_attr);
    idt_set_entry(19, (uint64_t)isr19, 0, exc_attr);
    idt_set_entry(20, (uint64_t)isr20, 0, exc_attr);
    idt_set_entry(21, (uint64_t)isr21, 0, exc_attr);
    idt_set_entry(22, (uint64_t)isr22, 0, exc_attr);
    idt_set_entry(23, (uint64_t)isr23, 0, exc_attr);
    idt_set_entry(24, (uint64_t)isr24, 0, exc_attr);
    idt_set_entry(25, (uint64_t)isr25, 0, exc_attr);
    idt_set_entry(26, (uint64_t)isr26, 0, exc_attr);
    idt_set_entry(27, (uint64_t)isr27, 0, exc_attr);
    idt_set_entry(28, (uint64_t)isr28, 0, exc_attr);
    idt_set_entry(29, (uint64_t)isr29, 0, exc_attr);
    idt_set_entry(30, (uint64_t)isr30, 0, exc_attr);
    idt_set_entry(31, (uint64_t)isr31, 0, exc_attr);
    
    // Hardware IRQs (32-47)
    uint8_t irq_attr = IDT_PRESENT | IDT_DPL_RING0 | IDT_INTERRUPT_GATE;
    
    idt_set_entry(32, (uint64_t)irq0, 0, irq_attr);
    idt_set_entry(33, (uint64_t)irq1, 0, irq_attr);
    idt_set_entry(34, (uint64_t)irq2, 0, irq_attr);
    idt_set_entry(35, (uint64_t)irq3, 0, irq_attr);
    idt_set_entry(36, (uint64_t)irq4, 0, irq_attr);
    idt_set_entry(37, (uint64_t)irq5, 0, irq_attr);
    idt_set_entry(38, (uint64_t)irq6, 0, irq_attr);
    idt_set_entry(39, (uint64_t)irq7, 0, irq_attr);
    idt_set_entry(40, (uint64_t)irq8, 0, irq_attr);
    idt_set_entry(41, (uint64_t)irq9, 0, irq_attr);
    idt_set_entry(42, (uint64_t)irq10, 0, irq_attr);
    idt_set_entry(43, (uint64_t)irq11, 0, irq_attr);
    idt_set_entry(44, (uint64_t)irq12, 0, irq_attr);
    idt_set_entry(45, (uint64_t)irq13, 0, irq_attr);
    idt_set_entry(46, (uint64_t)irq14, 0, irq_attr);
    idt_set_entry(47, (uint64_t)irq15, 0, irq_attr);
    
    // Load the IDT
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    idt_load(&idt_ptr);
}
