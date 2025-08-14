#ifndef IDE_H
#define IDE_H

void ide_read_sectors(uint32_t lba, uint8_t count, uint8_t* buf);

#endif
