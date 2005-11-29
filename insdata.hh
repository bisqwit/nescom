/* snescom 65c816 instruction database for snescom and deasm */

#include <string>

unsigned GetOperand1Size(unsigned modenum);
unsigned GetOperand2Size(unsigned modenum);
unsigned GetOperandSize(unsigned modenum);
bool IsReservedWord(const std::string& s);

struct AddrMode
{
    char forbid;
    const char *prereq;
    const char *postreq;
    enum { tNone, tByte, tWord, tLong, tA, tX, tRel8, tRel16 } p1, p2;
};
extern const struct AddrMode AddrModes[];
extern const unsigned AddrModeCount;

struct ins
{
    const char *token;
    const char *opcodes;
    bool operator< (const std::string &s) const { return s > token; }
};
extern const struct ins ins[];
extern const unsigned InsCount;
