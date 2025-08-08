#ifndef IO_H
#define IO_H

#include <stdint.h>

// Function to send a byte to an I/O port
void outb(uint16_t port, uint8_t data);

// Function to receieve a byte from an I/O Port
uint8_t inb(uint16_t port);

#endif
