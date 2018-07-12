#include <cctype>
#include <string>
#include <utility>
#include <vector>
#include <list>
#include <map>
#include <cassert>

#include "expr.hh"
#include "parse.hh"
#include "assemble.hh"
#include "object.hh"
#include "insdata.hh"
#include "precompile.hh"

static std::map<unsigned, std::string> PrevBranchLabel; // What "-" means (for each length of "-")
static std::map<unsigned, std::string> NextBranchLabel; // What "+" means (for each length of "+")

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
        void FlipREL8();
    };

    void OpcodeChoice::FlipREL8()
    {
        // Must have:
        //   - Two parameters
        //   - First param must be 1-byte const
        //   - Second param must be 1-byte and of type REL8

        std::vector<std::string> errors;

        if(parameters.size() != 2)
        {
            char Buf[128];
            std::sprintf(Buf, "Parameter count (%u) is not 2", (unsigned) parameters.size());
            errors.push_back(Buf);
        }
        if(parameters[0].first != 1)
        {
            char Buf[128];
            std::sprintf(Buf, "Parameter 1 is not byte (size is %u bytes)", parameters[0].first);
            errors.push_back(Buf);
        }
        if(parameters[1].first != 1)
        {
            char Buf[128];
            std::sprintf(Buf, "Parameter 2 is not byte (size is %u bytes)", parameters[1].first);
            errors.push_back(Buf);
        }
        if(!parameters[0].second.exp->IsConst())
        {
            char Buf[128];
            std::sprintf(Buf, "Parameter 1 is not const");
            errors.push_back(Buf);
        }
        if(parameters[1].second.prefix != FORCE_REL8)
        {
            char Buf[128];
            std::sprintf(Buf, "Parameter 2 is not REL8");
            errors.push_back(Buf);
        }

        if(!errors.empty())
        {
            std::fprintf(stderr, "Error: REL8-fixing when\n");
            for(unsigned a=0; a<errors.size(); ++a)
                std::fprintf(stderr, "- %s\n", errors[a].c_str());
            std::fprintf(stderr, "- Opcode:");
            for(unsigned a=0; a<parameters.size(); ++a)
            {
                std::fprintf(stderr, " (%u)%s",
                    parameters[a].first,
                    parameters[a].second.Dump().c_str());
            }
            std::fprintf(stderr, "\n");
            return;
        }

        unsigned char opcode = parameters[0].second.exp->GetConst();

        // 10 30 bpl bmi
        // 50 70 bvc bvs
        // 90 B0 bcc bcs
        // D0 F0 bne beq
        opcode ^= 0x20; // This flips the polarity

        std::vector<paramtype> newparams;
        // Insert a reverse-jump-by.
        newparams.push_back(paramtype(1, opcode));
        newparams.push_back(paramtype(1, 0x03));
        newparams.push_back(paramtype(1, 0x4C)); // Insert JMP

        ins_parameter newparam;
        newparam.prefix = FORCE_ABSWORD;
        newparam.exp    = std::move(parameters[1].second.exp);
        newparams.emplace_back(paramtype(2, std::move(newparam)));

        // FIXME: unallocate param1 here

        // Replace with the new instruction.
        parameters = std::move(newparams);
    }

    typedef std::vector<OpcodeChoice> ChoiceList;

    std::list<std::string> DefinedBranchLabels;

    void CreateNewPrevBranch(unsigned length)
    {
        static unsigned BranchNumber = 0;
        char Buf[128];
        std::sprintf(Buf, "$PrevBranch%u$%u", length,++BranchNumber);

        DefinedBranchLabels.push_back(PrevBranchLabel[length] = Buf);
    }
    void CreateNewNextBranch(unsigned length)
    {
        static unsigned BranchNumber = 0;
        char Buf[128];
        std::sprintf(Buf, "$NextBranch%u$%u", length,++BranchNumber);

        DefinedBranchLabels.push_back(NextBranchLabel[length] = Buf);
    }
    const std::string CreateNopLabel()
    {
        static unsigned BranchNumber = 0;
        char Buf[128];
        std::sprintf(Buf, "$NopLabel$%u", ++BranchNumber);

        DefinedBranchLabels.push_back(Buf);
        return Buf;
    }

    unsigned ParseConst(ins_parameter& p, const Object& obj)
    {
        unsigned value = 0;

        std::set<std::string> labels;
        FindExprUsedLabels(p.exp, labels);

        for(const std::string& label: labels)
        {
            SegmentSelection seg;
            if(obj.FindLabel(label, seg, value))
            {
                std::string before = p.exp->Dump();
                SubstituteExprLabel(p.exp, label, value);
                p.exp->Optimize(p.exp);
                /*fprintf(stderr, "Label substituted(%s)value($%X), result before(%s) after(%s)\n",
                    label.c_str(), value,
                    before.c_str(), p.exp->Dump().c_str());*/
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
            do {
                tok += data.GetC(); // defines global
            } while(data.PeekC() == '+');
            goto GotLabel;
        }
        else if(data.PeekC() == '-')
        {
            do {
                tok += data.GetC();
            } while(data.PeekC() == '-');
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
        if(!tok.empty() && tok[0] == '+') // It's a next-branch-label
        {
            unsigned length = tok.size();
            result.DefineLabel(NextBranchLabel[length]);
            CreateNewNextBranch(length);
            goto MoreLabels;
        }
        if(!tok.empty() && tok[0] == '-') // It's a prev-branch-label
        {
            unsigned length = tok.size();
            CreateNewPrevBranch(length);
            result.DefineLabel(PrevBranchLabel[length]);
            goto MoreLabels;
        }

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
                            choice.parameters.emplace_back(1, std::move(p));
                        }
                        continue;
                    }

                    ins_parameter p;
                    if(!ParseExpression(data, p) || !p.is_byte(result))
                    {
                        /* FIXME: syntax error */
                        ok = false;
                        break;
                    }
                    choice.parameters.emplace_back(1, std::move(p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.emplace_back(std::move(choice));
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
                    if(!ParseExpression(data, p) || p.is_word(result).is_false())
                    {
                        /* FIXME: syntax error */
                        std::fprintf(stderr, "Syntax error at '%s'\n",
                            data.GetRest().c_str());
                        ok = false;
                        break;
                    }
                    choice.parameters.emplace_back(2, std::move(p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.emplace_back(std::move(choice));
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
                    if(!ParseExpression(data, p) || p.is_long(result).is_false())
                    {
                        /* FIXME: syntax error */
                        std::fprintf(stderr, "Syntax error at '%s'\n",
                            data.GetRest().c_str());
                        ok = false;
                        break;
                    }
                    choice.parameters.emplace_back(3, std::move(p));
                }
                if(ok)
                {
                    choice.is_certain = true;
                    choices.push_back(std::move(choice));
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
                    //fprintf(stderr, "Label '%s' defined as %u\n", tok.c_str(), value);

                    p.exp.reset();

                    if(tok == "*") // Handles '*='
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

                    tristate valid = ParseAddrMode(data, addrmode, p1, p2, result);
                    if(!valid.is_false())
                    {
                        something_ok = true;

                        if(op == "sb") result.StartScope();
                        else if(op == "eb") result.EndScope();
                        else if(op == "gt") result.SelectTEXT();
                        else if(op == "gd") result.SelectDATA();
                        else if(op == "gz") result.SelectZERO();
                        else if(op == "gb") result.SelectBSS();
                        else if(op == "li")
                        {
                            assert(addrmode == 12 || addrmode == 13);
                            if(addrmode == 12) // .link group 1
                            {
                                result.SetLinkageGroup(ParseConst(p1, result));
                                p1.exp.reset();
                            }
                            else // .link page $FF
                            {
                                result.SetLinkagePage(ParseConst(p1, result));
                                p1.exp.reset();
                            }
                        }
                        else if(op == "np")
                        {
                            assert(addrmode == 14);

                            unsigned imm16 = ParseConst(p1, result);

                            OpcodeChoice choice;

                            if(imm16 > 3)
                            {
                                std::string NopLabel = CreateNopLabel();
                                result.DefineLabel(NopLabel, result.GetPos()+imm16);

                                // jmp
                                choice.parameters.emplace_back(1, 0x4C); // JMP

                                expression* e = new expr_label(NopLabel);
                                std::unique_ptr<expression> tmp(e);

                                p1.prefix = FORCE_ABSWORD;
                                p1.exp.swap(tmp);
                                choice.parameters.emplace_back(2, std::move(p1));

                                imm16 -= 3;
                            }

                            // Fill the rest with nops
                            for(unsigned n=0; n<imm16; ++n)
                                choice.parameters.emplace_back(1, 0xEA);

                            choice.is_certain = valid.is_true();
                            choices.emplace_back(std::move(choice));
                        }
                        else
                        {
                            // Opcode in hex.
                            unsigned char opcode = std::strtol(op.c_str(), NULL, 16);

                            OpcodeChoice choice;
                            unsigned op1size = GetOperand1Size(addrmode);
                            unsigned op2size = GetOperand2Size(addrmode);

                            if(AddrModes[addrmode].p1 == AddrMode::tRel8) p1.prefix = FORCE_REL8;

                            choice.parameters.emplace_back(1, opcode);
                            if(op1size) choice.parameters.emplace_back(op1size, std::move(p1));
                            if(op2size) choice.parameters.emplace_back(op2size, std::move(p2));

                            choice.is_certain = valid.is_true();
                            choices.emplace_back(std::move(choice));
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
                        (unsigned) choices.size());
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

        if(result.ShouldFlipHere())
        {
            //std::fprintf(stderr, "Flipping...\n");
            c.FlipREL8();
        }

#if SHOW_CHOICES
        std::fprintf(stderr, "Choice %u:", smallestnum);
#endif
        for(unsigned b=0; b<c.parameters.size(); ++b)
        {
            long value = 0;
            std::string ref;

            unsigned size              = c.parameters[b].first;
            const ins_parameter& param = c.parameters[b].second;

            const auto& e = param.exp;

            if(e->IsConst())
            {
                value = e->GetConst();
            }
            else if(const expr_label* l = dynamic_cast<const expr_label*> (e.get()))
            {
                ref = l->GetName();
            }
            else if(const sum_group* s = dynamic_cast<const sum_group*> (e.get()))
            {
                /* constant should always be last in a sum group. */

                /* sum_group always has at least 2 elements.
                 * If it has 0, it's converted to expr_number.
                 * if it has 1, it's converter into the element itself (or expr_negate).
                 */

                auto first = s->contents.begin(), last = std::prev(s->contents.end());

                std::string label;
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
                else if(const expr_label* lbl = dynamic_cast<const expr_label *> (first->first.get()))
                {
                    label = lbl->GetName();
                }
                else
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

                ref   = std::move(label);
                value = last->first->GetConst();
            }
            else
            {
                fprintf(stderr, "Invalid parameter (not a label/const/label+const): '%s'\n",
                    e->Dump().c_str());
                continue;
            }

            char prefix = param.prefix;
            /*if(!ref.empty() && !prefix)
            {
                // If the symbol is known and resides in .zero segment,
                // force the LOBYTE prefix.
                SegmentSelection seg;
                unsigned dummyvalue=0;
                std::fprintf(stderr, "Checking if %u-byte reference to '%s' is in ZERO...\n", size, ref.c_str());
                if(result.FindLabel(ref, seg, dummyvalue) && seg == ZERO && value+dummyvalue < 256)
                {
                    std::fprintf(stderr, "Reference to %s (%u) coded as lobyte\n", ref.c_str(), dummyvalue);
                    prefix = FORCE_LOBYTE;
                }
            }*/

            if(!prefix)
            {
                // Pick a prefix that represents what we're actually doing here
                switch(size)
                {
                    case 1: prefix = FORCE_LOBYTE; break;
                    case 2: prefix = FORCE_ABSWORD; break;
                    case 3: prefix = FORCE_LONG; break;
                    default:
                        std::fprintf(stderr, "Internal error - unknown size: %u\n", size);
                }
            }

            if(!ref.empty())
            {
                result.AddExtern(prefix, ref, value);
                value = 0;
            }
            else if(prefix == FORCE_REL8)
            {
                std::fprintf(stderr, "Error: Relative target must not be a constant\n");
                // FIXME: It isn't so bad...
            }

            switch(prefix)
            {
                case FORCE_SEGBYTE:
                    result.GenerateByte((value >>16) & 0xFF);
                    break;
                case FORCE_LONG:
                    result.GenerateByte((value >> 0) & 0xFF);
                    result.GenerateByte((value >> 8) & 0xFF);
                    result.GenerateByte((value >>16) & 0xFF);
                    break;
                case FORCE_ABSWORD:
                    result.GenerateByte((value >> 0) & 0xFF);
                    result.GenerateByte((value >> 8) & 0xFF);
                    break;
                case FORCE_HIBYTE:
                    result.GenerateByte((value >> 8) & 0xFF);
                    break;
                case FORCE_LOBYTE:
                    result.GenerateByte((value >> 0) & 0xFF);
                    break;
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

const std::string& GetPrevBranchLabel(unsigned length)
{
    std::string& l = PrevBranchLabel[length];
    if(l.empty()) CreateNewPrevBranch(length);
    return l;
}

const std::string& GetNextBranchLabel(unsigned length)
{
    std::string& l = NextBranchLabel[length];
    if(l.empty()) CreateNewNextBranch(length);
    return l;
}

void AssemblePrecompiled(std::FILE *fp, Object& obj)
{
    if(!fp)
    {
        return;
    }

    obj.StartScope();
    obj.SelectTEXT();

    for(;;)
    {
        char Buf[65536];
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
