// kernel/keyboard.c
// PS/2 Keyboard Driver
//
// The PS/2 keyboard sends scan codes when keys are pressed/released.
// We translate these to ASCII characters using a lookup table.

#include "keyboard.h"
#include "idt.h"
#include "pic.h"

//=============================================================================
// Port I/O
//=============================================================================

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

//=============================================================================
// Keyboard State
//=============================================================================

static struct KeyboardState kb_state = {
    .shift_pressed = false,
    .ctrl_pressed = false,
    .alt_pressed = false,
    .caps_lock = false
};

static struct KeyBuffer kb_buffer = {
    .read_pos = 0,
    .write_pos = 0,
    .count = 0
};

static uint8_t last_scancode = 0;

//=============================================================================
// US QWERTY Scan Code to ASCII Tables
//=============================================================================

// Normal (no shift)
static const char scancode_to_ascii[128] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',   // 0x00-0x07
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',  // 0x08-0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   // 0x10-0x17
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   // 0x18-0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   // 0x20-0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   // 0x28-0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,     // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',   // 0x40-0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x58-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0      // 0x78-0x7F
};

// With shift held
static const char scancode_to_ascii_shift[128] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',   // 0x00-0x07
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',  // 0x08-0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',   // 0x10-0x17
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',   // 0x18-0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',   // 0x20-0x27
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',   // 0x28-0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',   // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,     // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',   // 0x40-0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x58-0x5F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,     // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0      // 0x78-0x7F
};

//=============================================================================
// Buffer Operations
//=============================================================================

static void buffer_put(char c) {
    if (kb_buffer.count < KB_BUFFER_SIZE) {
        kb_buffer.buffer[kb_buffer.write_pos] = c;
        kb_buffer.write_pos = (kb_buffer.write_pos + 1) % KB_BUFFER_SIZE;
        kb_buffer.count++;
    }
}

static char buffer_get(void) {
    if (kb_buffer.count > 0) {
        char c = kb_buffer.buffer[kb_buffer.read_pos];
        kb_buffer.read_pos = (kb_buffer.read_pos + 1) % KB_BUFFER_SIZE;
        kb_buffer.count--;
        return c;
    }
    return 0;
}

//=============================================================================
// Keyboard Interrupt Handler (IRQ1 = INT 33)
//=============================================================================

static void keyboard_handler(struct InterruptFrame *frame) {
    (void)frame;
    
    // Read scan code from keyboard
    uint8_t scancode = inb(KB_DATA_PORT);
    last_scancode = scancode;
    
    // Check if this is a key release (high bit set)
    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F;  // Remove release bit
    
    // Handle modifier keys
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            kb_state.shift_pressed = !released;
            return;
        case KEY_LCTRL:
            kb_state.ctrl_pressed = !released;
            return;
        case KEY_LALT:
            kb_state.alt_pressed = !released;
            return;
        case KEY_CAPSLOCK:
            if (!released) {
                kb_state.caps_lock = !kb_state.caps_lock;
            }
            return;
    }
    
    // Only process key presses, not releases
    if (released) {
        return;
    }
    
    // Get ASCII character
    char c;
    if (kb_state.shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    // Apply caps lock to letters
    if (kb_state.caps_lock && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
    } else if (kb_state.caps_lock && c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
    }
    
    // Add to buffer if it's a valid character
    if (c != 0) {
        buffer_put(c);
    }
}

//=============================================================================
// Public Functions
//=============================================================================

void keyboard_init(void) {
    // Register interrupt handler
    idt_set_handler(33, keyboard_handler);  // IRQ1 = INT 33
    
    // Enable keyboard IRQ
    pic_unmask_irq(1);
    
    // Clear any pending data
    while (inb(KB_STATUS_PORT) & 0x01) {
        inb(KB_DATA_PORT);
    }
}

bool keyboard_has_key(void) {
    return kb_buffer.count > 0;
}

char keyboard_getchar(void) {
    // Wait for a key
    while (!keyboard_has_key()) {
        __asm__ volatile("hlt");  // Wait for interrupt
    }
    return buffer_get();
}

char keyboard_getchar_nonblock(void) {
    return buffer_get();
}

struct KeyboardState *keyboard_get_state(void) {
    return &kb_state;
}

uint8_t keyboard_get_last_scancode(void) {
    return last_scancode;
}
