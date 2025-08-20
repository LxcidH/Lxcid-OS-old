#include "keyboard.h"
#include "terminal.h"
#include "../io/io.h"
#include "../shell/shell.h"
#include <stdint.h>

// --- I/O Ports ---
#define KBD_STATUS_PORT 0x64
#define KBD_DATA_PORT   0x60

// --- Scancode Maps (US QWERTY) ---
const char scancode_map_base[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const char scancode_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// --- Keyboard State ---
static int escape_state = 0;
static int shift_pressed = 0;

// --- C-Level Interrupt Handler with Corrected Logic ---
void keyboard_handler(void) {
    uint8_t scancode = inb(KBD_DATA_PORT);

    // 1. Check for the special 0xE0 prefix (for arrow keys, etc.)
    if (scancode == 0xE0) {
        escape_state = 1;
        return; // Wait for the next byte.
    }

    // 2. Handle the second byte of an escape sequence.
    if (escape_state == 1) {
        switch (scancode) {
            case 0x48: shell_handle_key(KEY_UP); break;
            case 0x50: shell_handle_key(KEY_DOWN); break;
            case 0x4B: shell_handle_key(KEY_LEFT); break;
            case 0x4D: shell_handle_key(KEY_RIGHT); break;
        }
        escape_state = 0; // Reset state.
        return;
    }

    // 3. Ignore key releases, except for shift.
    if (scancode & 0x80) {
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) { // L/R Shift released
            shift_pressed = 0;
        }
        return;
    }

    // 4. Handle normal key presses.
    if (scancode < 128) {
        if (scancode == 0x2A || scancode == 0x36) { // L/R Shift pressed
            shift_pressed = 1;
        } else if (scancode == 0x0F) { // --- THIS IS THE NEW PART --- Tab key pressed
            shell_handle_key(KEY_TAB);
        } else {
            char c = shift_pressed ? scancode_map_shifted[scancode] : scancode_map_base[scancode];
            if (c != 0) {
                shell_handle_key(c);
            }
        }
    }
}


// --- Helper functions for PS/2 Controller ---
static void kbd_wait_input() {
    while (inb(KBD_STATUS_PORT) & 0x02);
}
static void kbd_wait_output() {
    while (!(inb(KBD_STATUS_PORT) & 0x01));
}

// --- Keyboard Initialization ---
void keyboard_init(void) {
    kbd_wait_input();
    outb(KBD_STATUS_PORT, 0x20);
    kbd_wait_output();
    uint8_t ccb = inb(KBD_DATA_PORT);
    ccb |= (1 << 6) | (1 << 0);
    kbd_wait_input();
    outb(KBD_STATUS_PORT, 0x60);
    kbd_wait_input();
    outb(KBD_DATA_PORT, ccb);
    while(inb(KBD_STATUS_PORT) & 0x01) {
        inb(KBD_DATA_PORT);
    }
}
