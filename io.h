#ifndef IO_H
#define IO_H

#include <stdint.h>

/* 
 * outb: Write a byte to an I/O port 
 * Used for: Moving cursor, sending commands to hardware
 */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

/* 
 * inb: Read a byte from an I/O port
 * Used for: Reading keyboard scancodes
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

#endif
