#include "../io/io.h"
#include "terminal.h"
#include <stdint.h>

// Define primary IDE controller I/O ports
#define IDE_DATA_REG                0x1F0
#define IDE_ERROR_REG               0x1F1
#define IDE_SECTOR_COUNT_REG        0x1F2
#define IDE_LBA_LO_REG              0x1F3
#define IDE_LBA_MID_REG             0x1F4
#define IDE_LBA_HI_REG              0x1F5
#define IDE_DRIVE_HEAD_REG          0x1F6
#define IDE_STATUS_REG              0x1F7
#define IDE_COMMAND_REG             0x1F7

// Status Register Bits
#define IDE_STATUS_BSY              0x80
#define IDE_STATUS_DRDY             0x40
#define IDE_STATUS_DRQ              0x08
#define IDE_STATUS_ERR              0x01

// Commands
#define IDE_CMD_READ_SECTORS        0x20
#define IDE_CMD_WRTIE_SECTORS       0x30

// Helper func for the 400ns delay
static void ide_400ns_delay() {
    // Reading the status port 4 times should be enough
    inb(IDE_STATUS_REG);
    inb(IDE_STATUS_REG);
    inb(IDE_STATUS_REG);
    inb(IDE_STATUS_REG);
}

// Poll the IDE controller until its no longer busy
static int ide_poll() {
    // Add a timeout counter
    for(int i = 0; i < 100000; i++) {
        if (!(inb(IDE_STATUS_REG) & IDE_STATUS_BSY)) {
            return 0; // Success!
        }
    }
    // If the loop finishes, we timed out
    return -1;
}

// Reads 'count' sectors from LBA into the buffer 'buf'
// Assumes buf is a valid ptr to a large enough memory area
void ide_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf) {
    ide_poll(); // Wait for drive to be ready

    // 1. Send the drive and LBA bits 24-27
    // For master drive, this is 0xE0 | (high 4 bits of LBA)
    outb(IDE_DRIVE_HEAD_REG, 0xE0 | (uint8_t)(lba >> 24));

    // 2. Send the sector count
    outb(IDE_SECTOR_COUNT_REG, count);

    // 3. Send the rest of the LBA address (low, mid, hi)
    outb(IDE_LBA_LO_REG, (uint8_t)lba);
    outb(IDE_LBA_MID_REG, (uint8_t)(lba >> 8));
    outb(IDE_LBA_HI_REG, (uint8_t)(lba >> 16));

    // 4. Send the read sectors command
    outb(IDE_COMMAND_REG, IDE_CMD_READ_SECTORS);

    // 5. Read the data from the disk
    uint16_t* ptr = (uint16_t*)buf;
    for(int i = 0; i < count; i++) {
        // Wait for the drive to be ready for data transfer (BSY = 0 && DRQ = 1)
        while (!(inb(IDE_STATUS_REG) & (IDE_STATUS_DRQ | IDE_STATUS_ERR))) {
            // wait
        }

        // If error occured
        if (inb(IDE_STATUS_REG) & IDE_STATUS_ERR) {
            // Handle error
            terminal_writeerror("IDE_STATUS_ERR!\n");
            uint8_t error = inb(IDE_ERROR_REG);
            terminal_printf("Error: %x", FG_RED, error);
            return;
        }

        // Read 256 16-bit words (512 bytes) from the data port
        insw(IDE_DATA_REG, ptr, 256);

        ptr += 256;         // Move the ptr to the next sector's location in buffer

        // A 400ns delay is required after reading each sector
        ide_400ns_delay();

    }
}
