#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// --- FAT32 On-Disk Structures ---

// Structure of the FAT32 Boot Sector.
// The __attribute__((packed)) ensures the compiler doesn't add padding.
typedef struct {
    uint8_t jmp_boot[3];
    char oem_name[8];
    uint16_t bytes_per_sec;
    uint8_t sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t num_fats;
    uint16_t root_ent_cnt;
    uint16_t tot_sec16;
    uint8_t media;
    uint16_t fat_sz16;
    uint16_t sec_per_trk;
    uint16_t num_heads;
    uint32_t hidd_sec;
    uint32_t tot_sec32;
    uint32_t fat_sz32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_clus;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t reserved[12];
    uint8_t drv_num;
    uint8_t reserved1;
    uint8_t boot_sig;
    uint32_t vol_id;
    char vol_lab[11];
    char fil_sys_type[8];
} __attribute__((packed)) FAT32_BootSector;

// Structure of a FAT32 Directory Entry.
typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t nt_res;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed)) FAT32_DirectoryEntry;

// Enum for the file attributes byte in a directory entry.
enum FAT32_Attributes {
    ATTR_READ_ONLY      = 0x01,
    ATTR_HIDDEN         = 0x02,
    ATTR_SYSTEM         = 0x04,
    ATTR_VOLUME_ID      = 0x08,
    ATTR_DIRECTORY      = 0x10,
    ATTR_ARCHIVE        = 0x20,
    ATTR_LONG_FILE_NAME = 0x0F,
};

// A helper struct to store the precise location of a directory entry on disk.
typedef struct {
    bool is_valid;
    uint32_t lba;
    uint32_t offset;
} dir_entry_location_t;


// --- Public API Functions ---

// Initializes the FAT32 driver by reading the boot sector.
void fat32_init();

// Gets the cluster number of the root directory.
uint32_t fat32_get_root_cluster();

// Lists the contents of a directory to the terminal.
void fat32_list_dir(uint32_t start_cluster);

// Reads the contents of a file into a buffer.
void fat32_read_file(FAT32_DirectoryEntry* entry, void* buffer);

// Finds a directory entry by its name.
FAT32_DirectoryEntry* fat32_find_entry(const char* filename, uint32_t start_cluster);

// Finds a directory entry by its starting cluster number.
FAT32_DirectoryEntry* fat32_find_entry_by_cluster(uint32_t cluster_to_find);

// Creates a new empty file.
bool fat32_create_file(const char* filename, uint32_t parent_cluster);

// Deletes a file.
bool fat32_delete_file(const char* filename, uint32_t parent_cluster);

// Creates a new empty directory.
bool fat32_create_directory(const char* dirname, uint32_t parent_cluster);

// Deletes an empty directory.
bool fat32_delete_directory(const char* dirname, uint32_t parent_cluster);

// Converts a standard 8.3 FAT filename to a readable string.
void fat_name_to_string(const char fat_name[11], char* out_name);

#endif // FAT32_H
