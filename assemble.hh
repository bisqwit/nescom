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
        FORCE_LONG    = '@',
        FORCE_SEGBYTE = '^',
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

const std::string& GetPrevBranchLabel(unsigned length); // What "-" means for each length of "-"
const std::string& GetNextBranchLabel(unsigned length); // What "+" means for each length of "+"

void AssemblePrecompiled(std::FILE *fp, Object& obj);

#endif
