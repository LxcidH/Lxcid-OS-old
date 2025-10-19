#include "fat32.h"
#include "../drivers/ide.h"
#include "../drivers/terminal.h"
#include "../lib/string.h"
#include "../memory/heap.h"
#include <stddef.h>
#include <stdbool.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// A struct to hold cached filesystem information for easy access.
typedef struct {
    uint32_t root_cluster_num;
    uint32_t first_data_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sec;
    uint32_t num_fats;
} FAT32_FSInfo;

// --- Global variables for the FAT32 driver ---
FAT32_BootSector g_boot_sector;
FAT32_FSInfo g_fat32_fs_info;
bool g_fat_ready = false;

// It's crucial to define the End of Chain (EOC) marker for FAT32.
#define FAT32_EOC_MARK 0x0FFFFFFF

// --- Forward declarations for static helper functions ---
static uint32_t cluster_to_lba(uint32_t cluster);
static uint32_t fat32_get_next_cluster(uint32_t current_cluster);
static void fat32_set_fat_entry(uint32_t cluster_num, uint32_t value);
static uint32_t fat32_find_free_cluster();
static dir_entry_location_t find_free_directory_entry(uint32_t start_cluster);
static void to_fat32_filename(const char* filename, char* out_name);
uint32_t fat32_get_fat_entry(uint32_t cluster);
uint64_t getTotalDriveSpace(const FAT32_BootSector* bpb);

// --- Public API Functions ---

void fat32_init() {
    ide_read_sectors(0, 1, (uint8_t*)&g_boot_sector);

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
    
    // Allocate a buffer for one cluster
    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if (cluster_buffer == NULL) {
        terminal_printf("Error: Not enough memory to list dir\n", FG_RED);
        return;
    }

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(current_cluster);
        ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);

        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)cluster_buffer;
        uint32_t entries_per_cluster = cluster_size_bytes / sizeof(FAT32_DirectoryEntry);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) { // End of directory
                free(cluster_buffer);
                return;
            }
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
    
    free(cluster_buffer);
}

void fat32_read_file(FAT32_DirectoryEntry* entry, void* buffer) {
    if (!g_fat_ready || entry == NULL || buffer == NULL) return;

    uint8_t* out_buffer = (uint8_t*)buffer;
    uint32_t current_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
    uint32_t file_size = entry->file_size;
    uint32_t bytes_read = 0;
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;

    // Allocate a temporary buffer to read full clusters from disk
    uint8_t* temp_cluster_buffer = malloc(cluster_size_bytes);
    if(temp_cluster_buffer == NULL) {
        terminal_printf("Error: Not enough memory to read file\n", FG_RED);
        return;
    }

    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        // Read the full cluster from disk into a temporary buffer
        uint32_t lba = cluster_to_lba(current_cluster);
        ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, temp_cluster_buffer);

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
    
    free(temp_cluster_buffer);
}

/**
 * @brief Finds an entry by name.
 * @note This function allocates memory for the returned entry.
 * The CALLER is responsible for freeing this memory!
 */
FAT32_DirectoryEntry* fat32_find_entry(const char* filename, uint32_t start_cluster) {
    // Allocate memory for the entry. The caller must free this.
    FAT32_DirectoryEntry* found_entry = malloc(sizeof(FAT32_DirectoryEntry));
    if (found_entry == NULL) {
        return NULL; // Out of memory
    }
    
    if (fat32_find_entry_by_name(filename, start_cluster, NULL, found_entry)) {
        return found_entry;
    }
    
    // If not found, free the memory we allocated and return NULL
    free(found_entry);
    return NULL;
}

FAT32_DirectoryEntry* fat32_find_entry_by_cluster(uint32_t cluster_to_find) {
    uint32_t dir_cluster = g_fat32_fs_info.root_cluster_num;
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;

    // Allocate a cluster buffer
    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if (cluster_buffer == NULL) return NULL;

    // Allocate memory for the entry we will return
    FAT32_DirectoryEntry* found_entry = malloc(sizeof(FAT32_DirectoryEntry));
    if (found_entry == NULL) {
        free(cluster_buffer);
        return NULL;
    }

    while (dir_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(dir_cluster);
        ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);

        FAT32_DirectoryEntry* entry = (FAT32_DirectoryEntry*)cluster_buffer;
        uint32_t entries_per_cluster = cluster_size_bytes / sizeof(FAT32_DirectoryEntry);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entry[i].name[0] == 0x00) {
                free(cluster_buffer);
                free(found_entry);
                return NULL;
            }
            if ((uint8_t)entry[i].name[0] == 0xE5) continue;
            if (entry[i].attr == ATTR_LONG_FILE_NAME) continue;

            uint32_t entry_cluster = (entry[i].fst_clus_hi << 16) | entry[i].fst_clus_lo;
            if (entry_cluster == cluster_to_find) {
                *found_entry = entry[i];
                free(cluster_buffer);
                return found_entry;
            }
        }
        dir_cluster = fat32_get_next_cluster(dir_cluster);
    }
    
    free(cluster_buffer);
    free(found_entry);
    return NULL;
}

// --- Static Helper Functions ---

static uint32_t cluster_to_lba(uint32_t cluster) {
    return g_fat32_fs_info.first_data_sector + (cluster - 2) * g_fat32_fs_info.sectors_per_cluster;
}

static uint32_t fat32_get_next_cluster(uint32_t current_cluster) {
    // This just delegates to the main fat32_get_fat_entry function
    return fat32_get_fat_entry(current_cluster);
}

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

    if (out_loc) {
        *out_loc = slot;
    }

    uint32_t initial_cluster = 0; // 0 for a zero-byte file

    // Allocate buffer for one sector
    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if (sector_buffer == NULL) return false;

    ide_read_sectors(slot.lba, 1, sector_buffer);
    
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(sector_buffer + slot.offset);

    memset(new_entry, 0, sizeof(FAT32_DirectoryEntry)); // Clear entry
    to_fat32_filename(filename, new_entry->name);
    new_entry->attr = ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (initial_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = initial_cluster & 0xFFFF;
    // Timestamps set to 0 for simplicity

    ide_write_sectors(slot.lba, 1, sector_buffer);
    // TODO: Check for ide_write_sectors failure here

    free(sector_buffer);
    return true;
}

void fat32_free_cluster_chain(uint32_t start_cluster) {
    uint32_t current_cluster = start_cluster;
    while (current_cluster < FAT32_EOC_MARK && current_cluster >= 2) {
        uint32_t next_cluster = fat32_get_fat_entry(current_cluster);
        fat32_set_fat_entry(current_cluster, 0); // Mark cluster as free
        current_cluster = next_cluster;
    }
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

    // Free the cluster chain
    uint32_t start_cluster = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    if (start_cluster >= 2) {
        fat32_free_cluster_chain(start_cluster);
    }

    // Allocate buffer for one sector
    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if (sector_buffer == NULL) return false;

    // Mark the directory entry as deleted
    ide_read_sectors(loc.lba, 1, sector_buffer);
    FAT32_DirectoryEntry* entry_to_delete = (FAT32_DirectoryEntry*)(sector_buffer + loc.offset);
    entry_to_delete->name[0] = 0xE5;
    ide_write_sectors(loc.lba, 1, sector_buffer);
    // TODO: Check for ide_write_sectors failure here
    
    free(sector_buffer);
    return true;
}

bool fat32_find_entry_by_name(const char* filename, uint32_t start_cluster, dir_entry_location_t* out_loc, FAT32_DirectoryEntry* out_entry) {
    if (!g_fat_ready) return false;

    char fat_filename[11];
    to_fat32_filename(filename, fat_filename);

    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;

    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if (cluster_buffer == NULL) return false;

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(current_cluster);
        ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);
        
        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)cluster_buffer;
        uint32_t entries_per_cluster = cluster_size_bytes / sizeof(FAT32_DirectoryEntry);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) { // End of directory
                free(cluster_buffer);
                return false;
            }
            if ((unsigned char)entries[i].name[0] == 0xE5 || entries[i].attr == ATTR_LONG_FILE_NAME) continue;

            if (strncmp(fat_filename, entries[i].name, 11) == 0) {
                if (out_entry) *out_entry = entries[i];
                if (out_loc) {
                    out_loc->is_valid = true;
                    uint32_t entry_offset = i * sizeof(FAT32_DirectoryEntry);
                    out_loc->lba = cluster_to_lba(current_cluster) + (entry_offset / g_boot_sector.bytes_per_sec);
                    out_loc->offset = entry_offset % g_boot_sector.bytes_per_sec;
                }
                free(cluster_buffer);
                return true;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    free(cluster_buffer);
    return false;
}

static dir_entry_location_t find_free_directory_entry(uint32_t start_cluster) {
    dir_entry_location_t invalid_loc = { .is_valid = false };
    if (!g_fat_ready) return invalid_loc;

    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;
    
    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if(cluster_buffer == NULL) return invalid_loc;

    // TODO: This loop does not handle allocating a new cluster if the directory is full
    // and needs to be extended. This should be added for a robust driver.

    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(current_cluster);
        ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);

        FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)cluster_buffer;
        uint32_t entries_per_cluster = cluster_size_bytes / sizeof(FAT32_DirectoryEntry);

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
                dir_entry_location_t loc;
                loc.is_valid = true;
                uint32_t entry_offset_in_cluster = i * sizeof(FAT32_DirectoryEntry);
                loc.lba = cluster_to_lba(current_cluster) + (entry_offset_in_cluster / g_boot_sector.bytes_per_sec);
                loc.offset = entry_offset_in_cluster % g_boot_sector.bytes_per_sec;
                free(cluster_buffer);
                return loc;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    free(cluster_buffer);
    return invalid_loc; // No free slot found
}

/**
 * @brief Sets a FAT entry for a given cluster. (FIXED)
 *
 * This function now correctly preserves the high 4 reserved bits
 * of the FAT entry, preventing filesystem corruption.
 */
static void fat32_set_fat_entry(uint32_t cluster_num, uint32_t value) {
    uint32_t fat_offset = cluster_num * 4;
    uint32_t fat_lba = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t fat_entry_offset = fat_offset % g_boot_sector.bytes_per_sec;

    // Allocate a buffer for one sector
    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if(sector_buffer == NULL) {
        terminal_printf("Error: Out of memory in set_fat_entry\n", FG_RED);
        return;
    }

    ide_read_sectors(fat_lba, 1, sector_buffer);

    // Get a pointer to the 32-bit entry in the buffer
    uint32_t* entry_ptr = (uint32_t*)&sector_buffer[fat_entry_offset];

    // Read the old value
    uint32_t old_value = *entry_ptr;

    // Combine the old reserved bits with the new value
    // (old_value & 0xF0000000) -> Preserves the top 4 bits
    // (value & 0x0FFFFFFF)     -> Takes only the 28 bits of the new value
    uint32_t new_value = (old_value & 0xF0000000) | (value & 0x0FFFFFFF);

    // Write the corrected value back to the buffer
    *entry_ptr = new_value;
    
    ide_write_sectors(fat_lba, 1, sector_buffer);
    // TODO: Check for ide_write_sectors failure here

    // --- FAT Mirroring ---
    // A robust driver must write to ALL FATs.
    // Assuming 2 FATs for simplicity:
    if (g_boot_sector.num_fats > 1) {
        uint32_t fat2_lba = fat_lba + g_boot_sector.fat_sz32;
        ide_write_sectors(fat2_lba, 1, sector_buffer);
         // TODO: Check for ide_write_sectors failure here
    }

    free(sector_buffer);
}

static uint32_t fat32_find_free_cluster() {
    if (!g_fat_ready) return 0;
    
    uint32_t entries_per_sector = g_boot_sector.bytes_per_sec / 4;
    uint32_t total_fat_sectors = g_boot_sector.fat_sz32;

    // Allocate a buffer for one sector
    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if(sector_buffer == NULL) return 0; // Out of memory

    // Start scan from cluster 2 (0 and 1 are reserved)
    // We'll skip sector 0 of the FAT which contains clusters 0 and 1
    for (uint32_t i = 0; i < total_fat_sectors; i++) {
        uint32_t lba = g_boot_sector.rsvd_sec_cnt + i;
        ide_read_sectors(lba, 1, sector_buffer);
        
        uint32_t* fat_entries = (uint32_t*)sector_buffer;
        
        for (uint32_t j = 0; j < entries_per_sector; j++) {
            if ((fat_entries[j] & 0x0FFFFFFF) == 0x00000000) {
                uint32_t cluster_num = (i * entries_per_sector) + j;
                if (cluster_num >= 2) { // Ensure we don't allocate 0 or 1
                    free(sector_buffer);
                    return cluster_num;
                }
            }
        }
    }
    
    free(sector_buffer);
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
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;

    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if(cluster_buffer == NULL) return false;

    // Read the directory's first cluster to check if it's empty
    uint32_t lba = cluster_to_lba(dir_cluster);
    ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);
    
    FAT32_DirectoryEntry* dir_entries = (FAT32_DirectoryEntry*)cluster_buffer;
    uint32_t entries_per_cluster = cluster_size_bytes / sizeof(FAT32_DirectoryEntry);

    for (uint32_t i = 0; i < entries_per_cluster; i++) {
        if (dir_entries[i].name[0] == 0x00) break;
        if ((unsigned char)dir_entries[i].name[0] == 0xE5 || dir_entries[i].attr == ATTR_LONG_FILE_NAME) continue;

        // Check for any file other than "." and ".."
        if (strncmp(dir_entries[i].name, ".          ", 11) != 0 && strncmp(dir_entries[i].name, "..         ", 11) != 0) {
            terminal_printf("Error: Directory '%s' is not empty.\n", FG_RED, dirname);
            free(cluster_buffer);
            return false;
        }
    }
    
    // Note: This only checks the first cluster. A robust implementation
    // would check all clusters in the directory chain.

    // Free the cluster(s) for the directory
    fat32_free_cluster_chain(dir_cluster);

    // Mark the directory entry as deleted in the parent
    // Re-use the cluster buffer as a sector buffer
    if (g_fat32_fs_info.bytes_per_sec > cluster_size_bytes) {
        // This shouldn't happen, but good to check
        free(cluster_buffer);
        cluster_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
        if(cluster_buffer == NULL) return false;
    }

    ide_read_sectors(loc.lba, 1, cluster_buffer);
    FAT32_DirectoryEntry* entry_to_delete = (FAT32_DirectoryEntry*)(cluster_buffer + loc.offset);
    entry_to_delete->name[0] = 0xE5;
    ide_write_sectors(loc.lba, 1, cluster_buffer);
    // TODO: Check for ide_write_sectors failure

    free(cluster_buffer);
    return true;
}


uint32_t fat32_get_parent_cluster(uint32_t cluster) {
    if (cluster == g_fat32_fs_info.root_cluster_num) {
        return g_fat32_fs_info.root_cluster_num;
    }

    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;
    uint8_t* buffer = malloc(cluster_size_bytes);
    if (buffer == NULL) {
        return 0; // Error
    }

    uint32_t lba = cluster_to_lba(cluster);
    ide_read_sectors(lba, g_fat32_fs_info.sectors_per_cluster, buffer);

    FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)buffer;

    // The '..' entry is always the second one
    FAT32_DirectoryEntry* dotdot_entry = &entries[1];

    if (strncmp(dotdot_entry->name, "..         ", 11) != 0 || !(dotdot_entry->attr & ATTR_DIRECTORY)) {
        free(buffer);
        return 0; // Error: Not a valid '..' entry
    }

    uint32_t parent_cluster = (dotdot_entry->fst_clus_hi << 16) | dotdot_entry->fst_clus_lo;

    // The root directory's '..' entry points to 0. 
    // If we read that, return the real root cluster instead.
    if (parent_cluster == 0) {
        parent_cluster = g_fat32_fs_info.root_cluster_num;
    }

    free(buffer);
    return parent_cluster;
}


bool fat32_write_file(FAT32_DirectoryEntry* entry, const void* buffer, uint32_t size) {
    uint32_t existing_cluster = ((uint32_t)entry->fst_clus_hi << 16) | entry->fst_clus_lo;

    // --- 1. Deallocate any existing cluster chain for the file ---
    if (existing_cluster != 0) {
        fat32_free_cluster_chain(existing_cluster);
    }

    // --- 2. Handle zero-size files as a special case ---
    if (size == 0) {
        entry->fst_clus_hi = 0;
        entry->fst_clus_lo = 0;
        entry->file_size = 0;
        return true; // Caller is responsible for writing updated entry
    }

    // --- 3. Allocate the first cluster for the new file content ---
    uint32_t first_cluster = fat32_find_free_cluster();
    if (first_cluster == 0) {
        return false; // No free clusters
    }
    fat32_set_fat_entry(first_cluster, FAT32_EOC_MARK);

    // Update the directory entry in memory
    entry->fst_clus_hi = (first_cluster >> 16) & 0xFFFF;
    entry->fst_clus_lo = first_cluster & 0xFFFF;
    entry->file_size = size;

    // --- 4. Write data to the cluster chain ---
    uint32_t current_cluster = first_cluster;
    uint32_t bytes_written = 0;
    const uint8_t* data_ptr = (const uint8_t*)buffer;
    const uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;

    // Allocate a temporary buffer for one cluster
    uint8_t* cluster_buffer = malloc(cluster_size_bytes);
    if(cluster_buffer == NULL) {
        // Out of memory. Clean up the chain we just started.
        fat32_free_cluster_chain(first_cluster);
        entry->fst_clus_hi = 0;
        entry->fst_clus_lo = 0;
        entry->file_size = 0;
        return false;
    }


    while (bytes_written < size) {
        uint32_t bytes_to_write = size - bytes_written;
        if (bytes_to_write > cluster_size_bytes) {
            bytes_to_write = cluster_size_bytes;
        }

        memcpy(cluster_buffer, data_ptr + bytes_written, bytes_to_write);

        // Zero out the rest of the buffer if it's the last chunk
        if (bytes_to_write < cluster_size_bytes) {
            memset(cluster_buffer + bytes_to_write, 0, cluster_size_bytes - bytes_to_write);
        }

        uint32_t lba = cluster_to_lba(current_cluster);
        ide_write_sectors(lba, g_fat32_fs_info.sectors_per_cluster, cluster_buffer);
        // TODO: Add error checking for ide_write_sectors.
        // If it fails, you must free the cluster_buffer, free the cluster
        // chain, reset the entry, and return false.

        bytes_written += bytes_to_write;

        // If there's still more data to write, allocate and link the next cluster.
        if (bytes_written < size) {
            uint32_t next_cluster = fat32_find_free_cluster();
            if (next_cluster == 0) {
                // Ran out of space mid-write.
                fat32_free_cluster_chain(first_cluster);
                entry->fst_clus_hi = 0;
                entry->fst_clus_lo = 0;
                entry->file_size = 0;
                free(cluster_buffer);
                return false;
            }

            fat32_set_fat_entry(current_cluster, next_cluster);
            fat32_set_fat_entry(next_cluster, FAT32_EOC_MARK);
            current_cluster = next_cluster;
        }
    }

    free(cluster_buffer);
    return true;
}


bool fat32_copy_file(const char* source_name, uint32_t source_dir_cluster, const char* dest_name, uint32_t dest_dir_cluster) {
    FAT32_DirectoryEntry source_entry;
    dir_entry_location_t dest_loc;

    if (!fat32_find_entry_by_name(source_name, source_dir_cluster, NULL, &source_entry)) {
        terminal_printf("Error: Source file '%s' not found.\n", FG_RED, source_name);
        return false;
    }
    if (source_entry.attr & ATTR_DIRECTORY) {
        terminal_printf("Error: Cannot copy a directory.\n", FG_RED);
        return false;
    }
    if (fat32_find_entry_by_name(dest_name, dest_dir_cluster, NULL, NULL)) {
        terminal_printf("Error: Destination file '%s' already exists.\n", FG_RED, dest_name);
        return false;
    }

    // Allocate a buffer for file content
    uint8_t* file_buffer = NULL;
    if(source_entry.file_size > 0) {
        file_buffer = malloc(source_entry.file_size);
        if (file_buffer == NULL) {
            terminal_printf("Error: Memory allocation failed.\n", FG_RED);
            return false;
        }
        fat32_read_file(&source_entry, file_buffer);
    }

    if (!fat32_create_file(dest_name, dest_dir_cluster, &dest_loc)) {
        if(file_buffer) free(file_buffer);
        return false;
    }

    // We need to read the new entry back to pass to fat32_write_file
    FAT32_DirectoryEntry new_dest_entry;
    if (!fat32_find_entry_by_name(dest_name, dest_dir_cluster, NULL, &new_dest_entry)) {
        if(file_buffer) free(file_buffer);
        // TODO: Should probably delete the 0-byte file we just created
        return false;
    }

    bool write_success = fat32_write_file(&new_dest_entry, file_buffer, source_entry.file_size);

    if (write_success) {
        // Write the updated directory entry (with correct size/cluster) back to disk
        fat32_update_entry(&new_dest_entry, &dest_loc);
    } else {
         // TODO: Should delete the failed file
    }

    if(file_buffer) free(file_buffer);
    return write_success;
}

bool fat32_update_entry(FAT32_DirectoryEntry* entry, dir_entry_location_t* loc) {
    if (!loc->is_valid) {
        return false;
    }

    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if(sector_buffer == NULL) return false;

    ide_read_sectors(loc->lba, 1, sector_buffer);

    memcpy(sector_buffer + loc->offset, entry, sizeof(FAT32_DirectoryEntry));

    ide_write_sectors(loc->lba, 1, sector_buffer);
    // TODO: Check for ide_write_sectors failure

    free(sector_buffer);
    return true;
}


bool fat32_create_directory(const char* dirname, uint32_t parent_cluster) {
    if (fat32_find_entry_by_name(dirname, parent_cluster, NULL, NULL)) {
        terminal_printf("Error: Directory '%s' already exists.\n", FG_RED, dirname);
        return false;
    }

    dir_entry_location_t slot = find_free_directory_entry(parent_cluster);
    if (!slot.is_valid) {
        terminal_printf("Error: Parent directory is full.\n", FG_RED);
        return false;
    }

    uint32_t new_dir_cluster = fat32_find_free_cluster();
    if (new_dir_cluster == 0) {
        terminal_printf("Error: Disk is full.\n", FG_RED);
        return false;
    }

    fat32_set_fat_entry(new_dir_cluster, 0x0FFFFFFF); // Mark as EOC

    // --- Update parent directory ---
    uint8_t* parent_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if(parent_buffer == NULL) {
        fat32_set_fat_entry(new_dir_cluster, 0); // Free cluster
        return false;
    }
    ide_read_sectors(slot.lba, 1, parent_buffer);

    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(parent_buffer + slot.offset);
    memset(new_entry, 0, sizeof(FAT32_DirectoryEntry));
    to_fat32_filename(dirname, new_entry->name);
    new_entry->attr = ATTR_DIRECTORY;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;

    ide_write_sectors(slot.lba, 1, parent_buffer);
    // TODO: Check for ide_write_sectors failure
    free(parent_buffer);

    // --- Initialize the new directory's cluster (. and ..) ---
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sec;
    uint8_t* new_dir_buffer = malloc(cluster_size_bytes);
    if(new_dir_buffer == NULL) {
        // This is bad. We've created an entry for a dir we can't initialize.
        // A robust driver would go back and delete the entry.
        return false;
    }
    memset(new_dir_buffer, 0, cluster_size_bytes);

    // Create the '.' entry
    FAT32_DirectoryEntry* dot_entry = (FAT32_DirectoryEntry*)new_dir_buffer;
    memset(dot_entry, 0, sizeof(FAT32_DirectoryEntry));
    to_fat32_filename(".", dot_entry->name);
    dot_entry->attr = ATTR_DIRECTORY;
    dot_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    dot_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;

    // Create the '..' entry
    FAT32_DirectoryEntry* dotdot_entry = dot_entry + 1;
    memset(dotdot_entry, 0, sizeof(FAT32_DirectoryEntry));
    to_fat32_filename("..", dotdot_entry->name);
    dotdot_entry->attr = ATTR_DIRECTORY;
    
    // Handle '..' entry for root directory
    uint32_t parent_dotdot_cluster = (parent_cluster == g_fat32_fs_info.root_cluster_num) ? 0 : parent_cluster;
    dotdot_entry->fst_clus_hi = (parent_dotdot_cluster >> 16) & 0xFFFF;
    dotdot_entry->fst_clus_lo = parent_dotdot_cluster & 0xFFFF;

    // Write the new directory's cluster to disk
    uint32_t new_dir_lba = cluster_to_lba(new_dir_cluster);
    ide_write_sectors(new_dir_lba, g_fat32_fs_info.sectors_per_cluster, new_dir_buffer);
    // TODO: Check for ide_write_sectors failure

    free(new_dir_buffer);
    return true;
}

uint32_t fat32_get_fat_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_boot_sector.bytes_per_sec);
    uint32_t ent_offset = fat_offset % g_boot_sector.bytes_per_sec;

    uint8_t* sector_buffer = malloc(g_fat32_fs_info.bytes_per_sec);
    if(sector_buffer == NULL) return 0; // Error

    ide_read_sectors(fat_sector, 1, sector_buffer);

    uint32_t table_value = *(uint32_t*)&sector_buffer[ent_offset];
    
    free(sector_buffer);

    return table_value & 0x0FFFFFFF;
}

disk_info fat32_get_disk_size(void) {
    disk_info dInfo;

    FAT32_BootSector bs = g_boot_sector;
    dInfo.disk_size_bytes = getTotalDriveSpace(&g_boot_sector);
    dInfo.vol_id = bs.vol_id;
    strcpy(dInfo.vol_lab, bs.vol_lab);
    return dInfo;
}

uint32_t get_total_sectors(const FAT32_BootSector* bpb) {
    if (bpb->tot_sec32 != 0) {
        return bpb->tot_sec32;
    } else {
        return bpb->tot_sec16;
    }
}

uint64_t getTotalDriveSpace(const FAT32_BootSector* bpb) {
    uint32_t total_sectors = get_total_sectors(bpb);
    return (uint64_t)total_sectors * bpb->bytes_per_sec;
}
