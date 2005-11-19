#ifndef bqt65asmAssembleHH
#define bqt65asmAssembleHH

#include "object.hh"
#include <string>

namespace
{
    enum
    {
        FORCE_LOBYTE  = '<',
        FORCE_HIBYTE  = '>',
        FORCE_ABSWORD = '!',
        FORCE_REL8    = 1
    };
    
    enum
    {
        prio_addsub = 1, // + -
        prio_shifts = 2, // << >>
        prio_divmul = 3, // * /
        prio_bitand = 4, // &
        prio_bitor  = 5, // |
        prio_bitxor = 6, // ^
        prio_negate = 7, // -
        prio_bitnot = 8  // ~
    };
}

extern bool A_16bit;
extern bool X_16bit;

extern std::string PrevBranchLabel; // What "-" means
extern std::string NextBranchLabel; // What "+" means

void AssemblePrecompiled(std::FILE *fp, Object& obj);

#endif
