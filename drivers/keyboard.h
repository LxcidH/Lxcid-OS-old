#ifndef KEYBOARD_H
#define KEYBOARD_H

// Special key definitions
#define KEY_UP      0x101
#define KEY_DOWN    0x102
#define KEY_LEFT    0x103
#define KEY_RIGHT   0x104

void keyboard_handler(void);
void keyboard_init(void);

#endif
