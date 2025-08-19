#include "fat32.h"
#include "../drivers/ide.h"
#include "../drivers/terminal.h"
#include "../lib/string.h"
#include <stddef.h>
#include <stdbool.h>

// A struct to hold cached filesystem information for easy access.
typedef struct {
    uint32_t root_cluster_num;
    uint32_t first_data_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
} FAT32_FSInfo;

// --- Global variables for the FAT32 driver ---
// These are no longer static so all functions in this file can access them.
FAT32_BootSector g_boot_sector;
FAT32_FSInfo g_fat32_fs_info;
bool g_fat_ready = false;

// A reusable buffer for reading single clusters safely (avoids stack overflow)
#define MAX_CLUSTER_SIZE 32768 // 32 KB
uint8_t g_cluster_buffer[MAX_CLUSTER_SIZE];


// --- Forward declarations for static helper functions ---
static void fat32_read_cluster(uint32_t cluster_num, void* buffer);
static uint32_t cluster_to_lba(uint32_t cluster);
static uint32_t fat32_get_next_cluster(uint32_t current_cluster);
static void fat32_set_fat_entry(uint32_t cluster_num, uint32_t value);
static uint32_t fat32_find_free_cluster();
static dir_entry_location_t find_free_directory_entry(uint32_t start_cluster);
static bool fat32_find_entry_by_name(const char* filename, uint32_t start_cluster, dir_entry_location_t* out_loc, FAT32_DirectoryEntry* out_entry);
static void to_fat32_filename(const char* filename, char* out_name);


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
    g_fat32_fs_info.bytes_per_sector = g_boot_sector.bytes_per_sec;

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
    uint32_t cluster_size_bytes = g_fat32_fs_info.bytes_per_sector * g_fat32_fs_info.sectors_per_cluster;
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
    uint32_t cluster_size_bytes = g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sector;

    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        fat32_read_cluster(current_cluster, out_buffer + bytes_read);
        bytes_read += cluster_size_bytes;
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
        uint32_t entries_per_cluster = (g_fat32_fs_info.sectors_per_cluster * g_fat32_fs_info.bytes_per_sector) / sizeof(FAT32_DirectoryEntry);

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
    uint32_t fat_sector = g_boot_sector.rsvd_sec_cnt + (fat_offset / g_fat32_fs_info.bytes_per_sector);
    uint32_t ent_offset = fat_offset % g_fat32_fs_info.bytes_per_sector;

    // A small static buffer for reading single FAT sectors
    static uint8_t fat_sector_buffer[512];
    ide_read_sectors(fat_sector, 1, fat_sector_buffer);

    return (*(uint32_t*)&fat_sector_buffer[ent_offset]) & 0x0FFFFFFF;
}

// ... the rest of your static helper functions (fat32_create_file, etc.) go here ...
// (The code you provided for these functions is fine and doesn't need to be changed)

bool fat32_create_file(const char* filename, uint32_t parent_cluster) {
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

    fat32_set_fat_entry(free_cluster, 0x0FFFFFFF);

    ide_read_sectors(slot.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(g_cluster_buffer + slot.offset);

    to_fat32_filename(filename, new_entry->name);
    new_entry->attr = ATTR_ARCHIVE;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (free_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = free_cluster & 0xFFFF;
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

static bool fat32_find_entry_by_name(const char* filename, uint32_t start_cluster, dir_entry_location_t* out_loc, FAT32_DirectoryEntry* out_entry) {
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
            if (entries[i].name[0] == 0x00) return false;
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

bool fat32_create_directory(const char* dirname, uint32_t parent_cluster) {
    if (!g_fat_ready) return false;

    if (fat32_find_entry_by_name(dirname, parent_cluster, NULL, NULL)) {
        terminal_printf("Error: '%s' already exists.\n", FG_RED, dirname);
        return false;
    }

    dir_entry_location_t slot = find_free_directory_entry(parent_cluster);
    if (!slot.is_valid) {
        terminal_printf("Error: Directory is full.\n", FG_RED);
        return false;
    }

    uint32_t new_dir_cluster = fat32_find_free_cluster();
    if (new_dir_cluster == 0) {
        terminal_printf("Error: Disk is full.\n", FG_RED);
        return false;
    }

    fat32_set_fat_entry(new_dir_cluster, 0x0FFFFFFF);

    ide_read_sectors(slot.lba, 1, g_cluster_buffer);
    FAT32_DirectoryEntry* new_entry = (FAT32_DirectoryEntry*)(g_cluster_buffer + slot.offset);
    to_fat32_filename(dirname, new_entry->name);
    new_entry->attr = ATTR_DIRECTORY;
    new_entry->file_size = 0;
    new_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    new_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;
    new_entry->wrt_time = 0; new_entry->wrt_date = 0;
    ide_write_sectors(slot.lba, 1, g_cluster_buffer);

    uint32_t new_dir_lba = cluster_to_lba(new_dir_cluster);
    uint32_t cluster_size_bytes = g_boot_sector.bytes_per_sec * g_boot_sector.sec_per_clus;
    memset(g_cluster_buffer, 0, cluster_size_bytes);

    FAT32_DirectoryEntry* dot_entry = (FAT32_DirectoryEntry*)g_cluster_buffer;
    memcpy(dot_entry->name, ".          ", 11);
    dot_entry->attr = ATTR_DIRECTORY;
    dot_entry->fst_clus_hi = (new_dir_cluster >> 16) & 0xFFFF;
    dot_entry->fst_clus_lo = new_dir_cluster & 0xFFFF;

    FAT32_DirectoryEntry* dotdot_entry = dot_entry + 1;
    memcpy(dotdot_entry->name, "..         ", 11);
    dotdot_entry->attr = ATTR_DIRECTORY;
    dotdot_entry->fst_clus_hi = (parent_cluster >> 16) & 0xFFFF;
    dotdot_entry->fst_clus_lo = parent_cluster & 0xFFFF;

    ide_write_sectors(new_dir_lba, g_boot_sector.sec_per_clus, g_cluster_buffer);

    return true;
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
