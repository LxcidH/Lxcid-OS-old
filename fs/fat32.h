#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

// --- Data Structures ---

// A standard 32-byte FAT directory entry
typedef struct {
    char        name[11];           // Short filename in 8.3 format
    uint8_t     attr;               // File attributes
    uint8_t     nt_res;             // Reserved for use by Windows NT
    uint8_t     crt_time_tenth;
    uint16_t    crt_time;
    uint16_t    crt_date;
    uint16_t    lst_acc_date;
    uint16_t    fst_clus_hi;        // High 16 bits of first cluster number
    uint16_t    wrt_time;
    uint16_t    wrt_date;
    uint16_t    fst_clus_lo;        // Low 16 bits of first cluster number
    uint32_t    file_size;
} __attribute__((packed)) FAT32_DirectoryEntry;

// The FAT32 BIOS Parameter Block (Boot Sector)
typedef struct {
    uint8_t     jmp_boot[3];
    char        oem_name[8];
    uint16_t    bytes_per_sec;
    uint8_t     sec_per_clus;
    uint16_t    rsvd_sec_cnt;
    uint8_t     num_fats;
    uint16_t    root_ent_cnt;
    uint16_t    tot_sec16;
    uint8_t     media;
    uint16_t    fat_sz16;
    uint16_t    sec_per_trk;
    uint16_t    num_heads;
    uint32_t    hidd_sec;
    uint32_t    tot_sec32;
    uint32_t    fat_sz32;
    uint16_t    ext_flags;
    uint16_t    fs_ver;
    uint32_t    root_clus;
    uint16_t    fs_info;
    uint16_t    bk_boot_sec;
    uint8_t     reserved[12];
    uint8_t     drv_num;
    uint8_t     reserved1;
    uint8_t     boot_sig;
    uint32_t    vol_id;
    char        vol_lab[11];
    char        fs_type[8];
} __attribute__((packed)) FAT32_BootSector;

// Holds the exact on-disk location of a directory entry
typedef struct {
    bool is_valid;
    uint32_t lba;
    uint32_t offset;
} dir_entry_location_t;


// --- Constants ---

// File attribute flags for the 'attr' field
#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_FILE_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)


// --- Public Function Prototypes ---

// Initializes the FAT32 driver
void fat32_init();

// Converts a cluster number to its starting LBA sector address
uint32_t cluster_to_lba(uint32_t cluster);

// Returns the cluster number of the root directory
uint32_t fat32_get_root_cluster(void);

// Lists the contents of a directory
void fat32_list_dir(uint32_t start_cluster);

// Reads the full contents of a file
void fat32_read_file(FAT32_DirectoryEntry* entry, uint8_t* out_buffer);

// Creates a new empty file in the specified directory
bool fat32_create_file(const char* filename, uint32_t parent_cluster);

// Deletes a file from the specified directory
bool fat32_delete_file(const char* filename, uint32_t parent_cluster);

// Creates a new empty subdirectory
bool fat32_create_directory(const char* dirname, uint32_t parent_cluster);

// Deletes an empty subdirectory
bool fat32_delete_directory(const char* dirname, uint32_t parent_cluster);

// Converts a raw 11-byte FAT name to a user-friendly string
void fat_name_to_string(const char fat_name[11], char* out_name);
FAT32_DirectoryEntry* fat32_find_entry(const char* filename, uint32_t start_cluster);
bool fat32_delete_directory(const char* dirname, uint32_t parent_cluster);



#endif // FAT32_H
