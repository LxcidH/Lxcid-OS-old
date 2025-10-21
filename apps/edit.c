#include "../drivers/terminal.h"
#include "../shell/shell.h"
#include "../fs/fat32.h"

void editor_init(FAT32_DirectoryEntry* fileName) {
  if (fileName == NULL) {
    uint32_t g_current_dir_cluster = shell_getCurrentDirCluster();
    dir_entry_location_t new_file_loc;
    if (!fat32_create_file(fileName, g_current_dir_cluster, &new_file_loc)) {
      shell_init();
    }
  }

  

  terminal_init();
  terminal_printf("---------------------------------- lxc v1.0.0 ----------------------------------\n", FG_MAGENTA);
}

void editor_handle_key(int key) {
  
}

