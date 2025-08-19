#include "lib/syscalls.h"
#include <stddef.h>

// A simple function to get a character from the keyboard
// We will need a sys_getchar later, but for now, this works
int getchar() {
    char c;
    read(0, &c, 1); // Read 1 character from stdin (fd=0)
    return c;
}

void main(int argc, char* argv[]) {
    clear_screen();

    const char* welcome = "LxcidOS Editor -- Version 1.0";
    write(1, welcome, 28);

    int running = 1;
    while (running) {
        // 1. Draw the status bar at the bottom of the screen
        set_cursor(0, 24); // Assuming a 80x25 screen
        const char* status = "Press 'q' to quit";
        write(1, status, 17);

        // 2. Move the cursor back to a default position
        set_cursor(0, 1);

        // 3. Wait for input
        int key = getchar();

        // 4. Process input
        if (key == 'q') {
            running = 0;
        }
    }

    // On exit, clear the screen again to return to a clean shell
    clear_screen();
}
