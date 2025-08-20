#include "fat32.h"
#include "../drivers/ide.h"
#include "../drivers/terminal.h"
#include "../lib/string.h"
#include "../memory/heap.h"
#include <stddef.h>
#include <stdbool.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
uint32_t fat32_read_directory(uint32_t cluster, FAT32_DirectoryEntry entries[], uint32_t max_entries);

// A struct to hold cached filesystem information for easy access.
typedef struct {
    uint32_t root_cluster_num;
    uint32_t first_data_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sec;
} FAT32_FSInfo;

// --- Global variables for the FAT32 driver ---
// These are no longer static so all functions in this file can access them.
FAT32_BootSector g_boot_sector;
FAT32_FSInfo g_fat32_fs_info;
bool g_fat_ready = false;

// A reusable buffer for reading single clusters safely (avoids stack overflow)
#define MAX_CLUSTER_SIZE 32768 // 32 KB
uint8_t g_cluster_buffer[MAX_CLUSTER_SIZE];

// It's crucial to define the End of Chain (EOC) marker for FAT32.
#define FAT32_EOC_MARK 0x0FFFFFFF

// --- Forward declarations for static helper functions ---
static void fat32_read_cluster(uint32_t cluster_num, void* buffer);
static uint32_t cluster_to_lba(uint32_t cluster);
static uint32_t fat32_get_next_cluster(uint32_t current_cluster);
static void fat32_set_fat_entry(uint32_t cluster_num, uint32_t value);
static uint32_t fat32_find_free_cluster();
static dir_entry_location_t find_free_directory_entry(uint32_t start_cluster);
static void to_fat32_filename(const char* filename, char* out_name);
bool fat32_write_file(FAT32_DirectoryEntry* entry, const void* buffer, uint32_t size);
void fat32_read_file(FAT32_DirectoryEntry* entry, void* buffer);
uint32_t fat32_get_fat_entry(uint32_t cluster);
void fat32_free_cluster_chain(uint32_t start_cluster);

// A constant for the number of entries per sector
#define ENTRIES_PER_SECTOR (g_bytes_per_sec / sizeof(FAT32_DirectoryEntry))


// --- Public API Functions ---

void fat32_init() {
    ide_read_sectors(0, 1, &g_boot_sector);

    if (g_boot_sector.bytes_per_sec == 0) {
        terminal_printf("Error: Invalid FAT32 volume.\n", FG_RED);
        g_fat_ready = false;
        return;
    }

    // Populate the FSInfo struct for easy access later
    g_fat32_fs_info.root_cluster_num = g_boot_sector.root_clus;
    g_fat32_fs_info.sectors_per_cluster = g_boot_sector.sec_per_clus;
    g_fat32_fs_info.bytes_per_sec = g_boot_sector.bytes_per_sec;

    uint32_t fat_start_sector = g_boot_sector.rsvd_sec_cnt;
    uint32_t fat_size_sectors = g_boot_sector.fat_sz32 * g_boot_sector.num_fats;
    g_fat32_fs_info.first_data_sector = fat_start_sector + fat_size_sectors;

    g_fat_ready = true;
}

uint32_t fat32_get_root_cluster() {
    return g_fat32_fs_info.root_cluster_num;
}

void fat32_list_dir(uint32_t start_cluster) {
    if (!g_fat_ready) return;

    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size_bytes = g_fat32_fs_info.bytes_per_sec * g_fat32_fs_info.sectors_per_cluster;
    if (cluster_size_bytes > MAX_CLUSTER_SIZE) return;

    while (current_cluster < 0x0FFFFFF8) {
        fat32_read_cluster(current_cluster, g_cluster_buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)g_cluster_buffer;

        for (uint32_t i = 0; i < (cluster_size_bytes / sizeof(FAT32_DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) return; // End of directory
            if ((uint8_t)entries[i].name[0] == 0xE5 || entries[i].attr == ATTR_LONG_FILE_NAME) continue;

            char readable_name[13];
            fat_name_to_string(entries[i].name, readable_name);

            if (entries[i].attr & ATTR_DIRECTORY) {
                if(strcmp(readable_name, ".") != 0 && strcmp(readable_name, "..") != 0) {
                    terminal_printf("<DIR>  %s\n", FG_WHITE, readable_name);
                }
            } else {
                terminal_printf("       %s\n", FG_WHITE, readable_name);
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

// Corrected to use void* for flexibility
void fat32_read_file(FAT32_DirectoryEntry* entry, void* buffer) {
    if (!g_fat_ready || entry == NULL || buffer == NULL) return;

    uint8_t* out_buffer = (uint8_t*)buffer;
    uint32_t current_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    uint32_t file_size = entry->file_size;
    uint32_t bytes_read = 0;
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;

    // Use a temporary buffer to read full clusters from disk
    static uint8_t temp_cluster_buffer[MAX_CLUSTER_SIZE];
     if (cluster_size_bytes > MAX_CLUSTER_SIZE) {
        // Handle error: cluster size is larger than our buffer
        return;
    }

    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        // Read the full cluster from disk into a temporary buffer
        fat32_read_cluster(current_cluster, temp_cluster_buffer);

        // Determine how many bytes to actually copy from this cluster
        uint32_t bytes_to_copy = file_size - bytes_read;
        if (bytes_to_copy > cluster_size_bytes) {
            bytes_to_copy = cluster_size_bytes;
        }

        // Copy only the necessary bytes to the final buffer
        memcpy(out_buffer + bytes_read, temp_cluster_buffer, bytes_to_copy);

        bytes_read += bytes_to_copy;
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
}

FAT32_DirectoryEntry* fat32_find_entry(const char* filename, uint32_t start_cluster) {
    static FAT32_DirectoryEntry found_entry;
    if (fat32_find_entry_by_name(filename, start_cluster, NULL, &found_entry)) {
        return &found_entry;
    }
    return NULL;
}

FAT32_DirectoryEntry* fat32_find_entry_by_cluster(uint32_t cluster_to_find) {
    static FAT32_DirectoryEntry found_entry;
    uint32_t dir_cluster = g_fat32_fs_info.root_cluster_num;

    while (dir_cluster < 0x0FFFFFF8) {
        fat32_read_cluster(dir_cluster, g_cluster_buffer);

        FAT32_DirectoryEntry* entry = (FAT32_DirectoryEntry*)g_cluster_buffer;
        uint32_t entries_per_cluster = (g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec) / sizeof(FAT32_DirectoryEntry);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entry[i].name[0] == 0x00) return NULL;
            if ((uint8_t)entry[i].name[0] == 0xE5) continue;
            if (entry[i].attr == ATTR_LONG_FILE_NAME) continue;

            uint32_t entry_cluster = (entry[i].fst_clus_hi << 16) | entry[i].fst_clus_lo;
            if (entry_cluster == cluster_to_find) {
                found_entry = entry[i];
                return &found_entry;
            }
        }
        dir_cluster = fat32_get_next_cluster(dir_cluster);
    }
    return NULL;
}


// --- Static Helper Functions ---

static void fat32_read_cluster(uint32_t cluster_num, void* buffer) {
    uint32_t lba = cluster_to_lba(cluster_num);
    ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, buffer);
}

static uint32_t cluster_to_lba(uint32_t cluster) {
    return g_fat32_fs_info.first_data_sector + (cluster - 2) * g_fat32_fs_info.sectors_per_cluster;
}

static uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_fat32_fs_info.bytes_per_sec);
    uint32_t ent_offset = fat_offset % g_fat32_fs_info.bytes_per_sec;

    // A small static buffer for reading single FAT sectors
    static uint8_t fat_sector_buffer[512];
    ide_read_sectors(fat_sector, 1, fat_sector_buffer);

    return (*(uint32_t*)&fat_sector_buffer[ent_offset]) & 0x0FFFFFFF;
}

// ... the rest of your static helper functions (fat32_create_file, etc.) go here ...
// (The code you provided for these functions is fine and doesn't need to be changed)

bool fat32_create_file(const char* filename, uint32_t parent_cluster, dir_entry_location_t* out_loc) {
    if (fat32_find_entry_by_name(filename, parent_cluster, NULL, NULL)) {
        terminal_printf("Error: File '%s' already exists.\n", FG_RED, filename);
        return false;
    }

    dir_entry_location_t slot = find_free_directory_entry(parent_cluster);
    if (!slot.is_valid) {
        terminal_printf("Error: Directory is full.\n", FG_RED);
        return false;
    }

    // --- THIS IS THE FIX ---
    // The original code had `out_loc = &slot;` which only changes the local pointer.
    // This version correctly copies the contents of the `slot` struct to the
    // memory location pointed to by `out_loc`, making it available to the caller.
    if (out_loc) {
        *out_loc = slot;
    }

    // For a new file, we don't need to allocate a cluster until data is written.
    // A starting cluster of 0 is standard for a zero-byte file.
    uint32_t initial_cluster = 0;

    ide_read_sectors(slot.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(g_cluster_buffer + slot.offset);

    to_fat32_filename(filename, new_entry->name);
    new_entry->attr = ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (initial_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = initial_cluster & 0xFFFF;
    new_entry->crt_time_tenth = 0; new_entry->crt_time = 0; new_entry->crt_date = 0;
    new_entry->lst_acc_date = 0; new_entry->wrt_time = 0; new_entry->wrt_date = 0;

    ide_write_sectors(slot.lba, 1, g_cluster_buffer);

    return true;
}

bool fat32_delete_file(const char* filename, uint32_t parent_cluster) {
    dir_entry_location_t loc;
    FAT32_DirectoryEntry entry;
    if (!fat32_find_entry_by_name(filename, parent_cluster, &loc, &entry)) {
        terminal_printf("Error: File '%s' not found.\n", FG_RED, filename);
        return false;
    }
    if (entry.attr & ATTR_DIRECTORY) {
        terminal_printf("Error: '%s' is a directory.\n", FG_RED, filename);
        return false;
    }

    uint32_t current_cluster = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
        uint32_t next_cluster = fat32_get_next_cluster(current_cluster);
        fat32_set_fat_entry(current_cluster, 0x00000000);
        current_cluster = next_cluster;
    }

    ide_read_sectors(loc.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* entry_to_delete = (FAT32_DirectoryEntry*)(g_cluster_buffer + loc.offset);
    entry_to_delete->name[0] = 0xE5;
    ide_write_sectors(loc.lba, 1, g_cluster_buffer);

    return true;
}

/**
 * @brief Finds a directory entry by name within a given directory cluster chain.
 *
 * @param filename The 8.3 filename to search for.
 * @param start_cluster The starting cluster of the directory to search in.
 * @param out_loc If not NULL, this will be filled with the disk location of the found entry.
 * @param out_entry If not NULL, this will be filled with the data of the found entry.
 * @return true if the entry was found, false otherwise.
 */
bool fat32_find_entry_by_name(const char* filename, uint32_t start_cluster, dir_entry_location_t* out_loc, FAT32_DirectoryEntry* out_entry) {
    if (!g_fat_ready) return false;

    char fat_filename[11];
    to_fat32_filename(filename, fat_filename);

    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;
    if (cluster_size_bytes > MAX_CLUSTER_SIZE) return false;

    while (current_cluster < 0x0FFFFFF8) {
        fat32_read_cluster(current_cluster, g_cluster_buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)g_cluster_buffer;

        for (uint32_t i = 0; i < (cluster_size_bytes / sizeof(FAT32_DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00) return false; // End of directory
            if ((unsigned char)entries[i].name[0] == 0xE5 || entries[i].attr == ATTR_LONG_FILE_NAME) continue;

            if (strncmp(fat_filename, entries[i].name, 11) == 0) {
                if (out_entry) *out_entry = entries[i];
                if (out_loc) {
                    out_loc->is_valid = true;
                    uint32_t entry_offset = i * sizeof(FAT32_DirectoryEntry);
                    out_loc->lba = cluster_to_lba(current_cluster) + (entry_offset / g_boot_sector.bytes_per_sec);
                    out_loc->offset = entry_offset % g_boot_sector.bytes_per_sec;
                }
                return true;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return false;
}

static dir_entry_location_t find_free_directory_entry(uint32_t start_cluster) {
    dir_entry_location_t invalid_loc = { .is_valid = false };
    if (!g_fat_ready) return invalid_loc;

    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;
    if (cluster_size_bytes > MAX_CLUSTER_SIZE) return invalid_loc;

    while (current_cluster < 0x0FFFFFF8) {
        fat32_read_cluster(current_cluster, g_cluster_buffer);
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)g_cluster_buffer;
        for (uint32_t i = 0; i < (cluster_size_bytes / sizeof(FAT32_DirectoryEntry)); i++) {
            if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
                dir_entry_location_t loc;
                loc.is_valid = true;
                uint32_t entry_offset_in_cluster = i * sizeof(FAT32_DirectoryEntry);
                loc.lba = cluster_to_lba(current_cluster) + (entry_offset_in_cluster / g_boot_sector.bytes_per_sec);
                loc.offset = entry_offset_in_cluster % g_boot_sector.bytes_per_sec;
                return loc;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    return invalid_loc;
}

static void fat32_set_fat_entry(uint32_t cluster_num, uint32_t value) {
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_lba = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t fat_entry_offset = fat_offset % g_boot_sector.bytes_per_sec;

    static uint8_t sector_buffer[512];
    ide_read_sectors(fat_lba, 1, sector_buffer);

    uint32_t* fat_table = (uint32_t*)sector_buffer;
    fat_table[fat_entry_offset / 4] = value;
    ide_write_sectors(fat_lba, 1, sector_buffer);
}

static uint32_t fat32_find_free_cluster() {
    if (!g_fat_ready) return 0;
    uint32_t entries_per_sector = g_boot_sector.bytes_per_sec / 4;
    for (uint32_t i = 0; i < g_boot_sector.fat_sz32; i++) {
        uint32_t lba = g_boot_sector.rsvd_sec_cnt + i;
        ide_read_sectors(lba, 1, g_cluster_buffer);
        uint32_t* fat_entries = (uint32_t*)g_cluster_buffer;
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            if (fat_entries[j] == 0x00000000) {
                uint32_t cluster_num = (i * entries_per_sector) + j;
                if (cluster_num >= 2) return cluster_num;
            }
        }
    }
    return 0; // Disk full
}

static void to_fat32_filename(const char* filename, char* out_name) {
    if (strcmp(filename, ".") == 0) {
        memcpy(out_name, ".          ", 11);
        return;
    }
    if (strcmp(filename, "..") == 0) {
        memcpy(out_name, "..         ", 11);
        return;
    }

    memset(out_name, ' ', 11);
    int i = 0;
    for (i = 0; i < 8 && filename[i] != '.' && filename[i] != '\0'; i++) {
        out_name[i] = to_upper(filename[i]);
    }
    if (filename[i] == '.') {
        i++;
        for (int j = 0; j < 3 && filename[i] != '\0'; i++, j++) {
            out_name[8 + j] = to_upper(filename[i]);
        }
    }
}

void fat_name_to_string(const char fat_name[11], char* out_name) {
    int i, j = 0;
    for (i = 0; i < 8 && fat_name[i] != ' '; i++) {
        out_name[j++] = fat_name[i];
    }
    if (fat_name[8] != ' ') {
        out_name[j++] = '.';
        for (i = 8; i < 11 && fat_name[i] != ' '; i++) {
            out_name[j++] = fat_name[i];
        }
    }
    out_name[j] = '\0';
}

/* bool fat32_create_file(const char* filename, uint32_t parent_cluster, dir_entry_location_t* out_loc) {
    if (fat32_find_entry_by_name(filename, parent_cluster, NULL, NULL)) {
        terminal_printf("Error: File '%s' already exists.\n", FG_RED, filename);
        return false;
    }

    dir_entry_location_t slot = find_free_directory_entry(parent_cluster);
    if (!slot.is_valid) {
        terminal_printf("Error: Directory is full.\n", FG_RED);
        return false;
    }

    uint32_t free_cluster = fat32_find_free_cluster();
    if (free_cluster == 0) {
        terminal_printf("Error: Disk is full.\n", FG_RED);
        return false;
    }

// Set the FAT entry for the new cluster to zero (don't mark as end-of-chain)
    fat32_set_fat_entry(free_cluster, 0x00000000); // Leave it free

    // Read the sector with the free slot.
    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if (!sector_buffer) return false;
    ide_read_sectors(slot.lba, 1, sector_buffer);

    // Create the new entry in the buffer.
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(sector_buffer + slot.offset);
    to_fat32_filename(filename, new_entry->name);
    new_entry->attr = ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (free_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = free_cluster & 0xFFFF;
    // Set creation/write times, etc., to 0 for simplicity.
    new_entry->crt_time_tenth = 0;
    new_entry->crt_time = 0;
    new_entry->crt_date = 0;
    new_entry->lst_acc_date = 0;
    new_entry->wrt_time = 0;
    new_entry->wrt_date = 0;

    // Write the modified sector back to disk.

    ide_write_sectors(slot.lba, 1, sector_buffer);
    bool success = true;
    free(sector_buffer);

    // Return the location of the newly created entry.
    if (success && out_loc) {
        *out_loc = slot;
    }

    return success;
} */

bool fat32_delete_directory(const char* dirname, uint32_t parent_cluster) {
    if (!g_fat_ready) return false;

    dir_entry_location_t loc;
    FAT32_DirectoryEntry entry;
    if (!fat32_find_entry_by_name(dirname, parent_cluster, &loc, &entry)) {
        terminal_printf("Error: Directory '%s' not found.\n", FG_RED, dirname);
        return false;
    }
    if (!(entry.attr & ATTR_DIRECTORY)) {
        terminal_printf("Error: '%s' is not a directory.\n", FG_RED, dirname);
        return false;
    }

    uint32_t dir_cluster = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    fat32_read_cluster(dir_cluster, g_cluster_buffer);
    FAT32_DirectoryEntry* dir_entries = (FAT32_DirectoryEntry*)g_cluster_buffer;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;

    for (uint32_t i = 0; i < cluster_size_bytes / sizeof(FAT32_DirectoryEntry); i++) {
        if (dir_entries[i].name[0] == 0x00) break;
        if ((unsigned char)dir_entries[i].name[0] == 0xE5 || dir_entries[i].attr == ATTR_LONG_FILE_NAME) continue;

        if (strncmp(dir_entries[i].name, ".          ", 11) != 0 && strncmp(dir_entries[i].name, "..         ", 11) != 0) {
            terminal_printf("Error: Directory '%s' is not empty.\n", FG_RED, dirname);
            return false;
        }
    }

    fat32_set_fat_entry(dir_cluster, 0x00000000);

    ide_read_sectors(loc.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* entry_to_delete = (FAT32_DirectoryEntry*)(g_cluster_buffer + loc.offset);
    entry_to_delete->name[0] = 0xE5;
    ide_write_sectors(loc.lba, 1, g_cluster_buffer);

    return true;
}

/**
 * @brief Reads all directory entries from a given cluster into a buffer.
 *
 * @param cluster The starting cluster of the directory.
 * @param entries An array to store the read directory entries.
 * @param max_entries The maximum number of entries the array can hold.
 * @return The number of entries successfully read.
 */
uint32_t fat32_get_parent_cluster(uint32_t cluster) {
    // If we're already at the root, the parent is the root.
    if (cluster == g_fat32_fs_info.root_cluster_num) {
        return g_fat32_fs_info.root_cluster_num;
    }

    // Allocate a temporary buffer for one cluster
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;
    uint8_t* buffer = malloc(cluster_size_bytes);
    if (buffer == NULL) {
        return 0; // Or an error code
    }

    // Read the current directory's cluster
    uint32_t start_sector = g_fat32_fs_info.first_data_sector + (cluster - 2) * g_fat32_fs_info.sectors_per_cluster;
    ide_read_sectors(start_sector, g_fat32_fs_info.sectors_per_cluster, buffer);

    // Get the directory entries
    FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

    // Check the first entry ('.')
    if (strncmp(entries[0].name, ".          ", 11) != 0 || !(entries[0].attr & ATTR_DIRECTORY)) {
        // This is not a valid directory, something is wrong
        free(buffer);
        return 0;
    }

    // The '..' entry is always the second one
    FAT32_DirectoryEntry* dotdot_entry = &entries[1];

    if (strncmp(dotdot_entry->name, "..         ", 11) != 0 || !(dotdot_entry->attr & ATTR_DIRECTORY)) {
        // The second entry is not the '..' directory, return an error
        free(buffer);
        return 0;
    }

    // Extract and return the parent cluster number
    uint32_t parent_cluster = (dotdot_entry->fst_clus_hi << 16) | dotdot_entry->fst_clus_lo;

    // The root directory's '..' entry's cluster is 0. If we read that,
    // we should return the real root cluster instead.
    if (parent_cluster == 0) {
        parent_cluster = g_fat32_fs_info.root_cluster_num;
    }

    free(buffer);
    return parent_cluster;
}

/**
 * @brief Writes or overwrites a file with new content.
 *
 * This function handles writing data to a FAT32 file. It addresses several key aspects:
 * 1.  **Existing Files**: If the file already has data (an existing cluster chain), this function
 * first deallocates the entire chain to prevent orphaned clusters.
 * 2.  **Zero-Sized Files**: Correctly handles the creation of an empty file by setting its size
 * to 0 and allocating no clusters.
 * 3.  **Cluster Allocation**: Allocates new clusters as needed to store the file content.
 * 4.  **Data Writing**: Writes the data to the allocated clusters. It uses a temporary buffer
 * to handle cases where the data does not perfectly align with cluster sizes, ensuring
 * that full sectors are always written to the disk.
 * 5.  **FAT Chaining**: Correctly links the clusters in the File Allocation Table (FAT).
 * 6.  **Atomic Update**: Updates the directory entry in memory and assumes the caller will
 * write it back to disk. This makes the operation safer; the directory entry is only
 * updated if the write succeeds.
 *
 * @param entry A pointer to the directory entry for the file. This entry will be modified.
 * @param buffer A pointer to the data that needs to be written.
 * @param size The total size of the data in bytes.
 * @return true if the file was written successfully, false otherwise.
 */
bool fat32_write_file(FAT32_DirectoryEntry* entry, const void* buffer, uint32_t size) {
    // Combine the high and low words to get the starting cluster of the existing file.
    uint32_t existing_cluster = ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;

    // --- 1. Deallocate any existing cluster chain for the file ---
    // If the file already has clusters allocated, we must free them before writing new data.
    if (existing_cluster != 0) {
        fat32_free_cluster_chain(existing_cluster);
    }

    // --- 2. Handle zero-size files as a special case ---
    // If the new size is 0, we just update the directory entry and we're done. No data clusters are needed.
    if (size == 0) {
        entry->fst_clus_hi = 0;
        entry->fst_clus_lo = 0;
        entry->file_size = 0;
        // The caller is responsible for writing the updated directory entry back to disk.
        return true;
    }

    // --- 3. Allocate the first cluster for the new file content ---
    uint32_t first_cluster = fat32_find_free_cluster();
    if (first_cluster == 0) {
        // No free clusters available on the disk.
        return false;
    }
    // Mark this cluster as allocated in the FAT.
    fat32_set_fat_entry(first_cluster, FAT32_EOC_MARK); // Mark as end of chain for now.

    // Update the directory entry in memory with the start cluster and final size.
    // This won't be committed to disk until the write is successful.
    entry->fst_clus_hi = (first_cluster >> 16) & 0xFFFF;
    entry->fst_clus_lo = first_cluster & 0xFFFF;
    entry->file_size = size;

    // --- 4. Write data to the cluster chain ---
    uint32_t current_cluster = first_cluster;
    uint32_t bytes_written = 0;
    const uint8_t* data_ptr = (const uint8_t*)buffer;
    const uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;

    // A temporary buffer is crucial for handling writes that are not cluster-aligned.
    // Using a static buffer to avoid potential stack overflow with large cluster sizes.
    static uint8_t cluster_buffer[MAX_CLUSTER_SIZE];
    if (cluster_size_bytes > MAX_CLUSTER_SIZE) {
        // Handle error: cluster size is larger than our buffer
        return false;
    }


    while (bytes_written < size) {
        // Determine how many bytes to write in this iteration.
        uint32_t bytes_to_write = size - bytes_written;
        if (bytes_to_write > cluster_size_bytes) {
            bytes_to_write = cluster_size_bytes;
        }

        // Copy the data chunk into our temporary cluster buffer.
        memcpy(cluster_buffer, data_ptr + bytes_written, bytes_to_write);

        // If the data for this cluster is less than the cluster size (i.e., it's the last chunk),
        // zero out the rest of the buffer to avoid writing garbage data to the disk.
        if (bytes_to_write < cluster_size_bytes) {
            memset(cluster_buffer + bytes_to_write, 0, cluster_size_bytes - bytes_to_write);
        }

        // Convert cluster number to its Linear Block Address (LBA) and write the entire cluster.
        uint32_t lba = cluster_to_lba(current_cluster);

        // The compiler indicated that 'ide_write_sectors' is a void function, so we can't check its return value.
        // We will assume the write is successful. Proper error handling would require modifying ide_write_sectors.
        ide_write_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);

        bytes_written += bytes_to_write;

        // If there's still more data to write, allocate and link the next cluster.
        if (bytes_written < size) {
            uint32_t next_cluster = fat32_find_free_cluster();
            if (next_cluster == 0) {
                // Ran out of space mid-write. This is a critical error.
                // We've written a partial file. We should free the chain we've created.
                fat32_free_cluster_chain(first_cluster);
                // Reset the directory entry to show a zero-length file, as the write failed.
                entry->fst_clus_hi = 0;
                entry->fst_clus_lo = 0;
                entry->file_size = 0;
                return false;
            }

            // Link the current cluster to the new one in the FAT.
            fat32_set_fat_entry(current_cluster, next_cluster);
            // Mark the new cluster as the end of the chain for now.
            fat32_set_fat_entry(next_cluster, FAT32_EOC_MARK);
            current_cluster = next_cluster;
        }
    }

    // The write was successful. The caller is responsible for persisting the updated 'entry'.
    return true;
}

/**
 * @brief Deallocates a chain of clusters in the FAT.
 *
 * @param start_cluster The first cluster in the chain to free.
 */
void fat32_free_cluster_chain(uint32_t start_cluster) {
    uint32_t current_cluster = start_cluster;
    while (current_cluster < FAT32_EOC_MARK) {
        uint32_t next_cluster = fat32_get_fat_entry(current_cluster);
        fat32_set_fat_entry(current_cluster, 0); // Mark cluster as free
        current_cluster = next_cluster;
    }
}

/**
 * @brief Copies a file from one directory to another.
 * * @param source_name The name of the file to copy.
 * @param source_dir_cluster The cluster of the source directory.
 * @param dest_name The name of the new file.
 * @param dest_dir_cluster The cluster of the destination directory.
 * @return true if the file was copied successfully, false otherwise.
 */
bool fat32_copy_file(const char* source_name, uint32_t source_dir_cluster, const char* dest_name, uint32_t dest_dir_cluster) {
    FAT32_DirectoryEntry source_entry;
    dir_entry_location_t dest_loc;

    // Step 1: Find and validate the source file.
    if (!fat32_find_entry_by_name(source_name, source_dir_cluster, NULL, &source_entry)) {
        terminal_printf("Error: Source file '%s' not found.\n", FG_RED, source_name);
        return false;
    }

    if (source_entry.attr & ATTR_DIRECTORY) {
        terminal_printf("Error: Cannot copy a directory.\n", FG_RED);
        return false;
    }

    // Step 2: Check if destination file already exists.
    if (fat32_find_entry_by_name(dest_name, dest_dir_cluster, NULL, NULL)) {
        terminal_printf("Error: Destination file '%s' already exists.\n", FG_RED, dest_name);
        return false;
    }

    // Step 3: Allocate a buffer for file content and read source file.
    uint8_t* file_buffer = malloc(source_entry.file_size);
    if (file_buffer == NULL) {
        terminal_printf("Error: Memory allocation failed.\n", FG_RED);
        return false;
    }
    fat32_read_file(&source_entry, file_buffer);

    // Step 4: Create a new file for the destination and get its disk location.
    if (!fat32_create_file(dest_name, dest_dir_cluster, &dest_loc)) {
        free(file_buffer);
        return false;
    }

    // Step 5: Write the buffer's contents to the new file.
    FAT32_DirectoryEntry new_dest_entry;
    if (!fat32_find_entry_by_name(dest_name, dest_dir_cluster, NULL, &new_dest_entry)) {
        free(file_buffer);
        return false;
    }

    bool write_success = fat32_write_file(&new_dest_entry, file_buffer, source_entry.file_size);

    // Step 6: Update the directory entry on disk with the correct size and clusters.
    if (write_success) {
        fat32_update_entry(&new_dest_entry, &dest_loc);
    }

    free(file_buffer);
    return write_success;
}

/**
 * @brief Writes a modified directory entry back to its location on disk.
 *
 * @param entry A pointer to the directory entry struct in memory containing the updated info.
 * @param loc A pointer to the struct that describes the exact disk location of the entry.
 * @return true if the update was successful, false otherwise.
 */
bool fat32_update_entry(FAT32_DirectoryEntry* entry, dir_entry_location_t* loc) {
    if (!loc->is_valid) {
        return false;
    }

    // Read the sector containing the entry.
    // A static buffer is used to avoid stack allocation issues in the kernel.
    static uint8_t sector_buffer[4096]; // Assuming max sector size of 4KB
    if(g_boot_sector.bytes_per_sec > sizeof(sector_buffer)) {
        // Handle error: sector size is larger than buffer
        return false;
    }
    ide_read_sectors(loc->lba, 1, sector_buffer);

    // Update the entry in the buffer by copying the new data over the old data.
    memcpy(sector_buffer + loc->offset, entry, sizeof(FAT32_DirectoryEntry));

    // Write the modified sector back to disk.
    ide_write_sectors(loc->lba, 1, sector_buffer);

    return true;
}



bool fat32_create_directory(const char* dirname, uint32_t parent_cluster) {
    // Check if a directory with the same name already exists
    if (fat32_find_entry_by_name(dirname, parent_cluster, NULL, NULL)) {
        terminal_printf("Error: Directory '%s' already exists.\n", FG_RED, dirname);
        return false;
    }

    // Find a free slot in the parent directory for the new directory's entry
    dir_entry_location_t slot = find_free_directory_entry(parent_cluster);
    if (!slot.is_valid) {
        terminal_printf("Error: Parent directory is full.\n", FG_RED);
        return false;
    }

    // Find a free cluster to store the new directory's contents
    uint32_t new_dir_cluster = fat32_find_free_cluster();
    if (new_dir_cluster == 0) {
        terminal_printf("Error: Disk is full.\n", FG_RED);
        return false;
    }

    // Mark the new cluster as end-of-chain in the FAT
    fat32_set_fat_entry(new_dir_cluster, 0x0FFFFFFF);

    // Read the parent directory's sector into a buffer
    uint8_t* parent_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    ide_read_sectors(slot.lba, 1, parent_buffer);

    // Set up the new directory's entry in the parent directory
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(parent_buffer + slot.offset);
    to_fat32_filename(dirname, new_entry->name);
    new_entry->attr = ATTR_DIRECTORY;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;

    // Write the modified parent directory's sector back to disk
    ide_write_sectors(slot.lba, 1, parent_buffer);
    free(parent_buffer);

    // --- Initialize the new directory's contents (. and .. entries) ---
    uint8_t* new_dir_buffer = malloc(g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec);
    memset(new_dir_buffer, 0, g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec);

    // Create the '.' entry
    FAT32_DirectoryEntry* dot_entry = (FAT32_DirectoryEntry*)new_dir_buffer;
    to_fat32_filename(".", dot_entry->name);
    dot_entry->attr = ATTR_DIRECTORY;
    dot_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    dot_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;

    // Create the '..' entry
    FAT32_DirectoryEntry* dotdot_entry = dot_entry + 1;
    to_fat32_filename("..", dotdot_entry->name);
    dotdot_entry->attr = ATTR_DIRECTORY;
    dotdot_entry->fst_clus_hi = (parent_cluster >> 16) & 0xFFFF;
    dotdot_entry->fst_clus_lo = parent_cluster & 0xFFFF;

    // Write the new directory's contents to disk
    uint32_t new_dir_lba = cluster_to_lba(new_dir_cluster);
    ide_write_sectors(new_dir_lba, g_fat32_fs_info.sectors_per_cluster, new_dir_buffer);
    free(new_dir_buffer);

    return true;
}

/**
 * @brief Reads a FAT entry for a given cluster.
 *
 * @param cluster The cluster number to look up in the FAT.
 * @return The value of the FAT entry.
 */
uint32_t fat32_get_fat_entry(uint32_t cluster) {
    // Calculate the sector and offset within the FAT for the given cluster.
    // These values are part of the main boot sector, not the FSInfo structure.
    uint32_t fat_offset = cluster * 4;
    // Corrected the member name from 'reserved_sector_count' to 'rsvd_sec_cnt'.
    uint32_t fat_sector = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t ent_offset = fat_offset % g_boot_sector.bytes_per_sec;

    // Read the sector containing the FAT entry.
    // Note: A robust implementation would cache FAT sectors.
    // Using a static buffer to avoid stack allocation issues.
    static uint8_t sector_buffer[4096]; // Assuming max sector size of 4KB
    if(g_boot_sector.bytes_per_sec > sizeof(sector_buffer)) {
        // Handle error: sector size is larger than buffer
        return 0;
    }
    ide_read_sectors(fat_sector, 1, sector_buffer);

    // The entry is a 32-bit integer at the calculated offset.
    uint32_t table_value = *(uint32_t*)&sector_buffer[ent_offset];

    // The upper 4 bits of the entry are reserved and should be masked off.
    return table_value & 0x0FFFFFFF;
}


