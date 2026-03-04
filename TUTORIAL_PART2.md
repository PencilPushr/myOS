# MyOS Tutorial Part 2: CPU Setup

## Phase 2: GDT, IDT, Interrupts, and Keyboard

Now that we can boot and display graphics, we need to set up the CPU properly. This phase transforms your kernel from a simple program into something that can respond to hardware events.

---

# Chapter 11: The Global Descriptor Table (GDT)

## Why Do We Need a GDT?

In x86, memory access goes through "segmentation" - an old memory protection mechanism. Even in 64-bit mode (where segmentation is mostly disabled), we still need a GDT because:

1. **Privilege Levels** - Define ring 0 (kernel) vs ring 3 (user)
2. **The TSS** - Required for interrupt stack switching
3. **Long Mode** - Code segments need the L bit set for 64-bit

### Historical Context

```
8086 (1978) - Real Mode:
  16-bit, 1MB limit
  Segments: CS, DS, ES, SS (16-bit)
  Address = Segment * 16 + Offset
  No protection!

80286 (1982) - Protected Mode:
  24-bit addresses (16MB)
  Segments become selectors → descriptors
  Descriptors contain base, limit, access rights
  
80386 (1985) - Paging Added:
  32-bit addresses (4GB)
  Segments still there, but paging does real protection
  
x86-64 (2003) - Long Mode:
  64-bit addresses
  Segmentation mostly disabled!
  But GDT still required for privilege levels
```

### Exercise 11.1: What Segments Do We Need?

```
64-bit GDT layout:

Index 0: Null descriptor     (required by CPU)
Index 1: Kernel Code (ring 0, 64-bit)
Index 2: Kernel Data (ring 0)
Index 3: User Code (ring 3, 64-bit)
Index 4: User Data (ring 3)
Index 5-6: TSS (takes 2 slots in 64-bit mode)

Segment Selectors (loaded into CS, DS, etc.):
  Format: [Index (13 bits)][TI (1 bit)][RPL (2 bits)]
  
  Kernel Code = 0x08 = 0b0000000000001_0_00 = Index 1, GDT, Ring 0
  Kernel Data = 0x10 = 0b0000000000010_0_00 = Index 2, GDT, Ring 0
  User Code   = 0x1B = 0b0000000000011_0_11 = Index 3, GDT, Ring 3
  User Data   = 0x23 = 0b0000000000100_0_11 = Index 4, GDT, Ring 3
  TSS         = 0x28 = 0b0000000000101_0_00 = Index 5, GDT, Ring 0
```

**Question:** Why does the user code selector (0x1B) have RPL=3 but the index is still 3?

**Answer:** The index (bits 3-15) is 3, which selects GDT entry 3 (user code segment). The RPL (bits 0-1) is also 3, indicating the requestor's privilege level. When accessing through this selector, the CPU checks that the segment's DPL (Descriptor Privilege Level) is >= RPL. For user code: DPL=3, RPL=3, so access is allowed.

---

## GDT Entry Structure

### 8-Byte Entry (Normal Segments)

```
Bits:    63         56 55   52 51   48 47         40 39        32
        ┌────────────┬──────┬──────────┬────────────┬───────────┐
        │ Base 31:24 │Flags │Limit19:16│   Access   │ Base 23:16│
        ├────────────┴──────┴──────────┴────────────┴───────────┤
        │           Base 15:0          │       Limit 15:0       │
        └──────────────────────────────┴────────────────────────┘
Bits:    31                           16 15                     0

In 64-bit mode:
  - Base is ignored (treated as 0) for code/data segments
  - Limit is ignored (all of memory accessible)
  - Only Access and Flags matter!
```

### Access Byte

```
Bit 7: Present (P)        - Must be 1 for valid segment
Bit 6-5: DPL              - Ring level (0-3)
Bit 4: Descriptor Type    - 1 = Code/Data, 0 = System (TSS, etc.)
Bit 3: Executable         - 1 = Code segment, 0 = Data segment
Bit 2: Direction/Conform  - Usually 0
Bit 1: Read/Write         - Readable code / Writable data
Bit 0: Accessed           - CPU sets this when segment used

Examples:
  Kernel Code: 0x9A = 1001 1010
               P=1, DPL=0, S=1, E=1, DC=0, RW=1, A=0
               
  Kernel Data: 0x92 = 1001 0010
               P=1, DPL=0, S=1, E=0, DC=0, RW=1, A=0
               
  User Code:   0xFA = 1111 1010
               P=1, DPL=3, S=1, E=1, DC=0, RW=1, A=0
```

### Flags Nibble

```
Bit 3: Granularity (G)    - 0 = Byte, 1 = 4KB pages (ignored in 64-bit)
Bit 2: Size (D/B)         - 0 = 16-bit, 1 = 32-bit (0 for 64-bit code!)
Bit 1: Long Mode (L)      - 1 = 64-bit code segment
Bit 0: Available          - Ignored

For 64-bit code: 0x2 = 0010 (L=1, D=0)
For data:        0x0 = 0000
```

### Exercise 11.2: Build a GDT Entry

Write a function to create a GDT entry:

```c
struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;  // Flags in high 4 bits, limit in low 4
    uint8_t  base_high;
} __attribute__((packed));

// Fill in for 64-bit kernel code segment:
void make_kernel_code_entry(struct GDTEntry *entry) {
    // In 64-bit mode, base and limit are ignored, but set to 0
    entry->limit_low = 0;
    entry->base_low = 0;
    entry->base_mid = 0;
    entry->base_high = 0;
    
    // Access: Present, Ring 0, Code/Data, Executable, Readable
    entry->access = 0x9A;
    
    // Flags: Long mode
    entry->flags_limit_high = 0x20;  // L=1 in high nibble
}
```

---

## The Task State Segment (TSS)

### Why TSS?

In 64-bit mode, the TSS has one crucial job: **define the stack to use when interrupts occur**.

```
When interrupt occurs in user mode (ring 3):
  1. CPU looks up TSS
  2. Loads RSP from TSS.RSP0 (ring 0 stack)
  3. Pushes SS, RSP, RFLAGS, CS, RIP
  4. Jumps to interrupt handler
  
Without TSS:
  CPU doesn't know what stack to use → triple fault!
```

### TSS Structure (64-bit)

```c
struct TSS {
    uint32_t reserved0;
    uint64_t rsp0;        // Stack for ring 0 (from ring 3)
    uint64_t rsp1;        // Stack for ring 1 (unused)
    uint64_t rsp2;        // Stack for ring 2 (unused)
    uint64_t reserved1;
    uint64_t ist1;        // Interrupt Stack Table 1 (for NMI)
    uint64_t ist2;        // IST 2 (for double fault)
    uint64_t ist3;        // IST 3
    uint64_t ist4;        // IST 4
    uint64_t ist5;        // IST 5
    uint64_t ist6;        // IST 6
    uint64_t ist7;        // IST 7
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset; // I/O Permission Bitmap
} __attribute__((packed));
```

### TSS Descriptor (16 bytes!)

In 64-bit mode, the TSS descriptor is 16 bytes (takes 2 GDT slots):

```
Bytes 0-7:  Like normal descriptor, but type = 0x89 (64-bit TSS Available)
Bytes 8-15: Upper 32 bits of base address + reserved

struct TSSDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  type;         // 0x89 = Present, 64-bit TSS
    uint8_t  flags_limit;
    uint8_t  base_mid2;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));
```

### Exercise 11.3: Initialize TSS

```c
static struct TSS tss;
static uint8_t kernel_stack[8192];  // 8KB kernel stack

void tss_init(void) {
    // Clear TSS
    memset(&tss, 0, sizeof(tss));
    
    // Set stack for ring 3 → ring 0 transitions
    // Stack grows down, so point to TOP of array
    tss.rsp0 = (uint64_t)&kernel_stack[8192];
    
    // I/O permission bitmap - disable by setting offset past TSS
    tss.iopb_offset = sizeof(struct TSS);
}
```

---

## Loading the GDT

### The GDTR Register

```
GDTR (GDT Register):
┌─────────────────────────────────────────┐
│      Base (64 bits)     │ Limit (16)   │
└─────────────────────────────────────────┘

Limit = Size of GDT - 1
Base = Linear address of GDT
```

### The LGDT Instruction

```asm
// Assembly to load GDT
lgdt (%rdi)    // Load GDTR from memory pointed to by RDI
```

### Reloading Segment Registers

After loading a new GDT, you must reload segment registers:

```asm
gdt_load:
    lgdt (%rdi)         // Load GDT
    
    // Reload data segments
    mov $0x10, %ax      // Kernel data selector
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    // Reload CS via far return
    // Can't MOV to CS - must use far jump or far return
    pop %rdi            // Get return address
    push $0x08          // Push kernel code selector
    push %rdi           // Push return address
    retfq               // Far return: pops RIP and CS
```

### Exercise 11.4: Why Far Return?

**Question:** Why can't we just `mov $0x08, %cs`?

**Answer:** The CPU doesn't allow direct modification of CS. The only ways to change CS are:
- Far CALL/JMP (jmp far ptr16:64)
- Far RET (retf/retfq)
- IRET (interrupt return)
- SYSRET/SYSEXIT

We use far return because it's simple: push the new CS and RIP, then retfq pops both.

---

# Chapter 12: The Interrupt Descriptor Table (IDT)

## Interrupts and Exceptions

```
┌─────────────────────────────────────────────────────────────────┐
│                    TYPES OF INTERRUPTS                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  EXCEPTIONS (Synchronous - caused by CPU)                       │
│  ├── Faults: Can be corrected, restart instruction             │
│  │   └── Page Fault, General Protection Fault                  │
│  ├── Traps: Reported after instruction completes               │
│  │   └── Breakpoint (INT 3), Overflow                          │
│  └── Aborts: Unrecoverable                                     │
│      └── Double Fault, Machine Check                           │
│                                                                 │
│  HARDWARE INTERRUPTS (Asynchronous - from devices)             │
│  ├── Timer tick                                                │
│  ├── Keyboard press                                            │
│  └── Disk completion, Network packet, etc.                     │
│                                                                 │
│  SOFTWARE INTERRUPTS (Intentional)                              │
│  └── INT instruction (used for system calls)                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Exercise 12.1: Exception Numbers

Memorize these important exception numbers:

| Vector | Name | Error Code? | Description |
|--------|------|-------------|-------------|
| 0 | #DE | No | Divide Error (div by zero) |
| 1 | #DB | No | Debug |
| 3 | #BP | No | Breakpoint (INT 3) |
| 6 | #UD | No | Invalid Opcode |
| 8 | #DF | Yes (0) | Double Fault |
| 13 | #GP | Yes | General Protection Fault |
| 14 | #PF | Yes | Page Fault |

**Question:** Vector 14 (Page Fault) - what information does the error code contain?

**Answer:** 
- Bit 0: Present (0 = not-present page, 1 = protection violation)
- Bit 1: Write (0 = read, 1 = write)
- Bit 2: User (0 = supervisor, 1 = user mode)
- Bit 3: Reserved bit violation
- Bit 4: Instruction fetch

Plus, CR2 register contains the faulting virtual address!

---

## IDT Entry Structure

### 16-Byte Gate Descriptor (64-bit)

```
┌───────────────────────────────────────────────────────────────┐
│ Offset 63:32 (32 bits)                   │ Reserved (32 bits)│ Bytes 8-15
├───────────────┬──────┬───────────────────┬───────────────────┤
│ Offset 31:16  │ P DPL│   Type    │  IST  │    Selector      │ Bytes 0-7
│   (16 bits)   │ 0 00 │ (4 bits)  │(3 bit)│    (16 bits)     │
│               │ 1 00 │           │       │                   │
└───────────────┴──────┴───────────────────┴───────────────────┘
      16 bits     8 bits    8 bits (lower)      16 bits

Fields:
  Offset:   Address of handler function (split across entry)
  Selector: Code segment selector (0x08 for kernel code)
  IST:      Interrupt Stack Table index (0 = don't switch, 1-7 = use IST)
  Type:     0xE = 64-bit Interrupt Gate, 0xF = 64-bit Trap Gate
  DPL:      Ring level required to call via INT instruction
  P:        Present bit (must be 1)
```

```c
struct IDTEntry {
    uint16_t offset_low;     // Offset bits 0-15
    uint16_t selector;       // Code segment selector
    uint8_t  ist;            // IST offset (0 = none)
    uint8_t  type_attr;      // Type and attributes
    uint16_t offset_mid;     // Offset bits 16-31
    uint32_t offset_high;    // Offset bits 32-63
    uint32_t reserved;       // Must be zero
} __attribute__((packed));
```

### Exercise 12.2: Interrupt vs Trap Gate

```
Interrupt Gate (Type = 0xE):
  - CPU clears IF (Interrupt Flag) automatically
  - Interrupts disabled while handler runs
  - Use for: hardware interrupts, most exceptions

Trap Gate (Type = 0xF):
  - IF unchanged
  - Interrupts stay enabled
  - Use for: breakpoints, system calls
```

**Question:** Why disable interrupts for hardware interrupt handlers?

**Answer:** Prevents nested interrupts of the same type. If timer interrupt handler is interrupted by another timer interrupt before completing, you could corrupt state. Critical sections need protection.

---

## The Interrupt Stack Frame

When an interrupt occurs, the CPU pushes this onto the stack:

```
                    │                │ (Higher addresses)
                    ├────────────────┤
                    │      SS        │ +32 (only from ring 3)
                    ├────────────────┤
                    │      RSP       │ +24 (only from ring 3)
                    ├────────────────┤
                    │    RFLAGS      │ +16
                    ├────────────────┤
                    │      CS        │ +8
                    ├────────────────┤
                    │      RIP       │ ← RSP after interrupt
                    ├────────────────┤
If error code:      │  Error Code    │
                    ├────────────────┤
Our stub pushes:    │ Saved Registers│
                    │                │ (Lower addresses)
```

### Exercise 12.3: Uniform Stack Layout

Problem: Some exceptions push error code, some don't. Our handler wants uniform layout.

Solution: For exceptions without error code, push a dummy 0:

```asm
// Exception WITHOUT error code (like divide error)
isr0:
    push $0         // Dummy error code
    push $0         // Interrupt number
    jmp isr_common

// Exception WITH error code (like page fault)
isr14:
    // Error code already pushed by CPU!
    push $14        // Interrupt number
    jmp isr_common
```

---

## Writing Interrupt Handlers

### The ISR Stub (Assembly)

```asm
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
    
    // First argument = pointer to stack frame
    mov %rsp, %rdi
    
    // Call C handler
    call interrupt_handler
    
    // Restore registers
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
    
    // Remove error code and interrupt number
    add $16, %rsp
    
    // Return from interrupt
    iretq
```

### The C Handler

```c
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

void interrupt_handler(struct InterruptFrame *frame) {
    if (frame->int_no < 32) {
        // CPU exception
        handle_exception(frame);
    } else if (frame->int_no < 48) {
        // Hardware interrupt (IRQ)
        handle_irq(frame->int_no - 32);
    }
}
```

### Exercise 12.4: Implement a Handler Table

```c
typedef void (*interrupt_handler_t)(struct InterruptFrame *);
static interrupt_handler_t handlers[256] = {0};

void idt_set_handler(uint8_t vector, interrupt_handler_t handler) {
    handlers[vector] = handler;
}

void interrupt_handler(struct InterruptFrame *frame) {
    if (handlers[frame->int_no]) {
        handlers[frame->int_no](frame);
    } else {
        // Unhandled interrupt
        if (frame->int_no < 32) {
            panic("Unhandled exception %d", frame->int_no);
        }
    }
}
```

---

# Chapter 13: The Programmable Interrupt Controller (PIC)

## Hardware Interrupts

```
┌─────────────────────────────────────────────────────────────────┐
│                    HARDWARE INTERRUPTS                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Device raises IRQ line                                         │
│         │                                                       │
│         ▼                                                       │
│  PIC receives signal                                            │
│         │                                                       │
│         ▼                                                       │
│  PIC asserts INTR to CPU                                        │
│         │                                                       │
│         ▼                                                       │
│  CPU (if IF=1) acknowledges                                     │
│         │                                                       │
│         ▼                                                       │
│  PIC sends vector number                                        │
│         │                                                       │
│         ▼                                                       │
│  CPU looks up IDT[vector], jumps to handler                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## The 8259 PIC

```
              ┌──────────────────┐
    IRQ0 ────►│                  │
    IRQ1 ────►│   Master 8259    │────► INTR (to CPU)
    IRQ2 ◄────│                  │
    IRQ3 ────►│   (Port 0x20)    │
    IRQ4 ────►│                  │
    IRQ5 ────►│                  │
    IRQ6 ────►│                  │
    IRQ7 ────►│                  │
              └───────┬──────────┘
                      │ Cascade (IRQ2)
              ┌───────▼──────────┐
    IRQ8 ────►│                  │
    IRQ9 ────►│   Slave 8259     │
    IRQ10────►│                  │
    IRQ11────►│   (Port 0xA0)    │
    IRQ12────►│                  │
    IRQ13────►│                  │
    IRQ14────►│                  │
    IRQ15────►│                  │
              └──────────────────┘
```

### Default Mapping (Problem!)

```
Default BIOS mapping:
  IRQ 0-7  → INT 0x08-0x0F  ← CONFLICTS WITH CPU EXCEPTIONS!
  IRQ 8-15 → INT 0x70-0x77

INT 8 = Double Fault OR Timer?
INT 13 = General Protection OR Floppy?

This is why we must remap!
```

### Exercise 13.1: Remap the PIC

```c
#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

void pic_remap(void) {
    // ICW1: Initialize + ICW4 needed
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    
    // ICW2: Vector offsets
    outb(PIC1_DATA, 0x20);    // IRQ 0-7 → INT 32-39
    outb(PIC2_DATA, 0x28);    // IRQ 8-15 → INT 40-47
    
    // ICW3: Cascade setup
    outb(PIC1_DATA, 0x04);    // Slave on IRQ2
    outb(PIC2_DATA, 0x02);    // Slave ID = 2
    
    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    
    // Mask all interrupts initially
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
```

### End of Interrupt (EOI)

After handling an IRQ, you must tell the PIC:

```c
void pic_send_eoi(int irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);  // EOI to slave
    }
    outb(PIC1_CMD, 0x20);      // EOI to master
}
```

**Question:** Why must we send EOI to both PICs for IRQ 8-15?

**Answer:** The slave PIC connects through IRQ2 of the master. When the slave asserts IRQ2, both need to know the interrupt is complete.

### Masking/Unmasking IRQs

```c
void pic_unmask_irq(int irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    
    uint8_t mask = inb(port);
    mask &= ~(1 << irq);       // Clear bit to unmask
    outb(port, mask);
}

void pic_mask_irq(int irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    
    uint8_t mask = inb(port);
    mask |= (1 << irq);        // Set bit to mask
    outb(port, mask);
}
```

---

# Chapter 14: Timer Interrupt

## The Programmable Interval Timer (PIT)

The PIT generates periodic interrupts (IRQ 0):

```
PIT (8253/8254):
  - 3 channels (we use channel 0)
  - 1.193182 MHz base frequency
  - Programmable divisor
  
Frequency = 1193182 / divisor
Default divisor ≈ 65536 → ~18.2 Hz
```

### Exercise 14.1: Simple Timer Handler

```c
static volatile uint64_t ticks = 0;

void timer_handler(struct InterruptFrame *frame) {
    (void)frame;
    ticks++;
    
    // Signal end of interrupt
    pic_send_eoi(0);
}

void timer_init(void) {
    // Register handler
    idt_set_handler(32, timer_handler);  // IRQ0 = INT 32
    
    // Unmask timer IRQ
    pic_unmask_irq(0);
}

uint64_t get_ticks(void) {
    return ticks;
}
```

### Programming the PIT (Optional)

```c
#define PIT_CHANNEL0  0x40
#define PIT_CMD       0x43

void pit_set_frequency(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;
    
    // Command: Channel 0, lobyte/hibyte, rate generator
    outb(PIT_CMD, 0x36);
    
    // Send divisor
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
```

---

# Chapter 15: Keyboard Input

## PS/2 Keyboard Protocol

```
Keyboard Controller (8042):
  Port 0x60: Data port (read scan codes)
  Port 0x64: Status/Command port
  
When key pressed:
  1. Keyboard sends scan code to controller
  2. Controller raises IRQ 1
  3. CPU calls our handler
  4. Handler reads scan code from port 0x60
```

## Scan Codes

The keyboard sends scan codes, NOT ASCII:

```
Key Press (Make Code):
  A pressed → 0x1E
  
Key Release (Break Code):  
  A released → 0x9E (0x1E | 0x80)
  
The high bit indicates release!
```

### Exercise 15.1: Scan Code to ASCII

```c
// US QWERTY layout (partial)
static const char scancode_ascii[128] = {
    0,   0x1B, '1', '2', '3', '4', '5', '6',   // 0x00-0x07
    '7', '8',  '9', '0', '-', '=', '\b', '\t', // 0x08-0x0F
    'q', 'w',  'e', 'r', 't', 'y', 'u', 'i',   // 0x10-0x17
    'o', 'p',  '[', ']', '\n', 0,  'a', 's',   // 0x18-0x1F
    'd', 'f',  'g', 'h', 'j', 'k', 'l', ';',   // 0x20-0x27
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',   // 0x28-0x2F
    'b', 'n',  'm', ',', '.', '/', 0,   '*',   // 0x30-0x37
    0,   ' ',  0,   0,   0,   0,   0,   0,     // 0x38-0x3F
    // ... function keys, etc.
};

static const char scancode_ascii_shift[128] = {
    0,   0x1B, '!', '@', '#', '$', '%', '^',   // 0x00-0x07
    '&', '*',  '(', ')', '_', '+', '\b', '\t', // 0x08-0x0F
    'Q', 'W',  'E', 'R', 'T', 'Y', 'U', 'I',   // 0x10-0x17
    // ... etc
};
```

### Exercise 15.2: Keyboard Handler

```c
static bool shift_pressed = false;
static char key_buffer[256];
static int buffer_pos = 0;

void keyboard_handler(struct InterruptFrame *frame) {
    (void)frame;
    
    uint8_t scancode = inb(0x60);
    
    // Check for key release
    if (scancode & 0x80) {
        // Key released
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = false;
        }
        pic_send_eoi(1);
        return;
    }
    
    // Key pressed
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        pic_send_eoi(1);
        return;
    }
    
    // Convert to ASCII
    char c;
    if (shift_pressed) {
        c = scancode_ascii_shift[scancode];
    } else {
        c = scancode_ascii[scancode];
    }
    
    // Add to buffer
    if (c != 0 && buffer_pos < 255) {
        key_buffer[buffer_pos++] = c;
    }
    
    pic_send_eoi(1);
}

void keyboard_init(void) {
    idt_set_handler(33, keyboard_handler);  // IRQ1 = INT 33
    pic_unmask_irq(1);
}
```

---

# Chapter 16: Putting It All Together

## Initialization Order

```c
void kernel_main(struct BootInfo *boot_info) {
    // 1. Set up display (for debugging)
    framebuffer_init(&boot_info->framebuffer);
    
    // 2. Load GDT (required for IDT)
    gdt_init();
    
    // 3. Remap PIC (before enabling interrupts!)
    pic_init();
    
    // 4. Load IDT (interrupt handlers ready)
    idt_init();
    
    // 5. Install exception handlers
    exceptions_init();
    
    // 6. Install device handlers
    timer_init();
    keyboard_init();
    
    // 7. Enable interrupts!
    __asm__ volatile("sti");
    
    // 8. Main loop
    while (1) {
        // Process keyboard buffer
        // Update display
        // etc.
        __asm__ volatile("hlt");  // Wait for interrupt
    }
}
```

### Exercise 16.1: Debug with Visual Indicators

Since we can't easily print text yet, use colored rectangles:

```c
void kernel_main(struct BootInfo *boot_info) {
    // Blue background
    fill_screen(0x000000FF);
    
    // Yellow = GDT loaded
    gdt_init();
    draw_rect(100, 100, 50, 50, 0x00FFFF00);
    
    // Magenta = PIC initialized
    pic_init();
    draw_rect(160, 100, 50, 50, 0x00FF00FF);
    
    // Cyan = IDT loaded
    idt_init();
    draw_rect(220, 100, 50, 50, 0x0000FFFF);
    
    // Green = Interrupts enabled
    exceptions_init();
    timer_init();
    keyboard_init();
    __asm__ volatile("sti");
    draw_rect(280, 100, 50, 50, 0x0000FF00);
    
    // If we get here without crashing, success!
    while (1) {
        __asm__ volatile("hlt");
    }
}
```

---

## Summary: Phase 2 Checklist

After completing Phase 2, you should have:

- [ ] GDT with:
  - [ ] Null descriptor
  - [ ] Kernel code segment (64-bit)
  - [ ] Kernel data segment
  - [ ] User code segment (for later)
  - [ ] User data segment (for later)
  - [ ] TSS descriptor
- [ ] TSS structure with RSP0 set
- [ ] IDT with:
  - [ ] 32 exception handlers (0-31)
  - [ ] 16 IRQ handlers (32-47)
- [ ] PIC:
  - [ ] Remapped to vectors 32-47
  - [ ] EOI handling
  - [ ] IRQ masking/unmasking
- [ ] Working interrupts:
  - [ ] Timer ticking (IRQ0)
  - [ ] Keyboard input (IRQ1)
- [ ] Ability to enable/disable interrupts (sti/cli)

---

# Exercises for Phase 2

## Exercise Set D: GDT

**D1.** Calculate the GDT selector for user data segment at index 4 with RPL=3.

**D2.** Why can't we use a single segment for both code and data in 64-bit mode?

**D3.** What happens if you forget to reload segment registers after loading a new GDT?

## Exercise Set E: IDT

**E1.** Write the code to create an IDT entry for vector 14 (page fault) with the handler at address 0xFFFF800000001000.

**E2.** What's the difference between DPL in the IDT entry vs the segment DPL in GDT?

**E3.** If an interrupt occurs while handling another interrupt, what determines which stack is used?

## Exercise Set F: Interrupts

**F1.** Trace what happens from key press to character appearing in your buffer: physical key → electrical signal → IRQ → handler → buffer.

**F2.** Why must we read from port 0x60 even if we don't care about the key pressed?

**F3.** What happens if you forget to send EOI?

---

# How to Use This Tutorial

## In a New Chat (Learning Mode)

1. Paste this tutorial document
2. Say "I'm learning OS development. Let's work through the exercises in Chapter X."
3. Work through exercises with explanations
4. Ask questions about concepts

## In Implementation Chat

1. Reference this by section: "As described in Chapter 12, I need to..."
2. Ask for implementation help: "Help me implement the ISR stubs from Exercise 12.3"
3. Debug using the concepts: "According to the checklist, I should have..."

## Suggested Learning Path

```
Day 1: Chapters 1-5 (UEFI basics)
Day 2: Chapters 6-10 (Bootloader to kernel)
Day 3: Chapters 11-12 (GDT and IDT)
Day 4: Chapters 13-15 (PIC and device handlers)
Day 5: Chapter 16 (Integration)
Day 6: Exercises and debugging
Day 7: Review and prepare for Phase 3
```

---

*This tutorial covers Phase 1 (Boot) and Phase 2 (CPU Setup). Continue with the implementation using the CONTEXT.md file and main project tarball.*
