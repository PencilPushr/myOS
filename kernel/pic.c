// kernel/pic.c
// 8259 Programmable Interrupt Controller driver
#include "pic.h"

//=============================================================================
// Port I/O Helper Functions
//=============================================================================

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    // Write to an unused port to add a small delay
    // Some old hardware needs time between I/O operations
    outb(0x80, 0);
}

//=============================================================================
// ICW (Initialization Command Words) for 8259 PIC
//=============================================================================

#define ICW1_INIT       0x10    // Initialization flag
#define ICW1_ICW4       0x01    // ICW4 will be sent
#define ICW4_8086       0x01    // 8086/88 mode

//=============================================================================
// Initialize and Remap PIC
//=============================================================================

void pic_init(void) {
    uint8_t mask1, mask2;
    
    // Save current masks
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set vector offsets
    outb(PIC1_DATA, 0x20);     // Master: IRQ 0-7 → INT 0x20-0x27 (32-39)
    io_wait();
    outb(PIC2_DATA, 0x28);     // Slave: IRQ 8-15 → INT 0x28-0x2F (40-47)
    io_wait();
    
    // ICW3: Configure cascade
    outb(PIC1_DATA, 0x04);     // Master: slave on IRQ2 (bit 2)
    io_wait();
    outb(PIC2_DATA, 0x02);     // Slave: cascade identity = 2
    io_wait();
    
    // ICW4: Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Restore saved masks (or mask all initially)
    // For now, mask all IRQs except cascade
    outb(PIC1_DATA, 0xFB);     // Mask all except IRQ2 (cascade)
    outb(PIC2_DATA, 0xFF);     // Mask all on slave
}

//=============================================================================
// Send End of Interrupt
//=============================================================================

void pic_send_eoi(uint8_t irq) {
    // If IRQ came from slave PIC (8-15), send EOI to both
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Always send EOI to master
    outb(PIC1_COMMAND, PIC_EOI);
}

//=============================================================================
// Mask (disable) an IRQ
//=============================================================================

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

//=============================================================================
// Unmask (enable) an IRQ
//=============================================================================

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

//=============================================================================
// Disable all IRQs
//=============================================================================

void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
