#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    // --- Standard BIOS Parameter block ---
    uint8_t     jmp_boot[3];
    char        oem_name[8];
    uint16_t    bytes_per_sec;      // Important! Usually 512
    uint8_t     sec_per_clus;       // Important! Sectors in a cluster
    uint16_t    rsvd_sec_cnt;       // Important! Size of reserved region
    uint8_t     num_fats;           // Important! Usually 2
    uint16_t    root_ent_cnt;       // (Not used in FAT32)
    uint16_t    tot_sec16;          // (Not used in FAT32)
    uint8_t     media;
    uint16_t    fat_sz16;           // (Not used in FAT32)
    uint16_t    sec_per_trk;        // Sectors per track
    uint16_t    num_heads;
    uint32_t    hidd_sec;
    uint32_t    tot_sec32;

    // --- FAT32 Extended BIOS Param Block ---
    uint32_t    fat_sz32;           // Important! Size of one FAT in sectors
    uint16_t    ext_flags;
    uint16_t    fs_ver;
    uint32_t    root_clus;          // Important! Cluster of the root dir
    uint16_t    fs_info;
    uint16_t    bk_boot_sec;
    uint8_t     reserved[12];
    uint8_t     drv_num;
    uint8_t     reserved1;
    uint8_t     boot_sig;
    uint32_t    vol_id;
    char        vol_lab[11];
    char        fs_type[8];         //  Should say "Fat32   " Make sure to pad with spaces.
}__attribute__((packed)) FAT32_BootSector;

// File attribute flags for the 'attr' field
#define ATTR_READ_ONLY      0x01
#define ATTR_HIDDEN         0x02
#define ATTR_SYSTEM         0x04
#define ATTR_VOLUME_ID      0x08
#define ATTR_DIRECTORY      0x10
#define ATTR_ARCHIVE        0x20
#define ATTR_LONG_FILE_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

typedef struct {
    char        name[11];           // Short filename in 8.3 format
    uint8_t     attr;               // File attributes
    uint8_t     nt_res;             // Reserved for use by window NT
    uint8_t     crt_time_tenth;     // Tenths of a second stamp at file creation
    uint16_t    crt_time;           // Time file was created
    uint16_t    crt_date;           // Date file was created
    uint16_t    lst_acc_date;       // Last access date
    uint16_t    fst_clus_hi;        // High 16 bits of the file's first cluster number
    uint16_t    wrt_time;           // Time of last write
    uint16_t    wrt_date;           // Date of last write
    uint16_t    fst_clus_lo;        // Low 16 bits of the file's first cluster number
    uint32_t    file_size;          // Size of the file in bytes
}__attribute__((packed)) FAT32_DirectoryEntry;

typedef struct {
    bool is_valid;
    uint32_t lba;     // The LBA of the sector containing the free entry
    uint32_t offset;  // The byte offset of the free entry within the sector
} dir_entry_location_t;

void fat32_init();

void fat32_list_root_dir();

bool fat32_create_file(const char* filename);

static uint32_t fat32_get_next_cluster(uint32_t current_cluster);
#endif
