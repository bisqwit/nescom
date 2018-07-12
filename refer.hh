#ifndef bqtReferHH
#define bqtReferHH

#include "romaddr.hh"

/* FIXME: Not yet converted to NES; uses SNES opcodes */

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
    unsigned GetAddr() const { return ROM2NESaddr(from_addr); }
    unsigned GetSize() const { return num_bytes; }
};
struct OffsPtrFrom: public ReferMethod
{
    OffsPtrFrom(unsigned from): ReferMethod(0,2,0, from) {}
    // Formula: (value), 2 bytes
};

#endif
