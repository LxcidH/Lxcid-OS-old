#include "keyboard.h"
#include "terminal.h"
#include "../io/io.h"
#include "../shell/shell.h"
#include <stdint.h>

// US QWERTY LAYOUT scancode map
const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// This is our C-level keyboard interrupt handler
void keyboard_handler(void) {
    // Read the scancode from the keyboard's data port
    uint32_t scancode = inb(0x60);

    // A scancode is a "key press" if the highest bit is 0
    // If the highest bit is 1, it's a "key release", which we can ignore for now
    if (scancode < 128) {
        char c = scancode_map[scancode];
        if (c != 0) {
            shell_handle_key(c);
        }
    }
}

// initializes the keyboard driver (does nothing for now)
void keyboard_init(void) {

}
