#include <cctype>

#include "parse.hh"
#include "expr.hh"
#include "insdata.hh"

namespace
{
    const std::string ParseToken(ParseData& data)
    {
        std::string token;
        data.SkipSpace();
        char c = data.PeekC();
        if(c == '$')
        {
        Hex:
            token += data.GetC();
        Hex2:
            for(;;)
            {
                c = data.PeekC();
                if( (c >= '0' && c <= '9')
                 || (c >= 'A' && c <= 'F')
                 || (c >= 'a' && c <= 'f')
                 || (c == '-' && token.size()==1)
                  )
                    token += data.GetC();
                else
                    break;
            }
        }
        else if(c == '-' || (c >= '0' && c <= '9'))
        {
            if(c == '-')
            {
                ParseData::StateType state = data.SaveState();
                data.GetC();
                char c = data.PeekC();
                data.LoadState(state);
                if(c == '$')
                {
                    token += c;
                    goto Hex;
                }
                bool ok = c >= '0' && c <= '9';
                if(!ok) return token;
            }
            for(;;)
            {
                token += data.GetC();
                
                if(data.EOF()) break;
                c = data.PeekC();
                
                if(c == 'x' || c == 'X')
                {
                    if(token == "0")  { data.GetC(); token = "$"; goto Hex2; }
                    if(token == "-0") { data.GetC(); token = "-$"; goto Hex2; }
                }
                if(c < '0' || c > '9') break;
            }
        }
        if(isalpha(c) || c == '_')
        {
            for(;;)
            {
                token += data.GetC();
                if(data.EOF()) break;
                c = data.PeekC();
                if(c != '_' && !isalnum(c)) break;
            }
        }
        return token;
    }
    
    expression* CreatePlusExpr(expression* left, expression* right)
    {
        return new sum_group(left, right, false);
    }

    expression* CreateMinusExpr(expression* left, expression* right)
    {
        return new sum_group(left, right, true);
    }

    expression* RealParseExpression(ParseData& data, int prio=0,
                                    char disallow_local_label=0)
    {
        std::string s = ParseToken(data);
        
        expression* left = NULL;
        
        if(s.empty()) /* If no number or symbol */
        {
            char c = data.PeekC();
            
            char local_label = 0;
            unsigned local_length = 0;
            
            /* disallow_local_label is used to prevent --
               from being parsed as -($PrevLabel$)
             */
            
            if((c == '+' || c == '-') && c != disallow_local_label)
            {
                ParseData::StateType state = data.SaveState();
                local_label = c;
                do { ++local_length; data.GetC(); } while(data.PeekC() == c);
                data.LoadState(state);
            }
            
            if(c == '-') // negation
            {
                ParseData::StateType state = data.SaveState();
                data.GetC(); // eat
                data.SkipSpace();
                const std::string RestBefore = data.GetRest();
                if(data.PeekC() != '+')
                {
                    left = RealParseExpression(data, prio_negate, c);
                }
                if(!left)
                {
                    const std::string RestAfter = data.GetRest();
                    data.LoadState(state);
                    
                    if(RestBefore != RestAfter)
                    {
                        // If the error happened later, return an error.
                        return left;
                    }
                    
                    // If it ate *nothing*, we've got PrevBranchLabel here.
                    goto GotLocalLabel;
                }
                else
                {
                    left = new expr_negate(left);
                }
            }
            else if(c == '~')
            {
                ParseData::StateType state = data.SaveState();
                data.GetC(); // eat
                left = RealParseExpression(data, prio_bitnot);
                if(!left) { data.LoadState(state); return left; }
                left = new expr_bitnot(left);
            }
            else if(c == '(')
            {
                ParseData::StateType state = data.SaveState();
                data.GetC(); // eat
                left = RealParseExpression(data, 0);
                data.SkipSpace();
                if(data.PeekC() == ')') data.GetC();
                else if(left) { delete left; left = NULL; }
                if(!left) { data.LoadState(state); return left; }
            }
            else
            {
            GotLocalLabel:
                switch(local_label)
                {
                    case '-':
                        left = new expr_label(GetPrevBranchLabel(local_length));
                        for(unsigned a=0; a<local_length; ++a) data.GetC();
                        break;
                    case '+':
                        left = new expr_label(GetNextBranchLabel(local_length));
                        for(unsigned a=0; a<local_length; ++a) data.GetC();
                        break;
                }
                // default
                return left;
            }
        }
        else if(s[0] == '-' || s[0] == '$' || (s[0] >= '0' && s[0] <= '9'))
        {
            long value = 0;
            bool negative = false;
            // Number.
            if(s[0] == '$')
            {
                unsigned pos = 1;
                if(s[1] == '-') { ++pos; negative = true; }
                
                for(; pos < s.size(); ++pos)
                {
                    value = value*16;
                    if(s[pos] >= 'A' && s[pos] <= 'F') value += 10+s[pos]-'A';
                    if(s[pos] >= 'a' && s[pos] <= 'f') value += 10+s[pos]-'a';
                    if(s[pos] >= '0' && s[pos] <= '9') value +=    s[pos]-'0';
                }
            }
            else
            {
                unsigned pos = 0;
                if(s[0] == '-') { negative = true; ++pos; }
                for(; pos < s.size(); ++pos)
                    value = value*10 + (s[pos] - '0');
            }
            
            if(negative) value = -value;
            left = new expr_number(value);
        }
        else
        {
            if(IsReservedWord(s))
            {
                /* Attempt to use a reserved as variable name */
                return left;
            }
            
            left = new expr_label(s);
        }

        data.SkipSpace();
        if(!left) return left;
        
    Reop:
        if(!data.EOF())
        {
            #define op2(reqprio, c1,c2, create_expr) \
                if(prio < reqprio && data.PeekC() == c1) \
                { \
                    ParseData::StateType state = data.SaveState(); \
                    bool ok = true; \
                    if(c2 != 0) \
                    { \
                        data.GetC(); char c = data.PeekC(); \
                        data.LoadState(state); \
                        ok = c == c2; \
                        if(ok) data.GetC(); \
                    } \
                    if(ok) \
                    { \
                        data.GetC(); \
                        expression *right = RealParseExpression(data, reqprio); \
                        if(!right) \
                        { \
                            data.LoadState(state); \
                            return left; \
                        } \
                        left = create_expr(left, right); \
                        if(left->IsConst()) \
                        { \
                            right = new expr_number(left->GetConst()); \
                            delete left; \
                            left = right; \
                        } \
                        goto Reop; \
                }   }
            
            op2(prio_addsub, '+',   0, CreatePlusExpr);
            op2(prio_addsub, '-',   0, CreateMinusExpr);
            op2(prio_divmul, '*',   0, new expr_mul);
            op2(prio_divmul, '/',   0, new expr_div);
            op2(prio_shifts, '<', '<', new expr_shl);
            op2(prio_shifts, '>', '>', new expr_shr);
            op2(prio_bitand, '&',   0, new expr_bitand);
            op2(prio_bitor,  '|',   0, new expr_bitor);
            op2(prio_bitxor, '^',   0, new expr_bitxor);
        }
        return left;
    }
}

bool ParseExpression(ParseData& data, ins_parameter& result)
{
    data.SkipSpace();
    
    if(data.EOF()) return false;
    
    char prefix = data.PeekC();
    if(prefix == FORCE_LOBYTE
    || prefix == FORCE_HIBYTE
    || prefix == FORCE_ABSWORD
    || prefix == FORCE_LONG
    || prefix == FORCE_SEGBYTE
      )
    {
        // good prefix
        data.GetC();
    }
    else
    {
        // no prefix
        prefix = 0;
    }
    
    expression* e = RealParseExpression(data);
    
    if(e)
    {
        e->Optimize(e);
    }

    //std::fprintf(stderr, "ParseExpression returned: '%s'\n", result.Dump().c_str());
    
    result.prefix = prefix;
    result.exp    = e;
    
    return e != NULL;
}

static bool CompareChar(char c1, char c2)
{
    if(c1 == '¶') c1 = '#';
    if(c2 == '¶') c2 = '#';
    return c1 == c2;
}

tristate ParseAddrMode(ParseData& data, unsigned modenum,
                       ins_parameter& p1, ins_parameter& p2)
{
    #define ParseReq(s) \
        for(const char *q = s; *q; ++q, data.GetC()) { \
            data.SkipSpace(); if(!CompareChar(data.PeekC(), *q)) return false; }
    #define ParseNotAllow(c) \
        data.SkipSpace(); if(CompareChar(data.PeekC(), c)) return false
    #define ParseOptional(c) \
        data.SkipSpace(); if(CompareChar(data.PeekC(), c)) data.GetC()
    #define ParseExpr(p) \
        if(!ParseExpression(data, p)) return false

    if(modenum >= AddrModeCount) return false;
    
    const AddrMode& modedata = AddrModes[modenum];
    
    if(modedata.forbid) { ParseNotAllow(modedata.forbid); }
    ParseReq(modedata.prereq);
    if(modedata.p1 != AddrMode::tNone) { ParseExpr(p1); }
    if(modedata.p2 != AddrMode::tNone) { ParseOptional(','); ParseExpr(p2); }
    ParseReq(modedata.postreq);
    
    data.SkipSpace();
    tristate result = data.EOF();
    switch(modedata.p1)
    {
        case AddrMode::tByte: result=result && p1.is_byte(); break;
        case AddrMode::tWord: result=result && p1.is_word(); break;
        case AddrMode::tLong: result=result && p1.is_long(); break;
        case AddrMode::tA: result=result && A_16bit ? p1.is_word() : p1.is_byte(); break;
        case AddrMode::tX: result=result && X_16bit ? p1.is_word() : p1.is_byte(); break;
        case AddrMode::tRel8: ;
        case AddrMode::tRel16: ;
        case AddrMode::tNone: ;
    }
    switch(modedata.p2)
    {
        case AddrMode::tByte: result=result && p2.is_byte(); break;
        case AddrMode::tWord: result=result && p2.is_word(); break;
        case AddrMode::tLong: result=result && p2.is_long(); break;
        case AddrMode::tA: result=result && A_16bit ? p2.is_word() : p2.is_byte(); break;
        case AddrMode::tX: result=result && X_16bit ? p2.is_word() : p2.is_byte(); break;
        case AddrMode::tRel8: ;
        case AddrMode::tRel16: ;
        case AddrMode::tNone: ;
    }
    
/*
    std::fprintf(stderr, "Parsed: p1=\"%s\", p2=\"%s\"\n",
        p1.Dump().c_str(),p2.Dump().c_str());
*/

    return result;
}

bool IsDelimiter(char c)
{
    return c == ':' || c == '\r' || c == '\n';
}
