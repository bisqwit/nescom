#ifndef bqtReferHH
#define bqtReferHH

#include "rommap.hh"

// Far call takes four bytes:
//     22 63 EA C0 = JSL $C0:$EA63

// Near jump takes three bytes:
//     4C 23 58    = JMP db:$5823    (db=register)

// Short jump takes two bytes:
//     80 2A       = JMP (IP + 2 + $2A)

class ReferMethod
{
private:
    unsigned from_addr;
    unsigned long or_mask;
    unsigned char num_bytes;
    signed char shr_by;
protected:
    ReferMethod(unsigned long o,
                unsigned char n,
                signed char s,
                unsigned from)
      : from_addr(from),
        or_mask(o), num_bytes(n), shr_by(s) { }
public:
    unsigned Evaluate(unsigned value) const;
    unsigned GetAddr() const { return ROM2SNESaddr(from_addr); }
    unsigned GetSize() const { return num_bytes; }
};
struct CallFrom: public ReferMethod
{
    // JSL to specific location.
    CallFrom(unsigned from): ReferMethod(0x00000022,4,-8, from) {}
    // Formula: (value << 8) | 0xC0000022, 4 bytes
};
struct LongPtrFrom: public ReferMethod
{
    LongPtrFrom(unsigned from): ReferMethod(0x000000,3,0, from) {}
    // Formula: (value | 0xC00000), 3 bytes
};
struct OffsPtrFrom: public ReferMethod
{
    OffsPtrFrom(unsigned from): ReferMethod(0,2,0, from) {}
    // Formula: (value), 2 bytes
};
struct PagePtrFrom: public ReferMethod
{
    PagePtrFrom(unsigned from): ReferMethod(0x00,1,16, from) {}
    // Formula: (value >> 16) | 0xC0, 1 byte
};

#endif
