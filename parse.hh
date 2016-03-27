#ifndef bqt65asmParseHH
#define bqt65asmParseHH

#include <string>

#include "expr.hh"
#include "assemble.hh"
#include "tristate"

#include <memory>

#undef EOF

struct ParseData
{
private:
    std::string data;
    size_t pos, eofpos;
public:
    typedef size_t StateType;

    ParseData() : data(), pos(0), eofpos(0) { }
    ParseData(const std::string& s) : data(s), pos(0), eofpos(data.size()) { }
    ParseData(const char *s) : data(s), pos(0), eofpos(data.size()) { }

    bool EOF() const { return pos >= eofpos; }
    void SkipSpace() { while(!EOF() && (data[pos] == ' ' || data[pos] == '\t'))++pos; }
    StateType SaveState() const { return pos; }
    void LoadState(const StateType state) { pos = state; }
    char GetC() { return EOF() ? '\0' : data[pos++]; }
    char PeekC() const { return EOF() ? '\0' : data[pos]; }

    const std::string GetRest() const { return data.substr(pos); }
};

struct ins_parameter
{
    char prefix;
    std::shared_ptr<expression> exp;

    ins_parameter(): prefix(0), exp(/*NULL*/)
    {
    }

    ins_parameter(unsigned char num)
    : prefix(FORCE_LOBYTE), exp(new expr_number(num))
    {
    }

    // Note: No deletes here. Would cause problems when storing in vectors.

    tristate is_byte() const
    {
        if(prefix == FORCE_LOBYTE
        || prefix == FORCE_HIBYTE
        || prefix == FORCE_SEGBYTE)
        {
            return true;
        }
        if(prefix) return false;
        if(!exp->IsConst()) return maybe;
        long value = exp->GetConst();
        return value >= -0x80 && value < 0x100;
    }
    tristate is_word() const
    {
        if(prefix == FORCE_ABSWORD) return true;
        if(prefix) return false;
        if(!exp->IsConst()) return maybe;
        long value = exp->GetConst();
        return value >= -0x8000 && value < 0x10000;
    }
    tristate is_long() const
    {
        if(prefix == FORCE_LONG) return true;
        if(prefix) return false;
        if(!exp->IsConst()) return maybe;
        long value = exp->GetConst();
        return value >= -0x800000 && value < 0x1000000;
    }

    const std::string Dump() const
    {
        std::string result;
        if(prefix) result += prefix;
        if(exp) result += exp->Dump(); else result += "(nil)";
        return result;
    }
public:
    ins_parameter(const ins_parameter& p) : prefix(p.prefix), exp(p.exp)
    {
    }
};

bool ParseExpression(ParseData& data, ins_parameter& result);

tristate ParseAddrMode(ParseData& data, unsigned modenum,
                       ins_parameter& p1, ins_parameter& p2);

bool IsDelimiter(char c);

#endif
