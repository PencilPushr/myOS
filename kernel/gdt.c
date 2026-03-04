// kernel/gdt.c
// Global Descriptor Table setup for x86-64
#include "gdt.h"

//=============================================================================
// GDT Layout
//
// Index 0: Null descriptor (required, CPU ignores)
// Index 1: Kernel code segment (ring 0, 64-bit)
// Index 2: Kernel data segment (ring 0)
// Index 3: User code segment (ring 3, 64-bit)
// Index 4: User data segment (ring 3)
// Index 5-6: TSS (takes 2 entries in 64-bit mode)
//=============================================================================

// We need 7 entries but TSS takes 2 slots, so 6 "logical" entries
static struct GDTEntry gdt[7];
static struct TSS tss;
static struct GDTPointer gdt_ptr;

//=============================================================================
// Assembly functions (defined in gdt_asm.S)
//=============================================================================

// Load GDTR and reload segment registers
extern void gdt_load(struct GDTPointer *gdt_ptr);

// Load Task Register with TSS selector
extern void tss_load(uint16_t selector);

//=============================================================================
// Helper: Create a GDT entry
//=============================================================================

static void gdt_set_entry(int index, uint8_t access, uint8_t flags) {
    // In 64-bit mode, base and limit are ignored for code/data segments
    // We set them to 0 for cleanliness
    gdt[index].limit_low = 0;
    gdt[index].base_low = 0;
    gdt[index].base_mid = 0;
    gdt[index].access = access;
    gdt[index].flags_limit = flags;  // High 4 bits are flags, low 4 bits are limit high
    gdt[index].base_high = 0;
}

//=============================================================================
// Helper: Create the TSS entry (16 bytes, spans indices 5-6)
//=============================================================================

static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    struct TSSEntry *tss_entry = (struct TSSEntry *)&gdt[index];
    
    tss_entry->limit_low = limit & 0xFFFF;
    tss_entry->base_low = base & 0xFFFF;
    tss_entry->base_mid = (base >> 16) & 0xFF;
    tss_entry->access = GDT_ACCESS_TSS;
    tss_entry->flags_limit = ((limit >> 16) & 0x0F);  // No granularity flag
    tss_entry->base_mid2 = (base >> 24) & 0xFF;
    tss_entry->base_high = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;
}

//=============================================================================
// Initialize GDT
//=============================================================================

void gdt_init(void) {
    // Entry 0: Null descriptor (required)
    gdt_set_entry(0, 0, 0);
    
    // Entry 1: Kernel code segment
    // Access: Present, Ring 0, Code/Data, Executable, Readable
    // Flags: Long mode (64-bit)
    gdt_set_entry(1,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR |
        GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_LONG
    );
    
    // Entry 2: Kernel data segment
    // Access: Present, Ring 0, Code/Data, Writable
    // Flags: None needed for data segments in 64-bit mode
    gdt_set_entry(2,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR |
        GDT_ACCESS_RW,
        0
    );
    
    // Entry 3: User code segment
    // Access: Present, Ring 3, Code/Data, Executable, Readable
    // Flags: Long mode (64-bit)
    gdt_set_entry(3,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR |
        GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
        GDT_FLAG_LONG
    );
    
    // Entry 4: User data segment
    // Access: Present, Ring 3, Code/Data, Writable
    // Flags: None
    gdt_set_entry(4,
        GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DESCRIPTOR |
        GDT_ACCESS_RW,
        0
    );
    
    // Initialize TSS
    // Clear the TSS structure
    uint8_t *tss_bytes = (uint8_t *)&tss;
    for (uint32_t i = 0; i < sizeof(struct TSS); i++) {
        tss_bytes[i] = 0;
    }
    
    // Set I/O permission bitmap offset to be past the TSS
    // This effectively disables the I/O permission bitmap
    tss.iopb_offset = sizeof(struct TSS);
    
    // Entry 5-6: TSS descriptor (16 bytes)
    gdt_set_tss(5, (uint64_t)&tss, sizeof(struct TSS) - 1);
    
    // Set up the GDT pointer
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    // Load the GDT
    gdt_load(&gdt_ptr);
    
    // Load the TSS
    tss_load(GDT_TSS);
}

//=============================================================================
// Set kernel stack in TSS
//
// This is called when switching to a user process - RSP0 defines
// what stack to use when an interrupt occurs in user mode
//=============================================================================

void gdt_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}
