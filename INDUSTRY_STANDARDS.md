# OS Development: Industry Standards & Elegant Approaches

A reference guide for production-quality OS techniques. These are documented for learning - implement them later when the basic OS is working.

---

## Table of Contents

1. [Memory Allocators](#1-memory-allocators)
2. [Higher-Half Kernel](#2-higher-half-kernel)
3. [Interrupt Controllers: PIC vs APIC](#3-interrupt-controllers-pic-vs-apic)
4. [Interrupt Handling: Top/Bottom Half](#4-interrupt-handling-topbottom-half)
5. [Exception Handling with IST](#5-exception-handling-with-ist)
6. [Kernel Logging](#6-kernel-logging)
7. [Boot Protocol Standards](#7-boot-protocol-standards)
8. [System Calls: int vs syscall](#8-system-calls-int-vs-syscall)
9. [Kernel ASLR](#9-kernel-aslr)
10. [Per-CPU Data](#10-per-cpu-data)
11. [Summary & Priorities](#11-summary--priorities)

---

## 1. Memory Allocators

### Overview

Physical memory allocators manage which 4KB pages are free/used.

| Approach | Alloc | Free | Contiguous | Fragmentation | Complexity |
|----------|-------|------|------------|---------------|------------|
| Bitmap | O(n) | O(1) | O(n) | None | Very Low |
| Free List | O(1) | O(1) | O(n) | High | Low |
| Buddy | O(log n) | O(log n) | O(log n) | Low | Medium |
| Buddy + Per-CPU | O(1)* | O(1)* | O(log n) | Low | High |

### What Linux Does: Buddy System + Slab

**Buddy System for Pages:**
```
Memory divided into power-of-2 sized blocks:

Order 0: 4KB blocks    ████ ████ ████ ████ ████ ████ ████ ████
Order 1: 8KB blocks    ████████ ████████ ████████ ████████
Order 2: 16KB blocks   ████████████████ ████████████████
Order 3: 32KB blocks   ████████████████████████████████
...up to order 10 (4MB blocks)

Free lists maintained for each order.
```

**How Buddy Allocation Works:**
```
Allocating 12KB (needs order-2 = 16KB):
1. Check order-2 free list
2. If empty, split order-3 block into two order-2 blocks
3. Return one, keep other on free list

Freeing:
1. Check if "buddy" block is also free
2. If yes, merge into larger block
3. Repeat until buddy is not free

This prevents fragmentation elegantly!
```

**Slab Allocator (on top of buddy):**
```c
// Pre-allocate caches of common object sizes
struct kmem_cache *task_cache = kmem_cache_create(
    "task_struct",           // Name
    sizeof(struct task),     // Object size
    0,                       // Alignment
    SLAB_PANIC,              // Flags
    NULL                     // Constructor
);

// Fast allocation of fixed-size objects
struct task *t = kmem_cache_alloc(task_cache, GFP_KERNEL);
kmem_cache_free(task_cache, t);
```

### What Windows Does: PFN Database

**Structure:**
```c
// Array indexed by Page Frame Number
MMPFN PfnDatabase[TotalPhysicalPages];

// O(1) lookup: physical address → page info
MMPFN *pfn = &PfnDatabase[phys_addr >> 12];
```

**Page State Lists:**
```
┌─────────────────────────────────────────────────────────────────┐
│                     PFN Database (array)                        │
├─────────────────────────────────────────────────────────────────┤
│  Free List Head ──────► [2] ──► [5] ──► [9] ──► NULL           │
│  Zeroed List Head ────► [0] ──► [4] ──► [7] ──► NULL           │
│  Standby List Head ───► [1] ──► [3] ──► NULL                   │
│  Modified List Head ──► [6] ──► NULL                           │
└─────────────────────────────────────────────────────────────────┘

- Free: Available, contents undefined
- Zeroed: Available, zero-filled (security - no data leaks)
- Standby: Soft-free, still cached, instant reclaim if needed
- Modified: Dirty, needs write to disk before reuse
```

**Zero Page Thread:**
```c
// Background thread (lowest priority)
while (1) {
    if (zeroed_list_count < threshold) {
        page = pop_free_list();
        memset(page, 0, 4096);
        push_zeroed_list(page);
    }
    sleep();
}
// Benefit: Allocation doesn't pay zeroing cost
```

**Contiguous Allocation Problem:**
```
Windows free list is unordered:
  [2] → [5] → [0] → [7] → [3] → [9]

Finding 4 consecutive pages requires O(n) scan!
Linux buddy: O(log n) - much better for DMA/GPU needs.

Windows solution: Memory compaction (expensive, move pages around)
```

### Comparison

| Aspect | Linux Buddy | Windows PFN |
|--------|-------------|-------------|
| Single page alloc | O(1) with per-CPU cache | O(1) |
| Contiguous alloc | O(log n) - elegant | O(n) + compaction |
| Fragmentation | Buddy merging prevents it | Compaction fixes it |
| Page caching | Page cache + LRU | Standby/Modified lists |

### Recommendation for MyOS

**Phase 3:** Simple free list (O(1), easy to understand)
```c
// Use free pages themselves as linked list
static uint64_t free_list_head = 0;

uint64_t pmm_alloc_page(void) {
    if (!free_list_head) return 0;
    uint64_t page = free_list_head;
    free_list_head = *(uint64_t *)phys_to_virt(page);
    return page;
}

void pmm_free_page(uint64_t page) {
    *(uint64_t *)phys_to_virt(page) = free_list_head;
    free_list_head = page;
}
```

**Later:** Add page metadata array (like Windows PFN), then buddy if needed.

---

## 2. Higher-Half Kernel

### The Problem

```
Current MyOS layout:

0x00000000 ┌─────────────────┐
           │ Real mode junk  │
0x00100000 ├─────────────────┤
           │ KERNEL (1MB)    │  ← We're here
0x00200000 ├─────────────────┤
           │ Free RAM        │
           │ (kernel + user  │
           │  all mixed!)    │  ← No separation!
           └─────────────────┘

Problems:
- User programs might overwrite kernel
- Context switch requires changing ALL mappings
- Kernel address varies based on where bootloader put it
```

### The Solution: Higher-Half Kernel

```
Virtual Memory Layout (Linux x86-64 example):

0x0000000000000000 ┌─────────────────────┐
                   │                     │
                   │    USER SPACE       │  128 TB for user programs
                   │    (per process)    │  Each process has own view
                   │                     │
0x00007FFFFFFFFFFF ├─────────────────────┤
                   │                     │
                   │  non-canonical hole │  CPU faults if accessed
                   │                     │
0xFFFF800000000000 ├─────────────────────┤
                   │  direct physical    │  All RAM mapped here
                   │  memory map         │  phys_to_virt() is just + offset
0xFFFF888000000000 ├─────────────────────┤
                   │  vmalloc area       │  Non-contiguous kernel allocs
                   │                     │
0xFFFFFFFF80000000 ├─────────────────────┤
                   │  KERNEL TEXT        │  Kernel code/data (-2GB)
                   │  (same in all       │  Same virtual address always
                   │   processes!)       │
0xFFFFFFFFFFFFFFFF └─────────────────────┘
```

### Why Higher-Half is Elegant

**1. Clean separation:**
```
User code:   0x0000000000400000  (low addresses)
Kernel code: 0xFFFFFFFF80100000  (high addresses)

User can't accidentally (or maliciously) access kernel addresses.
Different page tables entries entirely.
```

**2. Context switch optimization:**
```
Process A → Process B:

Without higher-half:
  - Swap ALL page table entries
  
With higher-half:
  - User half: Different per process, swap these
  - Kernel half: Same for all processes, keep these!
  
Page tables can share kernel portion = faster + less memory
```

**3. Physical memory access trick:**
```c
// Direct map: all physical RAM mapped at known virtual offset
#define PHYS_MAP_BASE  0xFFFF888000000000ULL

// Convert physical ↔ virtual instantly (no page table walk!)
#define phys_to_virt(p)  ((void *)((uint64_t)(p) + PHYS_MAP_BASE))
#define virt_to_phys(v)  ((uint64_t)(v) - PHYS_MAP_BASE)

// Access any physical address:
void *ptr = phys_to_virt(0x1000);  // Access physical page 0x1000
```

### Implementation Overview

**Boot sequence:**
```
1. Bootloader loads kernel (anywhere in physical memory)
2. Kernel creates page tables:
   - Identity map current location (so we don't crash)
   - Map kernel to 0xFFFFFFFF80000000
   - Map all physical RAM to 0xFFFF888000000000
3. Load CR3 with new page tables
4. Jump to high virtual address
5. Remove identity mapping (no longer needed)
```

**Page table structure (4-level):**
```
CR3 → PML4 (512 entries)
        ├── Entry 0: User space PML4E → PDPT → PD → PT → Pages
        ├── ...
        ├── Entry 256: Start of kernel space (0xFFFF800000000000)
        ├── ...
        └── Entry 511: Kernel text (0xFFFFFFFF80000000)
```

---

## 3. Interrupt Controllers: PIC vs APIC

### 8259 PIC (What We Use)

```
┌──────────────┐         ┌──────────────┐
│  Master PIC  │◄────────│  Slave PIC   │
│  (IRQ 0-7)   │  IRQ 2  │  (IRQ 8-15)  │
└──────┬───────┘         └──────┬───────┘
       │                        │
    IRQ 0: Timer             IRQ 8:  RTC
    IRQ 1: Keyboard          IRQ 9:  Free
    IRQ 2: Cascade ──────►   IRQ 10: Free
    IRQ 3: COM2              IRQ 11: Free
    IRQ 4: COM1              IRQ 12: PS/2 Mouse
    IRQ 5: LPT2              IRQ 13: FPU
    IRQ 6: Floppy            IRQ 14: Primary ATA
    IRQ 7: LPT1              IRQ 15: Secondary ATA
```

**Limitations:**
- Only 15 usable IRQs (IRQ 2 used for cascade)
- Single CPU only
- Edge-triggered only (can miss interrupts)
- Slow I/O port access
- No way to send inter-processor interrupts

### APIC (What Modern Systems Use)

```
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│  CPU 0  │    │  CPU 1  │    │  CPU 2  │    │  CPU 3  │
│ ┌─────┐ │    │ ┌─────┐ │    │ ┌─────┐ │    │ ┌─────┐ │
│ │LAPIC│ │    │ │LAPIC│ │    │ │LAPIC│ │    │ │LAPIC│ │
│ └──┬──┘ │    │ └──┬──┘ │    │ └──┬──┘ │    │ └──┬──┘ │
└────┼────┘    └────┼────┘    └────┼────┘    └────┼────┘
     │              │              │              │
     └──────────────┴──────────────┴──────────────┘
                         │
                    System Bus
                         │
                    ┌────┴────┐
                    │ I/O APIC │ ← Routes device IRQs to CPUs
                    └────┬────┘
                         │
        ┌────────┬───────┴────┬─────────┐
      [USB]  [NVMe]    [NIC]     [GPU]

LAPIC = Local APIC (one per CPU)
I/O APIC = Handles device interrupts
```

### Comparison

| Feature | 8259 PIC | APIC |
|---------|----------|------|
| IRQ count | 15 | 224 |
| Multi-CPU | No | Yes - can target specific CPU |
| Trigger mode | Edge only | Edge or Level selectable |
| Priority | Fixed | Fully programmable |
| IPI support | Impossible | Built-in (essential for SMP) |
| Access method | I/O ports (slow) | Memory-mapped (fast) |
| Modern systems | Emulated for compatibility | Primary controller |

### x2APIC (Even Better)

```c
// Regular APIC: memory-mapped registers at 0xFEE00000
volatile uint32_t *apic = (uint32_t *)0xFEE00000;
apic[0x80/4] = value;  // Memory write, needs TLB

// x2APIC: MSR access (faster, no TLB)
wrmsr(MSR_X2APIC_TPR, value);  // Direct MSR
```

### Inter-Processor Interrupts (IPI)

The killer feature of APIC:
```c
// "Hey CPU 3, I need you to do something!"
void send_ipi(int target_cpu, int vector) {
    uint32_t icr = (target_cpu << 24) | vector;
    apic_write(APIC_ICR, icr);
}

// Use cases:
// 1. TLB shootdown - tell other CPUs to flush TLB entries
send_ipi_all(TLB_FLUSH_VECTOR);

// 2. Scheduler - wake idle CPU to run new task
send_ipi(idle_cpu, RESCHEDULE_VECTOR);

// 3. Panic - stop all other CPUs
send_ipi_all_except_self(PANIC_VECTOR);
```

### APIC Initialization

```c
// 1. Check APIC support
uint32_t eax, ebx, ecx, edx;
cpuid(1, &eax, &ebx, &ecx, &edx);
bool has_apic = (edx >> 9) & 1;

// 2. Enable APIC via MSR
uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
apic_base |= (1 << 11);  // Enable bit
wrmsr(MSR_IA32_APIC_BASE, apic_base);

// 3. Map APIC registers (at physical 0xFEE00000)
// Usually identity-mapped or in direct physical map

// 4. Set spurious interrupt vector and enable
apic_write(APIC_SIVR, SPURIOUS_VECTOR | 0x100);

// 5. Configure I/O APIC for device interrupts
// Parse ACPI MADT table to find I/O APIC address
// Program redirection entries for each IRQ
```

---

## 4. Interrupt Handling: Top/Bottom Half

### The Problem

```c
// Our current approach:
void keyboard_handler(InterruptFrame *frame) {
    uint8_t scancode = inb(0x60);     // Quick
    char c = translate(scancode);      // Medium
    update_terminal(c);                // Could be slow!
    redraw_screen();                   // Very slow!
    // All running with interrupts disabled...
    // Miss timer ticks, network packets, etc.
}
```

### The Solution: Split Processing

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTERRUPT FLOW                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Device ──► IRQ ──► TOP HALF ──► Schedule ──► BOTTOM HALF       │
│                     (fast!)      bottom       (can take time)   │
│                     <1µs         half         interrupts on     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Top Half (Hardirq):**
- Runs with interrupts disabled
- Must be FAST (microseconds)
- Only does essential work: acknowledge hardware, save data
- Schedules bottom half for real processing

**Bottom Half:**
- Runs with interrupts enabled
- Can take as long as needed
- Does actual processing work
- Multiple mechanisms available

### Linux Bottom Half Mechanisms

**1. Softirqs (Fastest, limited)**
```c
// Fixed number (32), statically allocated
// Used for: networking, block devices, timers, scheduler

void my_softirq_handler(struct softirq_action *a) {
    // Process queued work
}

// Raised from top half:
raise_softirq(MY_SOFTIRQ);

// Runs after hardirq, still in interrupt context
// Cannot sleep!
```

**2. Tasklets (Simpler softirqs)**
```c
// Dynamic, built on softirqs
// Guaranteed: same tasklet won't run on two CPUs simultaneously

DECLARE_TASKLET(my_tasklet, my_tasklet_func, data);

// In top half:
tasklet_schedule(&my_tasklet);

// Tasklet runs later, interrupts enabled, but can't sleep
```

**3. Workqueues (Most flexible)**
```c
// Runs in process context (dedicated kernel threads)
// CAN SLEEP - can call kmalloc, mutex_lock, etc.

struct work_struct my_work;
INIT_WORK(&my_work, my_work_func);

// In top half or anywhere:
schedule_work(&my_work);

// Work function runs later in kworker thread:
void my_work_func(struct work_struct *work) {
    // Can do anything here - disk I/O, memory allocation, etc.
}
```

**4. Threaded IRQs (Dedicated thread per IRQ)**
```c
// Each IRQ gets its own kernel thread
request_threaded_irq(irq,
    quick_handler,    // Top half: just wake thread
    thread_handler,   // Thread: do real work
    flags, name, dev);

// Good for slow devices or when handler needs to sleep
```

### Example: Keyboard with Top/Bottom Half

```c
// TOP HALF - runs in interrupt context, interrupts disabled
static irqreturn_t keyboard_top_half(int irq, void *dev) {
    uint8_t scancode = inb(0x60);  // Must read to clear interrupt
    
    // Save to small buffer
    kb_scancode_buffer[kb_write_idx++] = scancode;
    
    // Schedule bottom half
    tasklet_schedule(&keyboard_tasklet);
    
    return IRQ_HANDLED;  // ~1µs total
}

// BOTTOM HALF - runs with interrupts enabled
static void keyboard_bottom_half(unsigned long data) {
    while (kb_read_idx != kb_write_idx) {
        uint8_t scancode = kb_scancode_buffer[kb_read_idx++];
        
        // Complex processing OK here:
        char c = translate_scancode(scancode);  // Lookup tables
        handle_modifiers(scancode);              // State machine
        update_terminal(c);                      // Buffer management
        wake_up_readers();                       // Wake waiting processes
    }
}
```

### When to Use What

| Mechanism | Latency | Can Sleep | Use Case |
|-----------|---------|-----------|----------|
| Softirq | Lowest | No | Network packets, block I/O |
| Tasklet | Low | No | Driver-specific, moderate work |
| Workqueue | Medium | Yes | Disk I/O, complex processing |
| Threaded IRQ | Higher | Yes | Slow devices, simplicity |

---

## 5. Exception Handling with IST

### The Problem

```
Normal exception handling:
  
  Exception occurs
       │
       ▼
  CPU pushes SS, RSP, RFLAGS, CS, RIP, [error code]
  onto CURRENT stack
       │
       ▼
  Jump to handler

What if the stack is broken?

  Kernel stack overflow
       │
       ▼
  Stack grows into unmapped memory
       │
       ▼
  Page fault! Push state to... broken stack?
       │
       ▼
  Double fault! Push state to... still broken!
       │
       ▼
  Triple fault → CPU reset → reboot
       │
       ▼
  No debugging info, no crash dump, nothing.
```

### The Solution: IST (Interrupt Stack Table)

The TSS can define up to 7 alternative stacks:

```c
struct TSS {
    uint32_t reserved0;
    uint64_t rsp0;        // Stack for ring 3 → ring 0
    uint64_t rsp1;        // (unused in 64-bit)
    uint64_t rsp2;        // (unused in 64-bit)
    uint64_t reserved1;
    uint64_t ist1;        // Emergency stack 1 (NMI)
    uint64_t ist2;        // Emergency stack 2 (Double Fault)
    uint64_t ist3;        // Emergency stack 3 (Machine Check)
    uint64_t ist4;        // Emergency stack 4 (Debug)
    uint64_t ist5;        // Available
    uint64_t ist6;        // Available
    uint64_t ist7;        // Available
    // ...
};
```

**Assign IST to critical exceptions:**
```c
// IDT entry has 3-bit IST field
idt[DOUBLE_FAULT].ist = 2;      // Use IST2
idt[NMI].ist = 1;               // Use IST1  
idt[MACHINE_CHECK].ist = 3;     // Use IST3
idt[DEBUG].ist = 4;             // Use IST4 (for stack traces)

// All other exceptions use IST=0 (current stack)
```

**Stack allocation:**
```c
#define IST_STACK_SIZE  4096

// Allocate separate stacks for each IST
static uint8_t ist1_stack[IST_STACK_SIZE] __aligned(16);
static uint8_t ist2_stack[IST_STACK_SIZE] __aligned(16);
static uint8_t ist3_stack[IST_STACK_SIZE] __aligned(16);
static uint8_t ist4_stack[IST_STACK_SIZE] __aligned(16);

void tss_init(void) {
    // IST entries point to TOP of stack (stack grows down)
    tss.ist1 = (uint64_t)&ist1_stack[IST_STACK_SIZE];
    tss.ist2 = (uint64_t)&ist2_stack[IST_STACK_SIZE];
    tss.ist3 = (uint64_t)&ist3_stack[IST_STACK_SIZE];
    tss.ist4 = (uint64_t)&ist4_stack[IST_STACK_SIZE];
}
```

### Now Stack Overflow is Survivable

```
Stack overflow occurs
       │
       ▼
Page fault (stack in guard page)
       │
       ▼
CPU switches to IST2 (separate, working stack!)
       │
       ▼
Double fault handler runs successfully
       │
       ▼
Can print "KERNEL STACK OVERFLOW"
Can print register dump
Can attempt crash dump to disk
Controlled panic instead of mysterious reboot
```

### Linux IST Usage

| IST | Exception | Why |
|-----|-----------|-----|
| 1 | NMI | NMI can occur anytime, even during exception handling |
| 2 | Double Fault | Original stack is probably broken |
| 3 | Machine Check | Hardware error, stack may be corrupt |
| 4 | Debug | Need reliable stack for debugging itself |

---

## 6. Kernel Logging

### Current Approach Problems

```c
console_print("Boot complete\n");
// Problems:
// 1. Lost if screen scrolls
// 2. Lost on crash (no scroll-back)
// 3. Can't capture remotely
// 4. No timestamps
// 5. No severity levels
// 6. Slow (framebuffer writes)
```

### Production Logging: printk

**Log Levels:**
```c
#define KERN_EMERG   "0"  // System is unusable
#define KERN_ALERT   "1"  // Action must be taken immediately  
#define KERN_CRIT    "2"  // Critical conditions
#define KERN_ERR     "3"  // Error conditions
#define KERN_WARNING "4"  // Warning conditions
#define KERN_NOTICE  "5"  // Normal but significant
#define KERN_INFO    "6"  // Informational
#define KERN_DEBUG   "7"  // Debug-level messages

// Usage:
printk(KERN_ERR "Failed to allocate page: error %d\n", err);
printk(KERN_INFO "Detected %d MB of RAM\n", ram_mb);
printk(KERN_DEBUG "Entering function %s\n", __func__);
```

**Ring Buffer:**
```
┌─────────────────────────────────────────────────────────────────┐
│                    Kernel Log Buffer                            │
│                    (circular, typically 1MB)                    │
├─────────────────────────────────────────────────────────────────┤
│[    0.000000] Linux version 5.15.0 ...                         │
│[    0.000023] Command line: console=ttyS0,115200 root=/dev/sda │
│[    0.004521] BIOS-provided physical RAM map:                   │
│[    0.004523] BIOS-e820: [mem 0x0000000000000000-0x000009fbff] │
│        ↓ ↓ ↓ (circular, overwrites oldest) ↓ ↓ ↓              │
│[  142.789012] USB device connected                             │
│[  142.790000] Keyboard detected                                │
│                                              ← write pointer   │
└─────────────────────────────────────────────────────────────────┘

Benefits:
- Survives crashes (if memory intact)
- dmesg reads from this buffer
- Bounded memory usage
- Fast writes (just memory copy)
```

**Multiple Output Backends:**
```c
// Same message goes to all configured outputs:

1. Ring buffer (always)
   - Read via dmesg
   - Read via /dev/kmsg
   
2. Console (if configured)
   - kernel command line: console=tty0
   - Framebuffer or VGA text
   
3. Serial port (essential for OS dev!)
   - kernel command line: console=ttyS0,115200
   - Capture on host machine
   - Works even when display broken
   
4. Netconsole (production debugging)
   - Send logs via UDP to remote server
   - Debug machines in datacenter
```

### Serial Console for OS Development

**Why essential:**
```
Scenario: Kernel crashes during boot before display initialized

With framebuffer only:
  - Black screen
  - No idea what happened
  - Add debug code, recompile, repeat...

With serial console:
  - All printk output captured on host
  - See exact line where it crashed
  - Works even for triple faults (with care)
```

**QEMU serial setup:**
```bash
# Output serial to terminal
qemu-system-x86_64 ... -serial stdio

# Output serial to file
qemu-system-x86_64 ... -serial file:serial.log

# Connect via telnet
qemu-system-x86_64 ... -serial tcp::4444,server,nowait
# Then: telnet localhost 4444
```

**Simple serial output (8250 UART):**
```c
#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);  // Disable interrupts
    outb(COM1 + 3, 0x80);  // Enable DLAB
    outb(COM1 + 0, 0x01);  // Baud divisor low (115200)
    outb(COM1 + 1, 0x00);  // Baud divisor high
    outb(COM1 + 3, 0x03);  // 8 bits, no parity, 1 stop
    outb(COM1 + 2, 0xC7);  // Enable FIFO
    outb(COM1 + 4, 0x0B);  // DTR + RTS
}

void serial_putc(char c) {
    while ((inb(COM1 + 5) & 0x20) == 0);  // Wait for TX empty
    outb(COM1, c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}
```

---

## 7. Boot Protocol Standards

### Current Approach

```c
// We invented our own BootInfo struct:
struct BootInfo {
    struct FramebufferInfo framebuffer;
    struct MemoryMapEntry memory_map[256];
    uint32_t memory_map_count;
    uint64_t acpi_rsdp;
};

// Problems:
// - Only works with our bootloader
// - We handle all the complexity
// - Edge cases probably not covered
```

### Multiboot2 (GRUB)

```c
// Standardized by GNU GRUB
// Kernel declares what it needs via header:

struct multiboot_header {
    uint32_t magic;           // 0xE85250D6
    uint32_t architecture;    // 0 = i386
    uint32_t header_length;
    uint32_t checksum;
    // ... tags requesting features
};

// GRUB provides info via structure:
struct multiboot_info {
    uint32_t total_size;
    uint32_t reserved;
    // ... variable tags with framebuffer, memory map, etc.
};

// Pros:
// - Very well documented
// - GRUB handles BIOS/UEFI differences
// - Works on real hardware with any GRUB install

// Cons:
// - Complex specification
// - BIOS-era design showing its age
// - GRUB must be installed
```

### Limine Protocol (Modern)

```c
// Modern UEFI-native bootloader
// Clean request/response design:

// Request framebuffer:
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Request memory map:
static volatile struct limine_memmap_request mmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

// Request higher-half direct map (Limine handles it!):
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

void _start(void) {
    // Limine filled in the responses:
    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    
    uint64_t hhdm_offset = hhdm_request.response->offset;
    // Now phys_to_virt is just: ptr = phys + hhdm_offset
}

// Pros:
// - Clean, modern design
// - Handles higher-half kernel for you
// - UEFI native
// - Active development

// Cons:
// - Less widespread than GRUB
// - Need to bundle Limine binary
```

### Why Standards Matter

1. **Documentation exists** - Someone else figured out edge cases
2. **Real hardware tested** - Community found bugs already
3. **Bootloader flexibility** - Switch bootloaders without kernel changes
4. **Features for free** - Limine gives you higher-half mapping

---

## 8. System Calls: int vs syscall

### Traditional: int 0x80

```asm
; User space makes system call:
mov rax, 1        ; syscall number (sys_write)
mov rdi, 1        ; arg1: fd (stdout)
mov rsi, message  ; arg2: buffer
mov rdx, length   ; arg3: length
int 0x80          ; trap to kernel!

; CPU does:
; 1. Look up IDT entry 0x80
; 2. Check privilege level
; 3. Load new CS:RIP from IDT
; 4. Switch to kernel stack (from TSS)
; 5. Push SS, RSP, RFLAGS, CS, RIP
; 6. Jump to handler
```

**Problems:**
- IDT lookup
- Full privilege check
- Stack switch via TSS (memory access)
- Push many values
- ~100+ CPU cycles

### Modern: syscall/sysret

```asm
; User space:
mov rax, 1        ; syscall number
mov rdi, 1        ; arg1 (same convention as function calls!)
mov rsi, message  ; arg2
mov rdx, length   ; arg3
syscall           ; fast system call!

; CPU does (hardware-accelerated):
; 1. RCX = RIP (save return address)
; 2. R11 = RFLAGS (save flags)
; 3. RIP = MSR_LSTAR (load handler address)
; 4. CS = MSR_STAR[47:32], SS = MSR_STAR[47:32] + 8
; 5. RFLAGS &= ~MSR_SFMASK
; That's it! ~20-30 cycles

; Kernel handler:
syscall_entry:
    swapgs                    ; Switch GS to kernel (per-CPU data)
    mov [gs:user_rsp], rsp    ; Save user stack
    mov rsp, [gs:kernel_rsp]  ; Load kernel stack
    push rcx                  ; Save return RIP
    push r11                  ; Save return RFLAGS
    ; ... handle syscall ...
    pop r11
    pop rcx
    mov rsp, [gs:user_rsp]    ; Restore user stack
    swapgs
    sysretq                   ; Return to user
```

### Comparison

| Aspect | int 0x80 | syscall |
|--------|----------|---------|
| Cycles | ~100+ | ~20-30 |
| IDT lookup | Yes | No |
| Privilege check | Full | Minimal (MSR-based) |
| Stack setup | Via TSS | Manual (faster) |
| Segment loads | All segments | Just CS/SS |
| Return | iret (~80 cycles) | sysret (~20 cycles) |

### Setup Requirements

```c
// syscall/sysret require MSR configuration:

// STAR: segment selectors
// Bits 32-47: kernel CS (also sets SS = CS + 8)
// Bits 48-63: user CS (user SS = CS + 8, but order reversed!)
uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32) |
                ((uint64_t)(GDT_USER_CODE - 16) << 48);
wrmsr(MSR_STAR, star);

// LSTAR: syscall entry point
wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

// SFMASK: RFLAGS bits to clear (disable interrupts during entry)
wrmsr(MSR_SFMASK, RFLAGS_IF | RFLAGS_TF);

// GDT must have specific layout:
// kernel code, kernel data, user code, user data (in that order!)
```

---

## 9. Kernel ASLR

### The Problem

```
Without ASLR:
  Kernel always at 0xFFFFFFFF81000000
  schedule() at 0xFFFFFFFF81234567
  
Attacker knows exactly where everything is.
Memory corruption bug → jump to known address → exploit
```

### The Solution

```
With KASLR (Kernel Address Space Layout Randomization):

Boot 1: Kernel base = 0xFFFFFFFF81234000
Boot 2: Kernel base = 0xFFFFFFFF89ABC000  
Boot 3: Kernel base = 0xFFFFFFFF85678000
        ↑ Random every boot!

Attacker doesn't know where schedule() is.
Must first leak a kernel address (much harder exploit).
```

### Implementation

**Bootloader picks random offset:**
```c
// Get random from RDRAND, RTC, or other entropy source
uint64_t random = get_random_long();

// Mask to valid range, align to 2MB
uint64_t kaslr_offset = random & 0x3FE00000;  // ~1GB range, 2MB aligned

// Load kernel at: BASE + kaslr_offset
load_kernel(KERNEL_BASE + kaslr_offset);

// Pass offset to kernel so it knows where it is
boot_params.kaslr_offset = kaslr_offset;
```

**Kernel must be position-independent:**
```bash
# Compile with:
-fPIC                    # Position Independent Code
-mcmodel=large           # Large code model (64-bit addresses)
# Or use -fno-pie and handle relocations
```

**Symbol addresses adjusted at runtime:**
```c
// All symbol addresses shifted by same offset
// kallsyms (kernel symbol table) adjusted at boot

// Getting real address:
void *schedule_addr = (void *)((uint64_t)schedule + kaslr_offset);
```

### Additional ASLR Targets

```
KASLR can randomize multiple regions separately:

- Kernel text: main kernel code
- Kernel modules: loadable modules  
- Direct physical map: phys_to_virt offset
- vmalloc area: dynamic allocations

Each can have different random offset = harder to exploit
```

### Defeating KASLR

Attackers try to leak addresses:
```c
// Information leaks that reveal kernel addresses:
// - /proc/kallsyms (now restricted)
// - Timing side channels (Spectre/Meltdown)
// - Uninitialized memory leaks
// - dmesg leaking pointers (now %pK format)

// Defense: restrict access, hash/hide pointers in logs
printk("Object at %pK\n", ptr);  // Shows "(____ptrval____)" to non-root
```

---

## 10. Per-CPU Data

### The Problem

```c
// Global variables with multiple CPUs:
volatile int current_task_id;

// CPU 0 writes: current_task_id = 5;
// CPU 1 writes: current_task_id = 7;
// CPU 0 reads:  current_task_id → maybe 5, maybe 7?

// Need locks everywhere:
spin_lock(&global_lock);
current_task_id = 5;
spin_unlock(&global_lock);

// Even for data that's logically per-CPU!
```

### The Solution: Per-CPU Variables

```c
// Linux: Each CPU has its own copy
DEFINE_PER_CPU(int, cpu_number);
DEFINE_PER_CPU(struct task *, current_task);
DEFINE_PER_CPU(unsigned long, irq_count);

// Access current CPU's copy (no lock needed!):
this_cpu_write(current_task, new_task);
struct task *t = this_cpu_read(current_task);

// Access specific CPU's copy:
int cpu3_irqs = per_cpu(irq_count, 3);
```

### Implementation

**Memory layout:**
```
┌─────────────────────┐  ← per_cpu_offset[0]
│   CPU 0 per-cpu     │
│   - current_task    │
│   - irq_count       │
│   - preempt_count   │
│   - ... more vars   │
├─────────────────────┤  ← per_cpu_offset[1]
│   CPU 1 per-cpu     │
│   - current_task    │
│   - irq_count       │
│   - ... (same vars) │
├─────────────────────┤  ← per_cpu_offset[2]
│   CPU 2 per-cpu     │
│   ...               │
└─────────────────────┘
```

**Fast access via GS segment:**
```c
// GS base register points to current CPU's per-cpu area
// Set during boot and on CPU switch

// Access is just: mov %gs:offset, %rax
// Single instruction, no locks, very fast!

static inline void *get_current_task(void) {
    void *task;
    asm("mov %%gs:%1, %0" 
        : "=r"(task) 
        : "m"(*(void **)CURRENT_TASK_OFFSET));
    return task;
}
```

**SWAPGS instruction:**
```asm
; On syscall/interrupt from user mode:
swapgs              ; Swap user GS ↔ kernel GS
; Now GS points to kernel per-CPU data

; On return to user mode:
swapgs              ; Swap back
; Now GS points to user's TLS (Thread Local Storage)
```

### Why This Matters

```
Without per-CPU:
  Read current task = lock + global read + unlock
  Every function that touches scheduler = contention
  
With per-CPU:
  Read current task = single memory read
  No locking for CPU-local data
  Linear scaling with CPU count
```

### Common Per-CPU Variables

| Variable | Purpose |
|----------|---------|
| current | Currently running task pointer |
| cpu_number | Which CPU am I? |
| irq_count | Nested interrupt depth |
| preempt_count | Preemption disable depth |
| runqueue | Per-CPU scheduler queue |
| page_cache | Per-CPU page allocator cache |

---

## 11. Summary & Priorities

### What We Have vs Production

| Component | MyOS Now | Production Quality |
|-----------|----------|-------------------|
| Kernel location | 1MB physical | Higher-half virtual |
| Interrupt controller | 8259 PIC | APIC + I/O APIC |
| Interrupt handling | All in handler | Top/bottom half split |
| Critical exceptions | Same stack | IST separate stacks |
| Logging | Framebuffer only | Ring buffer + serial |
| Boot protocol | Custom BootInfo | Multiboot2/Limine |
| System calls | (not yet) | syscall/sysret |
| Security | None | KASLR, SMEP, SMAP |
| Multi-CPU | Not supported | Per-CPU data |

### Recommended Priority Order

**For learning and debugging value:**

1. **Serial console logging** ★★★★★
   - Effort: Low
   - Value: Huge - makes debugging 10x easier
   - Do this first!

2. **IST for double fault** ★★★★☆
   - Effort: Low
   - Value: High - prevents mysterious reboots
   - Just allocate a stack and set TSS.ist2

3. **Higher-half kernel** ★★★★☆
   - Effort: Medium
   - Value: High - teaches virtual memory properly
   - Required for proper user space anyway

4. **APIC** ★★★☆☆
   - Effort: Medium
   - Value: Required for multi-core
   - Can defer until adding SMP

5. **Top/bottom half** ★★★☆☆
   - Effort: Medium
   - Value: Better responsiveness
   - Important when adding networking/storage

6. **syscall/sysret** ★★☆☆☆
   - Effort: Medium
   - Value: Performance (but int 0x80 works fine)
   - Optimize later

7. **KASLR** ★★☆☆☆
   - Effort: Medium
   - Value: Security
   - Production feature, not learning priority

8. **Boot protocol (Limine)** ★★☆☆☆
   - Effort: Medium (rewrite bootloader interface)
   - Value: Cleaner, but our bootloader works
   - Consider for "version 2" rewrite

---

## Quick Reference

### Memory Addresses (Higher-Half)
```
User space:    0x0000000000000000 - 0x00007FFFFFFFFFFF
Kernel space:  0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF
Direct map:    0xFFFF888000000000 + physical
Kernel text:   0xFFFFFFFF80000000 + offset
```

### Interrupt Numbers
```
0-31:    CPU exceptions
32-47:   Legacy IRQs (via PIC/APIC)
48-255:  Available for APIC, MSI, software
```

### MSRs for syscall
```
MSR_STAR   (0xC0000081): Segment selectors
MSR_LSTAR  (0xC0000082): Syscall entry point (64-bit)
MSR_SFMASK (0xC0000084): RFLAGS mask
```

### Log Levels
```
0=EMERG, 1=ALERT, 2=CRIT, 3=ERR
4=WARN,  5=NOTICE, 6=INFO, 7=DEBUG
```
