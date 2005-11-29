#include <cctype>
#include <string>
#include <utility>
#include <vector>
#include <list>
#include <map>

#include "expr.hh"
#include "parse.hh"
#include "assemble.hh"
#include "object.hh"
#include "insdata.hh"
#include "precompile.hh"

bool A_16bit = true;
bool X_16bit = true;

std::string PrevBranchLabel; // What "-" means
std::string NextBranchLabel; // What "+" means

#define SHOW_CHOICES   0
#define SHOW_POSSIBLES 0

namespace
{
    struct OpcodeChoice
    {
        typedef std::pair<unsigned, struct ins_parameter> paramtype;
        std::vector<paramtype> parameters;
        bool is_certain;
        
    public:
        OpcodeChoice(): parameters(), is_certain(false) { }
    };

    typedef std::vector<OpcodeChoice> ChoiceList;
    
    std::list<std::string> DefinedBranchLabels;
    
    void CreateNewPrevBranch()
    {
        static unsigned BranchNumber = 0;
        char Buf[128];
        std::sprintf(Buf, "$PrevBranch$%u", ++BranchNumber);
        
        PrevBranchLabel = Buf;
        
        DefinedBranchLabels.push_back(PrevBranchLabel);
    }
    void CreateNewNextBranch()
    {
        static unsigned BranchNumber = 0;
        char Buf[128];
        std::sprintf(Buf, "$NextBranch$%u", ++BranchNumber);
        
        NextBranchLabel = Buf;
        
        DefinedBranchLabels.push_back(NextBranchLabel);
    }
    
    unsigned ParseConst(ins_parameter& p, const Object& obj)
    {
        unsigned value = 0;
        
        std::set<std::string> labels;
        FindExprUsedLabels(p.exp, labels);
        
        for(std::set<std::string>::const_iterator
            i = labels.begin(); i != labels.end(); ++i)
        {
            const std::string& label = *i;

            SegmentSelection seg;

            if(obj.FindLabel(label, seg, value))
            {
                SubstituteExprLabel(p.exp, label, value);
            }
            else
            {
                fprintf(stderr,
                    "Error: Undefined label \"%s\" in expression - got \"%s\"\n",
                    label.c_str(), p.Dump().c_str());
            }
        }
        
        if(p.exp->IsConst())
            value = p.exp->GetConst();
        else
        {
            fprintf(stderr,
                "Error: Expression must be const - got \"%s\"\n",
                p.Dump().c_str());
        }
        
        return value;
    }
    
    void ParseIns(ParseData& data, Object& result)
    {
    MoreLabels:
        std::string tok;
        
        data.SkipSpace();
        
        if(IsDelimiter(data.PeekC()))
        {
            data.GetC();
            goto MoreLabels;
        }

        if(data.PeekC() == '+')
        {
            tok += data.GetC(); // defines global
        }
        else if(data.PeekC() == '-')
        {
            tok += data.GetC();
            goto GotLabel;
        }
        else
        {
            while(data.PeekC() == '&') tok += data.GetC();
        }
        
        if(data.PeekC() == '*')
        {
            tok += data.GetC();
            goto GotLabel;
        }
        
        // Label may not begin with ., but instruction may
        if(tok.empty() && data.PeekC() == '.')
            tok += data.GetC();
        
        for(bool first=true;; first=false)
        {
            char c = data.PeekC();
            if(isalpha(c) || c == '_'
            || (!first && isdigit(c))
            || (tok[0]=='.' && ispunct(c))
              )
                tok += data.GetC();
            else
            {
                break;
            }
        }

GotLabel:
        data.SkipSpace();
        if(tok == "+") // It's a next-branch-label
        {
            result.DefineLabel(NextBranchLabel);
            CreateNewNextBranch();
            goto MoreLabels;
        }
        if(tok == "-") // It's a prev-branch-label
        {
            CreateNewPrevBranch();
            result.DefineLabel(PrevBranchLabel);
            goto MoreLabels;
        }
        
        //std::fprintf(stderr, "Took token '%s'\n", tok.c_str());
        
        data.SkipSpace();
        
        std::vector<OpcodeChoice> choices;
        
        const struct ins *insdata = std::lower_bound(ins, ins+InsCount, tok);
        if(insdata == ins+InsCount || tok != insdata->token)
        {
            /* Other mnemonic */
            
            if(tok == ".byt")
            {
                OpcodeChoice choice;
                bool first=true, ok=true;
                for(;;)
                {
                    data.SkipSpace();
                    if(data.EOF()) break;
                    
                    if(first)
                        first=false;
                    else
                    {
                        if(data.PeekC() == ',') { data.GetC(); data.SkipSpace(); }
                    }

                    if(data.PeekC() == '"')
                    {
                        data.GetC();
                        while(!data.EOF())
                        {
                            char c = data.GetC();
                            if(c == '"') break;
                            if(c == '\\')
                            {
                                c = data.GetC();
                                if(c == 'n') c = '\n';
                                else if(c == 'r') c = '\r';
                            }
                            ins_parameter p(c);
                            choice.parameters.push_back(std::make_pair(1, p));
                        }
                        continue;
                    }
                    
                    ins_parameter p;
                    if(!ParseExpression(data, p) || !p.is_byte())
                    {
                        /* FIXME: syntax error */
                        ok = false;
                        break;
                    }
                    choice.parameters.push_back(std::make_pair(1, p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.push_back(choice);
                }
            }
            else if(tok == ".word")
            {
                OpcodeChoice choice;
                bool first=true, ok=true;
                for(;;)
                {
                    data.SkipSpace();
                    if(data.EOF()) break;
                    
                    if(first)
                        first=false;
                    else
                    {
                        if(data.PeekC() == ',') { data.GetC(); data.SkipSpace(); }
                    }
                    
                    ins_parameter p;
                    if(!ParseExpression(data, p)
                    || p.is_word().is_false())
                    {
                        /* FIXME: syntax error */
                        std::fprintf(stderr, "Syntax error at '%s'\n",
                            data.GetRest().c_str());
                        ok = false;
                        break;
                    }
                    choice.parameters.push_back(std::make_pair(2, p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.push_back(choice);
                }
            }
            else if(tok == ".long")
            {
                OpcodeChoice choice;
                bool first=true, ok=true;
                for(;;)
                {
                    data.SkipSpace();
                    if(data.EOF()) break;
                    
                    if(first)
                        first=false;
                    else
                    {
                        if(data.PeekC() == ',') { data.GetC(); data.SkipSpace(); }
                    }
                    
                    ins_parameter p;
                    if(!ParseExpression(data, p)
                    || p.is_long().is_false())
                    {
                        /* FIXME: syntax error */
                        std::fprintf(stderr, "Syntax error at '%s'\n",
                            data.GetRest().c_str());
                        ok = false;
                        break;
                    }
                    choice.parameters.push_back(std::make_pair(3, p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.push_back(choice);
                }
            }
            else if(!tok.empty() && tok[0] != '.')
            {
                // Labels may not begin with '.'
                
                if(data.PeekC() == '=')
                {
                    // Define label as something
                    
                    data.GetC(); data.SkipSpace();
                    
                    ins_parameter p;
                    if(!ParseExpression(data, p))
                    {
                        fprintf(stderr, "Expected expression: %s\n", data.GetRest().c_str());
                    }
                    
                    unsigned value = ParseConst(p, result);
                    
                    delete p.exp;
                    
                    if(tok == "*")
                    {
                        result.SetPos(value);
                    }
                    else
                    {
                        result.DefineLabel(tok, value);
                    }
                }
                else
                {
                    if(tok == "*")
                    {
                        std::fprintf(stderr,
                            "Cannot define label '*'. Perhaps you meant '*= <value>'?\n"
                                    );
                    }
                    result.DefineLabel(tok);
                }
                goto MoreLabels;
            }
            else if(!data.EOF())
            {
                std::fprintf(stderr,
                    "Error: What is '%s' - previous token: '%s'?\n",
                        data.GetRest().c_str(),
                        tok.c_str());
            }
        }
        else
        {
            /* Found mnemonic */
            
            bool something_ok = false;
            for(unsigned addrmode=0; insdata->opcodes[addrmode*3]; ++addrmode)
            {
                const std::string op(insdata->opcodes+addrmode*3, 2);
                if(op != "--")
                {
                    ins_parameter p1, p2;
                    
                    const ParseData::StateType state = data.SaveState();
                    
                    tristate valid = ParseAddrMode(data, addrmode, p1, p2);
                    if(!valid.is_false())
                    {
                        something_ok = true;
                        
                        if(op == "sb") result.StartScope();
                        else if(op == "eb") result.EndScope();
                        else if(op == "as") A_16bit = false;
                        else if(op == "al") A_16bit = true;
                        else if(op == "xs") X_16bit = false;
                        else if(op == "xl") X_16bit = true;
                        else if(op == "gt") result.SelectTEXT();
                        else if(op == "gd") result.SelectDATA();
                        else if(op == "gz") result.SelectZERO();
                        else if(op == "gb") result.SelectBSS();
                        else if(op == "li")
                        {
                            switch(addrmode)
                            {
                                case 26: // .link group 1
                                {
                                    result.Linkage.SetLinkageGroup(ParseConst(p1, result));
                                    //delete p1.exp;
                                    break;
                                }
                                case 27: // .link page $FF
                                {
                                    result.Linkage.SetLinkagePage(ParseConst(p1, result));
                                    //delete p1.exp;
                                    break;
                                }
                                default:
                                    // shouldn't happen
                                    break;
                            }
                        }
                        else
                        {
                            // Opcode in hex.
                            unsigned char opcode = std::strtol(op.c_str(), NULL, 16);
                            
                            OpcodeChoice choice;
                            unsigned op1size = GetOperand1Size(addrmode);
                            unsigned op2size = GetOperand2Size(addrmode);
                            
                            if(AddrModes[addrmode].p1 == AddrMode::tRel8)
                                p1.prefix = FORCE_REL8;
                            if(AddrModes[addrmode].p1 == AddrMode::tRel16)
                                p1.prefix = FORCE_REL16;
                            
                            choice.parameters.push_back(std::make_pair(1, opcode));
                            if(op1size)choice.parameters.push_back(std::make_pair(op1size, p1));
                            if(op2size)choice.parameters.push_back(std::make_pair(op2size, p2));

                            choice.is_certain = valid.is_true();
                            choices.push_back(choice);
                        }
#if SHOW_POSSIBLES
                        std::fprintf(stderr, "- %s mode %u (%s) (%u bytes)\n",
                            valid.is_true() ? "Is" : "Could be",
                            addrmode, op.c_str(),
                            GetOperandSize(addrmode)
                                    );
                        if(p1.exp)
                            std::fprintf(stderr, "  - p1=\"%s\"\n", p1.Dump().c_str());
                        if(p2.exp)
                            std::fprintf(stderr, "  - p2=\"%s\"\n", p2.Dump().c_str());
#endif
                    }
                    
                    data.LoadState(state);
                }
                if(!insdata->opcodes[addrmode*3+2]) break;
            }

            if(!something_ok)
            {
                std::fprintf(stderr,
                    "Error: '%s' is invalid parameter for '%s' in current context (%u choices).\n",
                        data.GetRest().c_str(),
                        tok.c_str(),
                        choices.size());
                return;
            }
        }

        if(choices.empty())
        {
            //std::fprintf(stderr, "? Confused before '%s'\n", data.GetRest().c_str());
            return;
        }

        /* Try to pick one the smallest of the certain choices */
        unsigned smallestsize = 0, smallestnum = 0;
        bool found=false;
        for(unsigned a=0; a<choices.size(); ++a)
        {
            const OpcodeChoice& c = choices[a];
            if(c.is_certain)
            {
                unsigned size = 0;
                for(unsigned b=0; b<c.parameters.size(); ++b)
                    size += c.parameters[b].first;
                if(size < smallestsize || !found)
                {
                    smallestsize = size;
                    smallestnum = a;
                    found = true;
                }
            }
        }
        if(!found)
        {
            /* If there were no certain choices, try to pick one of the uncertain ones. */
            for(unsigned a=0; a<choices.size(); ++a)
            {
                const OpcodeChoice& c = choices[a];
                
                unsigned sizediff = 0;
                for(unsigned b=0; b<c.parameters.size(); ++b)
                {
                    int diff = 2 - (int)c.parameters[b].first;
                    if(diff < 0)diff = -diff;
                    sizediff += diff;
                }
                if(sizediff < smallestsize || !found)
                {
                    smallestsize = sizediff;
                    smallestnum = a;
                    found = true;
                }
            }
        }
        if(!found)
        {
            std::fprintf(stderr, "Internal error\n");
        }
        
        OpcodeChoice& c = choices[smallestnum];
        
#if SHOW_CHOICES
        std::fprintf(stderr, "Choice %u:", smallestnum);
#endif
        for(unsigned b=0; b<c.parameters.size(); ++b)
        {
            long value = 0;
            std::string ref;
            
            unsigned size = c.parameters[b].first;
            const ins_parameter& param = c.parameters[b].second;
            
            const expression* e = param.exp;
            
            if(e->IsConst())
            {
                value = e->GetConst();
            }
            else if(const expr_label* l = dynamic_cast<const expr_label* > (e))
            {
                ref = l->GetName();
            }
            else if(const sum_group* s = dynamic_cast<const sum_group* > (e))
            {
                /* constant should always be last in a sum group. */
                
                /* sum_group always has at least 2 elements.
                 * If it has 0, it's converted to expr_number.
                 * if it has 1, it's converter into the element itself (or expr_negate).
                 */
                
                sum_group::list_t::const_iterator first = s->contents.begin();
                sum_group::list_t::const_iterator last = s->contents.end(); --last;
                
                const char *error = NULL;
                if(s->contents.size() != 2)
                {
                    error = "must have 2 elements";
                }
                else if(!last->first->IsConst()) // If 2nd isn't const
                {
                    error = "2nd elem isn't const";
                }
                else if(first->second)          // If 1st isn't positive
                {
                    error = "1st elem must not be negative";
                }
                else if(!dynamic_cast<const expr_label *> (first->first))
                {
                    error = "1st elem must be a label";
                }
                if(error)
                {
                    /* Invalid pointer arithmetic */
                    std::fprintf(stderr, "Invalid pointer arithmetic (%s): '%s'\n",
                        error,
                        e->Dump().c_str());
                    continue;
                }
               
                ref = (dynamic_cast<const expr_label *> (first->first))->GetName();
                value = last->first->GetConst();
            }
            else
            {
                fprintf(stderr, "Invalid parameter (not a label/const/label+const): '%s'\n",
                    e->Dump().c_str());
                continue;
            }
            
            char prefix = param.prefix;
            if(!prefix)
            {
                // Pick a prefix that represents what we're actually doing here
                switch(size)
                {
                    case 1: prefix = FORCE_LOBYTE; break;
                    case 2: prefix = FORCE_ABSWORD; break;
                    case 3: prefix = FORCE_LONG; break;
                    default:
                        std::fprintf(stderr, "Internal error - unknown size: %u\n",
                            size);
                }
            }
            
            if(!ref.empty())
            {
                result.AddExtern(prefix, ref, value);
                value = 0;
            }
            else if(prefix == FORCE_REL8 || prefix == FORCE_REL16)
            {
                std::fprintf(stderr, "Error: Relative target must not be a constant\n");
                // FIXME: It isn't so bad...
            }

            switch(prefix)
            {
                case FORCE_SEGBYTE:
                    result.GenerateByte((value >> 16) & 0xFF);
                    break;
                case FORCE_LONG:
                    result.GenerateByte(value & 0xFF);
                    value >>= 8;
                    //passthru
                case FORCE_ABSWORD:
                    result.GenerateByte(value & 0xFF);
                    //passthru
                case FORCE_HIBYTE:
                    value >>= 8;
                    //passthru
                case FORCE_LOBYTE:
                    result.GenerateByte(value & 0xFF);
                    break;
                
                case FORCE_REL16:
                    result.GenerateByte(0x00);
                    //passthru
                case FORCE_REL8:
                    result.GenerateByte(0x00);
                    break;
            }
            
#if SHOW_CHOICES
            std::fprintf(stderr, " %s(%u)", param.Dump().c_str(), size);
#endif
        }
#if SHOW_CHOICES
        if(c.is_certain)
            std::fprintf(stderr, " (certain)");
        std::fprintf(stderr, "\n");
#endif
        
        // *FIXME* choices not properly deallocated
    }

    void ParseLine(Object& result, const std::string& s)
    {
        // Break into statements, assemble each by each
        for(unsigned a=0; a<s.size(); )
        {
            unsigned b=a;
            bool quote = false;
            while(b < s.size())
            {
                if(quote && s[b] == '\\' && (b+1) < s.size()) ++b;
                if(s[b] == '"') quote = !quote;
                if(!quote && IsDelimiter(s[b]))break;
                ++b;
            }
            
            if(b > a)
            {
                const std::string tmp = s.substr(a, b-a);
                //std::fprintf(stderr, "Parsing '%s'\n", tmp.c_str());
                ParseData data(tmp);
                ParseIns(data, result);
            }
            a = b+1;
        }
    }
}

void AssemblePrecompiled(std::FILE *fp, Object& obj)
{
    if(!fp)
    {
        return;
    }
    
    CreateNewPrevBranch();
    CreateNewNextBranch();

    obj.StartScope();
    obj.SelectTEXT();
    
    for(;;)
    {
        char Buf[16384];
        if(!std::fgets(Buf, sizeof Buf, fp)) break;
        
        if(Buf[0] == '#')
        {
            // Probably something generated by gcc
            continue;
        }
        ParseLine(obj, Buf);
    }

    obj.EndScope();

    for(std::list<std::string>::const_iterator
        i = DefinedBranchLabels.begin();
        i != DefinedBranchLabels.end();
        ++i)
    {
        obj.UndefineLabel(*i);
    }
    DefinedBranchLabels.clear();
}
