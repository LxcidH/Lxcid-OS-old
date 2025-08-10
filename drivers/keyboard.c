#include "keyboard.h"
#include "terminal.h"
#include "../io/io.h"
#include "../shell/shell.h"
#include <stdint.h>

// --- Scancode Maps ---
// Index corresponds to the scancode received from the keyboard.

// Standard US QWERTY layout
const char scancode_map_base[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// Shifted US QWERTY layout
const char scancode_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// --- Keyboard State Variables ---
static int escape_state = 0;
static int shift_pressed = 0;

// This is our C-level keyboard interrupt handler
void keyboard_handler(void) {
    // Read the scancode from the keyboard's data port
    uint8_t scancode = inb(0x60);
    // --- Handle Key Releases ---
    if (scancode & 0x80) { // The top bit is set on key release
        scancode &= 0x7F; // Convert break code to make code
        if (scancode == 0x2A || scancode == 0x36) { // Left or Right Shift released
            shift_pressed = 0;
        }
        // We don't need to do anything else for other key releases
        return;
    }

    // --- Handle Key Presses ---

    // Handle special multi-byte sequences for arrow keys, etc.
    if (escape_state == 0) {
        if (scancode == 0xE0) { // Special key code prefix
            escape_state = 1;   // Set state to 1, not -1
        } else {
            // It's a normal key press
            if (scancode < 128) {
                char c;
                if (shift_pressed) {
                    c = scancode_map_shifted[scancode];
                } else {
                    c = scancode_map_base[scancode];
                }

                if (c != 0) {
                    shell_handle_key(c);
                } else if (scancode == 0x2A || scancode == 0x36) { // Left or Right Shift pressed
                    shift_pressed = 1;
                }
            }
        }
    } else if (escape_state == 1) {
        // We received 0xE0 last time, this is the actual key
        switch (scancode) {
            case 0x48:
                // CHANGE: Added FG_YELLOW as the color argument. KEY_UP is now the third argument.
                terminal_printf("[DEBUG: Sending KEY_UP with value 0x%x]\n", FG_YELLOW, KEY_UP);
                shell_handle_key(KEY_UP);
                 break;
            case 0x50: shell_handle_key(KEY_DOWN); break;
            case 0x4B: shell_handle_key(KEY_LEFT); break;
            case 0x4D: shell_handle_key(KEY_RIGHT); break;
        }
        escape_state = 0; // Reset state after handling
    }
}

// Initializes the keyboard driver (does nothing for now)
void keyboard_init(void) {
}
