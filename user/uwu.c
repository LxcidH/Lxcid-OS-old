#include "lib/syscalls.h"

void main(void) {
  const char* msg = "uwu :3\n";
  write(1, msg, 7);
  
}
