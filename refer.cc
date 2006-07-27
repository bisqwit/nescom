#include "refer.hh"

unsigned ReferMethod::Evaluate(unsigned value) const
{
#if 0
    
    if(num_bytes == 1)
    {
        /* special case */
        return ROM2SNESpage(value >> 16) | or_mask;
    }
    
    if(num_bytes >= 3) { value = ROM2SNESaddr(value); }
#endif
    
    if(shr_by > 0) value >>= shr_by;
    else if(shr_by < 0) value <<= -shr_by;
    value |= or_mask;
    return value;
}
