#ifndef bqtRomAddrHH
#define bqtRomAddrHH

extern unsigned ROMmap_npages;

unsigned long ROM2NESaddr(unsigned long addr);
unsigned long NES2ROMaddr(unsigned long addr);

unsigned long MakeNESaddr(unsigned char bank, unsigned addr);
void SplitNESaddr(unsigned addr, unsigned char& bank, unsigned& offs);

unsigned GetPageSize()
#ifdef __GNUC__
 __attribute__((pure))
#endif
 ;

#endif
