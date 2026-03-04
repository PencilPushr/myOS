// kernel/exceptions.c
// CPU Exception Handlers
//
// When a CPU exception occurs, these handlers display diagnostic info
// and halt the system. In a real OS, some exceptions (like page fault)
// would be handled and execution would continue.

#include "exceptions.h"
#include "idt.h"
#include "../common/bootinfo.h"

// External framebuffer pointer (set in main.c)
extern struct FramebufferInfo *g_fb;

// External draw function (defined in main.c)
extern void draw_rect(struct FramebufferInfo *fb,
                      uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h,
                      uint32_t color);

//=============================================================================
// Exception Names
//=============================================================================

const char *exception_names[32] = {
    "Divide Error",                 // 0
    "Debug",                        // 1
    "NMI",                          // 2
    "Breakpoint",                   // 3
    "Overflow",                     // 4
    "Bound Range Exceeded",         // 5
    "Invalid Opcode",               // 6
    "Device Not Available",         // 7
    "Double Fault",                 // 8
    "Coprocessor Segment",          // 9
    "Invalid TSS",                  // 10
    "Segment Not Present",          // 11
    "Stack-Segment Fault",          // 12
    "General Protection Fault",     // 13
    "Page Fault",                   // 14
    "Reserved",                     // 15
    "x87 FPU Error",                // 16
    "Alignment Check",              // 17
    "Machine Check",                // 18
    "SIMD Exception",               // 19
    "Virtualization Exception",     // 20
    "Control Protection",           // 21
    "Reserved",                     // 22
    "Reserved",                     // 23
    "Reserved",                     // 24
    "Reserved",                     // 25
    "Reserved",                     // 26
    "Reserved",                     // 27
    "Reserved",                     // 28
    "Reserved",                     // 29
    "Security Exception",           // 30
    "Reserved"                      // 31
};

//=============================================================================
// Helper: Draw a hex digit as a colored block
//=============================================================================

static void draw_hex_digit(uint32_t x, uint32_t y, uint8_t digit) {
    // Color based on digit value (0-15)
    // Use a gradient from dark to bright
    uint32_t colors[16] = {
        0x00000000, 0x00110011, 0x00220022, 0x00330033,
        0x00440044, 0x00550055, 0x00660066, 0x00770077,
        0x00880088, 0x00990099, 0x00AA00AA, 0x00BB00BB,
        0x00CC00CC, 0x00DD00DD, 0x00EE00EE, 0x00FF00FF
    };
    
    if (g_fb) {
        draw_rect(g_fb, x, y, 8, 16, colors[digit & 0xF]);
    }
}

//=============================================================================
// Helper: Draw a 64-bit value as colored blocks
//=============================================================================

static void draw_hex_value(uint32_t x, uint32_t y, uint64_t value) {
    for (int i = 15; i >= 0; i--) {
        uint8_t digit = (value >> (i * 4)) & 0xF;
        draw_hex_digit(x + (15 - i) * 9, y, digit);
    }
}

//=============================================================================
// Helper: Draw exception number as pattern
//=============================================================================

static void draw_exception_indicator(uint32_t exc_num) {
    if (!g_fb) return;
    
    // Red background for panic
    uint32_t screen_w = g_fb->width;
    uint32_t screen_h = g_fb->height;
    
    // Draw red border to indicate panic
    draw_rect(g_fb, 0, 0, screen_w, 10, 0x00FF0000);           // Top
    draw_rect(g_fb, 0, screen_h - 10, screen_w, 10, 0x00FF0000); // Bottom
    draw_rect(g_fb, 0, 0, 10, screen_h, 0x00FF0000);           // Left
    draw_rect(g_fb, screen_w - 10, 0, 10, screen_h, 0x00FF0000); // Right
    
    // Draw exception number as binary pattern (5 bits)
    uint32_t box_y = 50;
    uint32_t box_size = 30;
    
    for (int i = 4; i >= 0; i--) {
        uint32_t color = (exc_num & (1 << i)) ? 0x00FFFFFF : 0x00404040;
        draw_rect(g_fb, 100 + (4 - i) * 35, box_y, box_size, box_size, color);
    }
}

//=============================================================================
// Generic Exception Handler
//=============================================================================

static void exception_handler(struct InterruptFrame *frame) {
    // Disable interrupts
    __asm__ volatile("cli");
    
    uint64_t exc_num = frame->int_no;
    
    // Draw panic indicator
    draw_exception_indicator(exc_num);
    
    if (g_fb) {
        uint32_t y = 100;
        uint32_t x = 100;
        
        // Draw register values as colored bars
        // RIP (instruction pointer) - most important
        draw_rect(g_fb, x, y, 150, 16, 0x00FFFF00);  // Yellow label
        draw_hex_value(x + 160, y, frame->rip);
        y += 25;
        
        // Error code
        draw_rect(g_fb, x, y, 150, 16, 0x00FF8800);  // Orange label
        draw_hex_value(x + 160, y, frame->error_code);
        y += 25;
        
        // RSP
        draw_rect(g_fb, x, y, 150, 16, 0x0000FFFF);  // Cyan label
        draw_hex_value(x + 160, y, frame->rsp);
        y += 25;
        
        // RAX
        draw_rect(g_fb, x, y, 150, 16, 0x0000FF00);  // Green label
        draw_hex_value(x + 160, y, frame->rax);
        y += 25;
        
        // RBX
        draw_rect(g_fb, x, y, 150, 16, 0x000088FF);  // Light blue label
        draw_hex_value(x + 160, y, frame->rbx);
        y += 25;
        
        // CR2 (for page faults - faulting address)
        if (exc_num == 14) {
            uint64_t cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            draw_rect(g_fb, x, y, 150, 16, 0x00FF00FF);  // Magenta label
            draw_hex_value(x + 160, y, cr2);
        }
    }
    
    // Halt forever
    while (1) {
        __asm__ volatile("hlt");
    }
}

//=============================================================================
// Specific Exception Handlers
// Some exceptions need special handling
//=============================================================================

static void divide_error_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void debug_handler(struct InterruptFrame *frame) {
    // Debug exceptions can sometimes be continued
    // For now, treat as fatal
    exception_handler(frame);
}

static void nmi_handler(struct InterruptFrame *frame) {
    // NMI is often hardware failure
    exception_handler(frame);
}

static void breakpoint_handler(struct InterruptFrame *frame) {
    // Breakpoint (int 3) - could be used for debugging
    // For now, treat as fatal
    exception_handler(frame);
}

static void overflow_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void bound_range_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void invalid_opcode_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void device_not_available_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void double_fault_handler(struct InterruptFrame *frame) {
    // Double fault is very bad - usually means stack overflow
    // or corrupted IDT
    exception_handler(frame);
}

static void invalid_tss_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void segment_not_present_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void stack_fault_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void general_protection_handler(struct InterruptFrame *frame) {
    // GPF is common - often means bad memory access or privilege violation
    exception_handler(frame);
}

static void page_fault_handler(struct InterruptFrame *frame) {
    // Page fault - could be handled for demand paging
    // For now, treat as fatal
    // CR2 contains the faulting address
    // Error code bits:
    //   Bit 0: Present (0=not present, 1=protection violation)
    //   Bit 1: Write (0=read, 1=write)
    //   Bit 2: User (0=supervisor, 1=user mode)
    //   Bit 3: Reserved bit set
    //   Bit 4: Instruction fetch
    exception_handler(frame);
}

static void x87_fpu_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void alignment_check_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

static void machine_check_handler(struct InterruptFrame *frame) {
    // Machine check - hardware failure
    exception_handler(frame);
}

static void simd_handler(struct InterruptFrame *frame) {
    exception_handler(frame);
}

//=============================================================================
// Initialize Exception Handlers
//=============================================================================

void exceptions_init(void) {
    idt_set_handler(0, divide_error_handler);
    idt_set_handler(1, debug_handler);
    idt_set_handler(2, nmi_handler);
    idt_set_handler(3, breakpoint_handler);
    idt_set_handler(4, overflow_handler);
    idt_set_handler(5, bound_range_handler);
    idt_set_handler(6, invalid_opcode_handler);
    idt_set_handler(7, device_not_available_handler);
    idt_set_handler(8, double_fault_handler);
    // 9 is legacy (coprocessor segment overrun)
    idt_set_handler(10, invalid_tss_handler);
    idt_set_handler(11, segment_not_present_handler);
    idt_set_handler(12, stack_fault_handler);
    idt_set_handler(13, general_protection_handler);
    idt_set_handler(14, page_fault_handler);
    // 15 is reserved
    idt_set_handler(16, x87_fpu_handler);
    idt_set_handler(17, alignment_check_handler);
    idt_set_handler(18, machine_check_handler);
    idt_set_handler(19, simd_handler);
    // 20-31 are reserved/virtualization
}
