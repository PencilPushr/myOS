// kernel/keyboard.h
// PS/2 Keyboard Driver
#pragma once

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Keyboard Ports
//=============================================================================

#define KB_DATA_PORT    0x60    // Read scan codes
#define KB_STATUS_PORT  0x64    // Read status / write commands

//=============================================================================
// Special Keys
//=============================================================================

#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_F11         0x57
#define KEY_F12         0x58

//=============================================================================
// Keyboard State
//=============================================================================

struct KeyboardState {
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
    bool caps_lock;
};

//=============================================================================
// Circular Buffer for Key Input
//=============================================================================

#define KB_BUFFER_SIZE 256

struct KeyBuffer {
    char buffer[KB_BUFFER_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
};

//=============================================================================
// Functions
//=============================================================================

// Initialize keyboard driver
void keyboard_init(void);

// Check if a key is available
bool keyboard_has_key(void);

// Get next key from buffer (blocking)
char keyboard_getchar(void);

// Get next key from buffer (non-blocking, returns 0 if empty)
char keyboard_getchar_nonblock(void);

// Get current keyboard state
struct KeyboardState *keyboard_get_state(void);

// Get the last scan code (for debugging)
uint8_t keyboard_get_last_scancode(void);
