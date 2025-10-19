#include "lib/syscalls.h"
#include "lib/libc.h"

#define COLS            80
#define ROWS            24
#define STDOUT          1

#define KEY_BACKSPACE   0x08
#define KEY_ENTER       0x1C
#define KEY_CTRL_Q      0x11
#define KEY_ARROW_UP    0x48
#define KEY_ARROW_DOWN  0x50
#define KEY_ARROW_LEFT  0x4B
#define KEY_ARROW_RIGHT 0x4D

char buffer[ROWS][COLS];
int cursor_x = 0;
int cursor_y = 0;

void buffer_init() {
  for (int y = 0; y < ROWS; y++){
    for (int x = 0; x < COLS; x++) {
      buffer[y][x] = ' ';
    }
  }
}

void editor_redraw() {
  clear_screen();
  char line_buffer[COLS];
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      set_cursor(x, y);
      write(STDOUT, &buffer[y][x], 1);
    }
  }
  set_cursor(cursor_x, cursor_y);
}


void editor_process_key(int key) {
    switch (key) {
        case KEY_CTRL_Q:
            clear_screen();
            sys_exit();
            break;

        case KEY_ARROW_UP:
            if (cursor_y > 0) cursor_y--;
            break;
        case KEY_ARROW_DOWN:
            if (cursor_y < ROWS - 1) cursor_y++;
            break;
        case KEY_ARROW_LEFT:
            if (cursor_x > 0) cursor_x--;
            break;
        case KEY_ARROW_RIGHT:
            if (cursor_x < COLS - 1) cursor_x++;
            break;

        case KEY_BACKSPACE:
            if (cursor_x > 0) {
                cursor_x--;
                buffer[cursor_y][cursor_x] = ' ';
            }
            break;

        default:
            // Check for printable ASCII characters
            if (key >= ' ' && key <= '~') {
                if (cursor_x < COLS) {
                    buffer[cursor_y][cursor_x] = (char)key;
                    cursor_x++;
                }
            }
            break;
    }

    // Ensure cursor never goes out of bounds
    if (cursor_x >= COLS) cursor_x = COLS - 1;
    if (cursor_y >= ROWS) cursor_y = ROWS - 1;
}


void main(void) {
  buffer_init();

  while (1) {
    editor_redraw();
    write(1, "uwu", 3);
    int key = get_key();
    editor_process_key(key);
  }
}
