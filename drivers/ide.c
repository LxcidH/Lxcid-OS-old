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
#define IDE_CMD_WRITE_SECTORS       0x30

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
        // --- CORRECTED POLLING LOGIC ---
        // 1. Wait for the drive to not be busy.
        while (inb(IDE_STATUS_REG) & IDE_STATUS_BSY) {
            // Wait
        }

        // 2. Check for errors or if data is ready.
        uint8_t status = inb(IDE_STATUS_REG);
        if (status & IDE_STATUS_ERR) {
            terminal_writeerror("IDE Read Error!\n");
            // Handle error...
            return;
        }
        if (!(status & IDE_STATUS_DRQ)) {
            terminal_writeerror("IDE DRQ not set!\n");
            // This is an unexpected state, handle as an error.
            return;
        }

        // Read 256 16-bit words (512 bytes) from the data port
        insw(IDE_DATA_REG, ptr, 256);

        ptr += 256;         // Move the ptr to the next sector's location in buffer

        // A 400ns delay is required after reading each sector
        ide_400ns_delay();

    }
}

void ide_write_sectors(uint32_t lba, uint8_t count, uint8_t* buf) {
    ide_poll();
    outb(IDE_DRIVE_HEAD_REG, 0xE0 | (uint8_t)(lba >> 24));
        outb(IDE_SECTOR_COUNT_REG, count);
    outb(IDE_LBA_LO_REG, (uint8_t)lba);
    outb(IDE_LBA_MID_REG, (uint8_t)(lba >> 8));
    outb(IDE_LBA_HI_REG, (uint8_t)(lba >> 16));
    outb(IDE_COMMAND_REG, IDE_CMD_WRITE_SECTORS); // Command 0x30
    ide_poll();

    uint16_t* ptr = (uint16_t*)buf;
    for (int i = 0; i < count; i++) {
        outsw(IDE_DATA_REG, ptr, 256); // Use outsw
        ptr += 256;
    }

    // FLUSH COMMAND NEEDED FOR REAL HARDWARE, QEMU IS FINE WITHOUT
}
