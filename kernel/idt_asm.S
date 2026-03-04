// kernel/idt_asm.S
// Interrupt service routine stubs for x86-64
//
// When an interrupt occurs, the CPU:
// 1. Pushes SS, RSP, RFLAGS, CS, RIP (and error code for some exceptions)
// 2. Loads CS:RIP from IDT entry
// 3. Jumps to our stub
//
// Our stub then:
// 1. Pushes a dummy error code if needed (so stack is uniform)
// 2. Pushes interrupt number
// 3. Saves all registers
// 4. Calls C handler
// 5. Restores registers
// 6. Returns with iretq

.section .text
.code64

//=============================================================================
// Common handler - all stubs jump here after pushing int_no and error_code
//=============================================================================

.extern interrupt_handler

isr_common:
    // Save all general-purpose registers
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    
    // Pass pointer to InterruptFrame as first argument
    mov %rsp, %rdi
    
    // Align stack to 16 bytes (required by System V ABI)
    // RSP might not be aligned after all the pushes
    mov %rsp, %rbp
    and $~0xF, %rsp
    
    // Call C handler
    call interrupt_handler
    
    // Restore stack pointer
    mov %rbp, %rsp
    
    // Restore all general-purpose registers
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    
    // Remove error code and interrupt number from stack
    add $16, %rsp
    
    // Return from interrupt
    iretq

//=============================================================================
// idt_load - Load IDT register
//
// void idt_load(struct IDTPointer *idt_ptr);
//=============================================================================

.global idt_load
.type idt_load, @function
idt_load:
    lidt (%rdi)
    ret
.size idt_load, . - idt_load

//=============================================================================
// CPU Exception Stubs (0-31)
//
// Some exceptions push an error code, others don't.
// We push a dummy 0 for those that don't, so the stack is uniform.
//
// Exceptions WITH error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
// All others: no error code
//=============================================================================

// Macro for exceptions WITHOUT error code
.macro ISR_NOERR num
.global isr\num
.type isr\num, @function
isr\num:
    push $0           // Dummy error code
    push $\num        // Interrupt number
    jmp isr_common
.size isr\num, . - isr\num
.endm

// Macro for exceptions WITH error code (CPU pushes it)
.macro ISR_ERR num
.global isr\num
.type isr\num, @function
isr\num:
    // Error code already pushed by CPU
    push $\num        // Interrupt number
    jmp isr_common
.size isr\num, . - isr\num
.endm

// Generate exception stubs
ISR_NOERR 0     // Divide Error
ISR_NOERR 1     // Debug
ISR_NOERR 2     // NMI
ISR_NOERR 3     // Breakpoint
ISR_NOERR 4     // Overflow
ISR_NOERR 5     // Bound Range Exceeded
ISR_NOERR 6     // Invalid Opcode
ISR_NOERR 7     // Device Not Available
ISR_ERR   8     // Double Fault (error code = 0)
ISR_NOERR 9     // Coprocessor Segment Overrun (legacy)
ISR_ERR   10    // Invalid TSS
ISR_ERR   11    // Segment Not Present
ISR_ERR   12    // Stack-Segment Fault
ISR_ERR   13    // General Protection Fault
ISR_ERR   14    // Page Fault
ISR_NOERR 15    // Reserved
ISR_NOERR 16    // x87 FPU Error
ISR_ERR   17    // Alignment Check
ISR_NOERR 18    // Machine Check
ISR_NOERR 19    // SIMD Floating-Point
ISR_NOERR 20    // Virtualization Exception
ISR_ERR   21    // Control Protection Exception
ISR_NOERR 22    // Reserved
ISR_NOERR 23    // Reserved
ISR_NOERR 24    // Reserved
ISR_NOERR 25    // Reserved
ISR_NOERR 26    // Reserved
ISR_NOERR 27    // Reserved
ISR_NOERR 28    // Reserved
ISR_ERR   29    // VMM Communication Exception
ISR_ERR   30    // Security Exception
ISR_NOERR 31    // Reserved

//=============================================================================
// IRQ Stubs (remapped to 32-47)
//
// Hardware interrupts never have error codes
//=============================================================================

.macro IRQ num, int_num
.global irq\num
.type irq\num, @function
irq\num:
    push $0           // Dummy error code
    push $\int_num    // Interrupt number (32 + IRQ number)
    jmp isr_common
.size irq\num, . - irq\num
.endm

// Generate IRQ stubs (IRQ 0-15 mapped to INT 32-47)
IRQ 0, 32       // Timer
IRQ 1, 33       // Keyboard
IRQ 2, 34       // Cascade
IRQ 3, 35       // COM2
IRQ 4, 36       // COM1
IRQ 5, 37       // LPT2
IRQ 6, 38       // Floppy
IRQ 7, 39       // LPT1 / Spurious
IRQ 8, 40       // CMOS RTC
IRQ 9, 41       // Free
IRQ 10, 42      // Free
IRQ 11, 43      // Free
IRQ 12, 44      // PS/2 Mouse
IRQ 13, 45      // FPU
IRQ 14, 46      // Primary ATA
IRQ 15, 47      // Secondary ATA
