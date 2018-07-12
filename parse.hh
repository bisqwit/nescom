#ifndef bqt65asmParseHH
#define bqt65asmParseHH

#include <string>
#include <memory>

#include "expr.hh"
#include "assemble.hh"
#include "tristate"

#undef EOF

struct ParseData
{
private:
    std::string data;
    unsigned pos, eofpos;
public:
    typedef unsigned StateType;

    ParseData() : data(), pos(0), eofpos(0) { }
    ParseData(const std::string& s) : data(s), pos(0), eofpos(data.size()) { }
    ParseData(const char *s) : data(s), pos(0), eofpos(data.size()) { }

    bool EOF() const { return pos >= eofpos; }
    void SkipSpace() { while(!EOF() && (data[pos] == ' ' || data[pos] == '\t'))++pos; }
    StateType SaveState() const { return pos; }
    void LoadState(const StateType state) { pos = state; }
    char GetC() { return EOF() ? 0 : data[pos++]; }
    char PeekC() const { return EOF() ? 0 : data[pos]; }

    const std::string GetRest() const { return data.substr(pos); }
};

class Object;
struct ins_parameter
{
    char prefix;
    std::unique_ptr<expression> exp;

    ins_parameter(): prefix(0), exp(/*NULL*/)
    {
    }

    ins_parameter(unsigned char num)
    : prefix(FORCE_LOBYTE), exp(new expr_number(num))
    {
    }

    tristate is_byte(const Object& obj) const;
    tristate is_word(const Object& obj) const;
    tristate is_long(const Object& obj) const;

    const std::string Dump() const
    {
        std::string result;
        if(prefix) result += prefix;
        if(exp) result += exp->Dump(); else result += "(nil)";
        return result;
    }
};

bool ParseExpression(ParseData& data, ins_parameter& result);

tristate ParseAddrMode(ParseData& data, unsigned modenum,
                       ins_parameter& p1, ins_parameter& p2,
                       const Object& obj);

bool IsDelimiter(char c);

#endif
