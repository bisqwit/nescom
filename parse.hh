#ifndef bqt65asmParseHH
#define bqt65asmParseHH

#include <string>

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

struct ins_parameter
{
    char prefix;
    expression *exp;
    
    ins_parameter(): prefix(0), exp(NULL)
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
        || prefix == FORCE_HIBYTE)
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
        /* Note: no autoptr is used here */
    }
    const ins_parameter& operator= (const ins_parameter& p)
    {
        prefix = p.prefix;
        exp = p.exp;
        /* Note: no autoptr is used here */
        return *this;
    }
};

bool ParseExpression(ParseData& data, ins_parameter& result);

tristate ParseAddrMode(ParseData& data, unsigned modenum,
                       ins_parameter& p1, ins_parameter& p2);

bool IsDelimiter(char c);

#endif
