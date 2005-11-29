#include "refer.hh"

unsigned ReferMethod::Evaluate(unsigned value) const
{
    if(shr_by > 0) value >>= shr_by;
    else if(shr_by < 0) value <<= -shr_by;
    value |= or_mask;
    return value;
}
