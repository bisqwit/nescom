#include "romaddr.hh"
#include <stdio.h>

unsigned ROMmap_npages = 16;

unsigned long MakeNESaddr(unsigned char bank, unsigned offs)
{
    if(bank >= ROMmap_npages-1) return offs | 0xC000;
    return (bank << 16) | offs | 0x8000;
}

void SplitNESaddr(unsigned addr, unsigned char& bank, unsigned& offs)
{
    unsigned ROMaddr = NES2ROMaddr(addr);
    bank = ROMaddr >> 14;
    offs = ROMaddr & 0x3FFF;
}

static unsigned long ROM2NESaddr_(unsigned long addr)
{
    unsigned bank = addr >> 14;
    unsigned offs = addr & 0x3FFF;
    return MakeNESaddr(bank, offs);
}

static unsigned long NES2ROMaddr_(unsigned long addr)
{
    switch(addr / 0x4000)
    {
        case 0: case 1: break; // 0..7
        case 3: // C..F
        {
            unsigned bank = ROMmap_npages-1;
            unsigned offs = addr & 0x3FFF;
            return bank * 0x4000 + offs;
        }
        case 2:  // 8..B
        default: // 10...
            unsigned bank = addr >> 16;
            unsigned offs = addr & 0x3FFF;
            return bank * 0x4000 + offs;
    }
    return addr;
}

unsigned long ROM2NESaddr(unsigned long addr)
{
    unsigned long ret = ROM2NESaddr_(addr);
    //printf("ROM %X -> NES %X\n", addr, ret);
    return ret;
}

unsigned long NES2ROMaddr(unsigned long addr)
{
    unsigned long ret = NES2ROMaddr_(addr);
    //printf("NES %X -> ROM %X\n", addr, ret);
    return ret;
}

unsigned GetPageSize()
{
    return 0x4000;
}
