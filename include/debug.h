#pragma once
#include <efi.h>

static inline void dbg_puts(const char* s)
{
    while (*s)
    {
        __asm__ volatile ("outb %0, %1" : : "a"(*s), "Nd"((UINT16)0x402));
        s++;
    }
}
