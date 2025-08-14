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
        if ((unsigned char)entries[i].name[0] == 0xE5) {
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
    while (current_cluster < 0x0FFFFFF8) {
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

// Scans the FAT for the first free cluster.
// Returns the cluster number, or 0 if no free clusters are found.
uint32_t fat32_find_free_cluster() {
    if (!g_fat_ready) {
        return 0; // Return 0 for error
    }

    // Calculate how many 4-byte entries are in one sector
    uint32_t entries_per_sector = g_boot_sector.bytes_per_sec / 4;

    // 1. Outer Loop: Iterate through each sector of the FAT
    for (uint32_t i = 0; i < g_boot_sector.fat_sz32; i++) {

        // Calculate the LBA of the current FAT sector to read
        uint32_t lba = g_fat_start_lba + i;

        // 2. Read one sector of the FAT into our global buffer
        ide_read_sectors(lba, 1, g_cluster_buffer);

        // Treat the buffer as an array of cluster entries
        uint32_t* fat_entries = (uint32_t*)g_cluster_buffer;

        // 3. Inner Loop: Iterate through all entries in the loaded sector
        for (uint32_t j = 0; j < entries_per_sector; j++) {

            // 4. Check if the entry is zero (meaning the cluster is free)
            if (fat_entries[j] == 0x00000000) {

                // 5. Calculate the absolute cluster number
                uint32_t cluster_num = (i * entries_per_sector) + j;

                // Clusters 0 and 1 are reserved and cannot be used for data.
                // The first usable cluster is #2.
                if (cluster_num >= 2) {
                    return cluster_num; // Found a free, usable cluster!
                }
            }
        }
    }

    // 6. If the loops complete, no free cluster was found.
    return 0; // Indicates disk is full
}

static dir_entry_location_t find_free_directory_entry() {
    uint32_t current_cluster = g_boot_sector.root_clus;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(current_cluster);

        ide_read_sectors(lba, g_boot_sector.sec_per_clus, g_cluster_buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)g_cluster_buffer;

        for (uint32_t i = 0; i < cluster_size_bytes / sizeof(FAT32_DirectoryEntry); i++) {
            if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
                // Found a free slot!
                dir_entry_location_t loc;
                loc.is_valid = true;
                // Calculate LBA of the specific sector within the cluster
                loc.lba = lba + (i * sizeof(FAT32_DirectoryEntry)) / g_boot_sector.bytes_per_sec;
                // Calculate the offset within that sector
                loc.offset = (i * sizeof(FAT32_DirectoryEntry)) % g_boot_sector.bytes_per_sec;
                return loc;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    dir_entry_location_t invalid_loc = { .is_valid = false };
    return invalid_loc; // No free slot found
}

bool fat32_create_file(const char* filename) {
    if (!g_fat_ready) return false;

    // First, check if file already exists
    if (fat32_find_entry(filename) != NULL) {
        terminal_writeerror("File '%s' already exists.\n", filename);
        return false;
    }

    // --- Step 1: Find space ---
    dir_entry_location_t slot = find_free_directory_entry();
    if (!slot.is_valid) {
        terminal_writeerror("Directory is full.\n");
        return false;
    }

    uint32_t free_cluster = fat32_find_free_cluster();
    if (free_cluster == 0) {
        terminal_writeerror("Disk is full.\n");
        return false;
    }

    // --- Step 2: Update the FAT ---
    uint32_t fat_offset = free_cluster * 4;
    uint32_t fat_lba = g_fat_start_lba + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t fat_entry_offset = fat_offset % g_boot_sector.bytes_per_sec;

    // Read-Modify-Write the FAT sector
    ide_read_sectors(fat_lba, 1, g_cluster_buffer);
    uint32_t* fat_table = (uint32_t*)g_cluster_buffer;
    fat_table[fat_entry_offset / 4] = 0x0FFFFFFF; // Mark as End of Chain
    ide_write_sectors(fat_lba, 1, g_cluster_buffer);

    // --- Step 3: Write the directory entry ---

    // Read-Modify-Write the directory sector
    ide_read_sectors(slot.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(g_cluster_buffer + slot.offset);

    // Fill out the metadata
    to_fat32_filename(filename, new_entry->name);
    new_entry->attr = ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (free_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = free_cluster & 0xFFFF;
    // Set all date/time fields to 0 for simplicity
    new_entry->crt_time_tenth = 0;
    new_entry->crt_time = 0;
    new_entry->crt_date = 0;
    new_entry->lst_acc_date = 0;
    new_entry->wrt_time = 0;
    new_entry->wrt_date = 0;

    ide_write_sectors(slot.lba, 1, g_cluster_buffer);

    return true; // Success!
}

/**
 * Finds a file in the root directory and returns its exact location on disk.
 */
static dir_entry_location_t find_entry_location(const char* filename) {
    dir_entry_location_t invalid_loc = { .is_valid = false };
    if (!g_fat_ready) return invalid_loc;

    char fat_filename[11];
    to_fat32_filename(filename, fat_filename);

    uint32_t current_cluster = g_boot_sector.root_clus;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t cluster_lba = cluster_to_lba(current_cluster);

        // Read the entire cluster into the global buffer
        ide_read_sectors(cluster_lba, g_boot_sector.sec_per_clus, g_cluster_buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)g_cluster_buffer;

        // Loop through all entries in this cluster
        for (uint32_t i = 0; i < (cluster_size_bytes / sizeof(FAT32_DirectoryEntry)); i++) {

            // Check for end of directory
            if (entries[i].name[0] == 0x00) {
                return invalid_loc;
            }

            // Skip unused or LFN entries
            if ((unsigned char)entries[i].name[0] == 0xE5 || entries[i].attr == ATTR_LONG_FILE_NAME) {
                continue;
            }

            // Compare the 11-byte names
            if (strncmp(fat_filename, entries[i].name, 11) == 0) {
                // Found it! Now calculate its exact location.
                dir_entry_location_t loc;
                loc.is_valid = true;

                // Calculate the byte offset of this entry from the start of the cluster
                uint32_t entry_offset_in_cluster = i * sizeof(FAT32_DirectoryEntry);

                // Calculate the LBA of the specific sector within the cluster that holds the entry
                loc.lba = cluster_lba + (entry_offset_in_cluster / g_boot_sector.bytes_per_sec);

                // Calculate the offset of the entry within that specific sector
                loc.offset = entry_offset_in_cluster % g_boot_sector.bytes_per_sec;

                return loc;
            }
        }
        // Move to the next cluster in the directory's chain
        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    return invalid_loc; // File not found after checking all clusters
}

bool fat32_delete_file(const char* filename) {
    if (!g_fat_ready) return false;

    // --- Step 1: Find the file's directory entry and it's location ---
    FAT32_DirectoryEntry* entry = fat32_find_entry(filename);
    if (entry == NULL) {
        terminal_writeerror("File %s not found!\n", filename);
        return false;
    }

    dir_entry_location_t loc = find_entry_location(filename);
    if (!loc.is_valid) {
        return false;
    }

    // --- Step 2: Free the cluster chain in the FAT ---
    uint32_t current_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        uint32_t next_cluster = fat32_get_next_cluster(current_cluster);

        // Calc where the FAT entry for the current cluster is
        uint32_t fat_offset = current_cluster * 4;
        uint32_t fat_lba = g_fat_start_lba + (fat_offset / g_boot_sector.bytes_per_sec);
        uint32_t fat_entry_offset = fat_offset % g_boot_sector.bytes_per_sec;

        // Read-Modify-Write the FAT sector
        ide_read_sectors(fat_lba, 1, g_cluster_buffer);
        uint32_t* fat_table = (uint32_t*)g_cluster_buffer;
        fat_table[fat_entry_offset / 4] = 0x00000000;   // Mark cluster as free
        ide_write_sectors(fat_lba, 1, g_cluster_buffer);

        current_cluster = next_cluster;
    }

    // --- Step 3. Mark the directory entry as deleted ---

    // Read the directory sector
    ide_read_sectors(loc.lba, 1, g_cluster_buffer);

    // Get a ptr to the entry and mark its first byte
    FAT32_DirectoryEntry* entry_to_delete = (FAT32_DirectoryEntry*)(g_cluster_buffer + loc.offset);
    entry_to_delete->name[0] = 0xE5;    // Mark as deleted

    // Write the mdoified directory sector back to disk
    ide_write_sectors(loc.lba, 1, g_cluster_buffer);

    return true;
}
