// kernel/gdt_asm.S
// Assembly routines for GDT and TSS loading
//
// We need assembly because:
// 1. LGDT instruction loads the GDT register
// 2. Reloading CS requires a far return (can't MOV to CS)
// 3. LTR loads the Task Register

.section .text
.code64

//=============================================================================
// gdt_load - Load GDT and reload segment registers
//
// void gdt_load(struct GDTPointer *gdt_ptr);
//
// Input: RDI = pointer to GDT descriptor (limit + base)
//=============================================================================

.global gdt_load
.type gdt_load, @function
gdt_load:
    // Load the GDT register
    lgdt (%rdi)
    
    // Reload data segment registers with kernel data selector (0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    // Reload CS using a far return
    // We push the new CS and the return address, then do a far return
    //
    // Stack before: [return address]
    // We want:      [return address][new CS] <- then RETFQ pops both
    
    pop %rdi                    // Pop return address into RDI
    push $0x08                  // Push kernel code selector
    push %rdi                   // Push return address
    retfq                       // Far return: pops RIP and CS

.size gdt_load, . - gdt_load

//=============================================================================
// tss_load - Load Task Register
//
// void tss_load(uint16_t selector);
//
// Input: DI = TSS selector (0x28)
//=============================================================================

.global tss_load
.type tss_load, @function
tss_load:
    ltr %di
    ret

.size tss_load, . - tss_load
