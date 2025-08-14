#include "fat32.h"
#include "../drivers/ide.h"
#include "../drivers/terminal.h"
#include "../lib/string.h"
#include <stddef.h>
#include <stdbool.h>

#define MAX_CLUSTER_SIZE 32768 // 32 KB
static uint8_t g_cluster_buffer[MAX_CLUSTER_SIZE];

static FAT32_BootSector g_boot_sector;
static uint32_t g_fat_start_lba;
static uint32_t g_data_start_lba;
static bool g_fat_ready = false;

void fat32_init() {
    // Create a buffer for the sector
    uint8_t buffer[512];
    ide_read_sectors(0, 1, buffer); // Assuming partition starts at LBA 0

    // Copy the buffer into struct
    g_boot_sector = *(FAT32_BootSector*)buffer;

    // Check to see if it's FAT32
    if (g_boot_sector.fs_type[0] != 'F' || g_boot_sector.bytes_per_sec == 0) {
        terminal_writeerror("This is not a valid FAT32 volume!\n");
        g_fat_ready = false;
        return;
    }

    g_fat_start_lba = g_boot_sector.rsvd_sec_cnt;
    g_data_start_lba = g_fat_start_lba + (g_boot_sector.num_fats * g_boot_sector.fat_sz32);

        g_fat_ready = true;

    if(g_fat_ready) {
        terminal_printf("FAT32 init successful. Root cluster is at: %d\n", FG_MAGENTA, g_boot_sector.root_clus);
    } else {
        terminal_printf("FAT32 init failed :(\n", FG_RED);
    }
}

uint32_t cluster_to_lba(uint32_t cluster) {
    // The first 2 clusters are reserved in FAT
    // The data region starts at cluster #2
    return g_data_start_lba + (cluster - 2) * g_boot_sector.sec_per_clus;
}

void fat32_list_root_dir() {
    if(!g_fat_ready) return;

    // Find the LBA of the root dir's first cluster
    uint32_t root_dir_lba = cluster_to_lba(g_boot_sector.root_clus);

    // Read the sectors of the root directory cluster
    uint8_t buffer[g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus];
    ide_read_sectors(root_dir_lba, g_boot_sector.sec_per_clus, buffer);

    // Cast the buffer to an array of directory entries
    FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

    // Loop through entries until we find an empty one
    for (int i = 0; i < (sizeof(buffer) / sizeof(FAT32_DirectoryEntry)); i++) {
        // First byte of name being 0x00 means end of directory
        if (entries[i].name[0] == 0x00) {
            break;
        }

        // First byte being 0xE5 means entry is unused
        if (entries[i].name[0] == 0xE5) {
            continue;
        }

        // Skip long file name entries for now
        if (entries[i].attr == 0x0F) {
            continue;
        }

        // Print the readable filename
        char readable_name[13];
        fat_name_to_string(entries[i].name, readable_name);
        str_lower(readable_name);
        terminal_printf("%s\n", FG_WHITE, readable_name);
    }
}

static uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    if (!g_fat_ready) return 0x0FFFFFF8; // Return End of Chain if not ready

    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector_lba = g_fat_start_lba + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t fat_entry_offset = fat_offset % g_boot_sector.bytes_per_sec;

    // Read that sector in bytes
    uint8_t buffer[512];
    ide_read_sectors(fat_sector_lba, 1, buffer);

    // The FAT is an array of 32-bit integers. get the entry here
    uint32_t* fat_table = (uint32_t*)buffer;
    uint32_t next_cluster = fat_table[fat_entry_offset / 4];

    return next_cluster;
}

void fat32_read_file(FAT32_DirectoryEntry* entry, uint8_t* out_buffer) {
    if (!g_fat_ready || entry == NULL || out_buffer == NULL) return;

    uint32_t current_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    uint32_t file_size = entry->file_size;
    uint32_t bytes_read = 0;

    uint32_t cluster_size_bytes = g_boot_sector.sec_per_clus * g_boot_sector.bytes_per_sec;

    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        // Conver the cluster number to a sector address
        uint32_t lba = cluster_to_lba(current_cluster);

        // Read the entire cluster into the output buffer, at the correct offset
        ide_read_sectors(lba, g_boot_sector.sec_per_clus, out_buffer + bytes_read);

        bytes_read += cluster_size_bytes;

        // Find the next cluster in the chain by reading the FAT
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

void to_fat32_filename(const char* filename, char* out_name) {
    // Clear the output buffer with spaces
    for (int i = 0; i < 11; i++) {
        out_name[i] = ' ';
    }

    int i = 0;
    // Copy the name part (before the dot)
    for (i = 0; i < 8 && filename[i] != '.' && filename[i] != '\0'; i++) {
        out_name[i] = to_upper(filename[i]);
    }

    // If there's an extension, find it
    if (filename[i] == '.') {
        i++;
        // Copy the extension part
        for (int j = 0; j < 3 && filename[i] != '\0'; i++, j++) {
            out_name[8+j] = to_upper(filename[i]);
        }
    }
}

void fat_name_to_string(const char fat_name[11], char* out_name) {
    int i;
    int j = 0;

    // Copy the name part, stopping at the first space
    for (i = 0; i < 8 && fat_name[i] != ' '; i++) {
        out_name[j++] = fat_name[i];
    }

    // Check if there is an extension (the first character is not a space)
    if (fat_name[8] != ' ') {
        out_name[j++] = '.';    // Add the dot

        // Copy the extension part, stopping at the first space
        for (i = 8; i < 11 && fat_name[i] != ' '; i++) {
            out_name[j++] = fat_name[i];
        }
    }

    // Null-terminate the final string
    out_name[j] = '\0';
}

// A static variable to hold the found entry, so we can return a stable pointer
static FAT32_DirectoryEntry g_found_entry;

FAT32_DirectoryEntry* fat32_find_entry(const char* filename) {
    if(!g_fat_ready) return NULL;

    char fat_filename[11];
    to_fat32_filename(filename, fat_filename);

    uint32_t current_cluster = g_boot_sector.root_clus;     // Start with the root dir
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;
    if (cluster_size_bytes > MAX_CLUSTER_SIZE) {
        terminal_writeerror("Cluster is too big for our buffer!\n");
        return;
    }
    while (current_cluster < 0xFFFFFF8) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        // Read the entire cluster
        uint8_t buffer[g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus];
        ide_read_sectors(cluster_lba, g_boot_sector.sec_per_clus, buffer);

        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

        // Inner loop: iterate through entries in this cluster
        for (int i = 0; i < (cluster_size_bytes / sizeof(FAT32_DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) { // End of directory
                return NULL;
            }
            if (entries[i].name[0] == 0xE5 || entries[i].attr == ATTR_LONG_FILE_NAME) {
                continue;   // Skip unused or LFN entries
            }

            // Compare the 11-byte names
            if (strncmp(fat_filename, entries[i].name, 11) == 0) {
                // Found it, copy to our static variable and return a ptr to it
                g_found_entry = entries[i];
                return &g_found_entry;
            }
        }
        // Not in this cluster, go to the next one in the chain
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return NULL;    // File not found
}
