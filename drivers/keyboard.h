#ifndef KEYBOARD_H
#define KEYBOARD_H

// Special key definitions
#define KEY_UP      0x101
#define KEY_DOWN    0x102
#define KEY_LEFT    0x103
#define KEY_RIGHT   0x104
#define KEY_CTRL_S 0x13 // Example: Ctrl+'S'
#define KEY_CTRL_Q 0x10 // Example: Ctrl+'Q'
#define KEY_BACKSPACE 0x0E
#define KEY_TAB '\t'
void keyboard_handler(void);
// Initializes the keyboard driver.
void keyboard_init(void);

// Waits for and returns a single character from the keyboard buffer.
char keyboard_getc(void);
#endif
