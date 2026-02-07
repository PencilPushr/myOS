// kernel/main.c
// Minimal kernel that demonstrates we have control
#include "../common/bootinfo.h"

// Forward declaration so we can call from entry
static void draw_rect(struct FramebufferInfo *fb,
                      uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h,
                      uint32_t color);
static void fill_screen(struct FramebufferInfo *fb, uint32_t color);

//=============================================================================
// Kernel Entry Point - MUST BE FIRST FUNCTION
// The bootloader jumps directly to the start of the binary
//=============================================================================

void kernel_main(struct BootInfo *boot_info) {
    struct FramebufferInfo *fb = &boot_info->framebuffer;
    
    // Dark blue background
    fill_screen(fb, 0x00102040);
    
    // White border
    uint32_t border = 20;
    uint32_t white = 0x00FFFFFF;
    draw_rect(fb, border, border, fb->width - 2*border, 4, white);                    // Top
    draw_rect(fb, border, fb->height - border - 4, fb->width - 2*border, 4, white);   // Bottom
    draw_rect(fb, border, border, 4, fb->height - 2*border, white);                   // Left
    draw_rect(fb, fb->width - border - 4, border, 4, fb->height - 2*border, white);   // Right
    
    // Green "success" rectangle in center
    uint32_t cx = fb->width / 2;
    uint32_t cy = fb->height / 2;
    draw_rect(fb, cx - 100, cy - 50, 200, 100, 0x0000FF00);
    
    // Draw memory indicator bars (one green bar per usable memory region)
    uint32_t bar_x = 50;
    uint32_t bar_y = fb->height - 60;
    uint32_t bar_count = 0;
    
    for (uint32_t i = 0; i < boot_info->memory_map_count && bar_count < 30; i++) {
        if (boot_info->memory_map[i].type == MEMORY_TYPE_USABLE) {
            draw_rect(fb, bar_x + bar_count * 12, bar_y, 10, 30, 0x0000FF00);
            bar_count++;
        }
    }
    
    // Halt forever
    while (1) {
        __asm__ volatile("hlt");
    }
}

//=============================================================================
// Simple Drawing Functions (after kernel_main)
//=============================================================================

static void draw_rect(struct FramebufferInfo *fb,
                      uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h,
                      uint32_t color) {
    uint32_t *pixels = (uint32_t *)fb->base;
    uint32_t ppsl = fb->pitch / 4;  // Pixels per scan line
    
    for (uint32_t row = y; row < y + h && row < fb->height; row++) {
        for (uint32_t col = x; col < x + w && col < fb->width; col++) {
            pixels[row * ppsl + col] = color;
        }
    }
}

static void fill_screen(struct FramebufferInfo *fb, uint32_t color) {
    draw_rect(fb, 0, 0, fb->width, fb->height, color);
}
