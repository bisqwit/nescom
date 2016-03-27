/* xa65 object file loader and linker for C++
 * For loading and linking 65816 object files
 * Copyright (C) 1992,2016 Bisqwit (http://iki.fi/bisqwit/)
 *
 * Version 1.9.2 - Aug 18 2003, Sep 4 2003, Jan 23 2004,
 *                 Jan 31 2004, Mar 27 2016
 */

#define DEBUG_FIXUPS 0

#include <map>
#include <set>
#include <memory>

#include "o65.hh"

using std::set;
using std::map;
using std::make_pair;
using std::fprintf;
#ifndef stderr
using std::stderr;
#endif

#include "relocdata.hh"

// TODO: per-segment linkage instruction

namespace
{
    unsigned LoadWord(FILE *fp)
    {
        unsigned temp = fgetc(fp);
        if(temp == (unsigned)EOF)return temp;
        return temp | (fgetc(fp) << 8);
    }
    unsigned LoadDWord(FILE *fp)
    {
        unsigned temp = LoadWord(fp);
        if(temp == (unsigned)EOF) return temp;
        unsigned temp2 = LoadWord(fp);
        if(temp2 == (unsigned)EOF) return temp2;
        return temp | (temp2 << 16);
    }
    unsigned LoadSWord(FILE *fp, bool use32=false)
    {
        return use32 ? LoadDWord(fp) : LoadWord(fp);
    }
    unsigned long LoadVar(FILE* fp) // cc65
    {
        unsigned long V=0, Shift=0, C;
        do V |= ((unsigned long)(C = std::fgetc(fp)) & 0x7F) << (Shift++)*7; while(C & 0x80);
        return V;
    }
    long LoadSDWord(FILE *fp) { return (int)LoadSWord(fp,true); }
    std::string LoadRaw(FILE* fp, std::size_t size)
    {
        std::string data(size, '\0');
        for(std::size_t a=0; a<size; ++a) data[a] = fgetc(fp);
        return data;
    }
    std::string LoadZString(FILE* fp)
    {
        std::string varname;
        while(int c = fgetc(fp)) { if(c==EOF)break; varname += (char) c; }
        return varname;
    }
    void LoadRawTo(FILE* fp, std::size_t size, unsigned char* target)
    {
        std::fread(target, size, 1, fp);
    }
}

class O65::Defs
{
    std::map<std::string, unsigned> symno;
    std::map<unsigned, std::string> nosym;
    std::set<unsigned> undefines;
    std::map<unsigned, unsigned> defines;
public:
    Defs(): symno(), nosym(), undefines(), defines()
    {
    }

    unsigned AddUndefined(const std::string& name)
    {
        unsigned a = symno.size();
        undefines.insert(a);
        symno[name] = a;
        nosym[a] = name;
        return a;
    }
    unsigned GetSymno(const std::string& name) const
    {
        std::map<std::string, unsigned>::const_iterator i = symno.find(name);
        if(i == symno.end()) return ~0U;
        return i->second;
    }
    bool IsDefined(unsigned a) const
    {
        return defines.find(a) != defines.end();
    }
    unsigned GetValue(unsigned a) const
    {
        return defines.find(a)->second;
    }
    void Define(unsigned a, unsigned value)
    {
        undefines.erase(a);
        defines[a] = value;
    }
    const vector<std::string> GetExternList() const
    {
        vector<std::string> result;
        result.reserve(undefines.size());
        for(std::set<unsigned>::const_iterator
            i = undefines.begin(); i != undefines.end(); ++i)
        {
            const std::string& name = nosym.find(*i)->second;
            result.push_back(name);
        }
        return result;
    }
    void DumpUndefines() const
    {
        for(std::set<unsigned>::const_iterator
            i = undefines.begin(); i != undefines.end(); ++i)
        {
            const std::string& name = nosym.find(*i)->second;

            fprintf(stderr, "Symbol %s is still not defined\n",
                name.c_str());
        }
    }
};

class O65::Segment
{
public:
    vector<unsigned char> space;

    //! where it is assumed to start
    unsigned base;

    //! absolute addresses of all publics
    typedef map<std::string, unsigned> publicmap_t;
    publicmap_t publics;

public:
    typedef Relocdata<unsigned> RT; RT R;
public:
    Segment(): space(), base(0), publics(), R()
    {
    }
private:
    friend class O65;
    void Locate(SegmentSelection seg, unsigned diff, bool is_me);
    void LocateSym(unsigned symno, unsigned newaddress);
    void LoadRelocations(FILE* fp);
};

O65::O65()
   : customheaders(),
     defs(new Defs),
     code(NULL),
     data(NULL),
     zero(NULL),
     bss(NULL),
     error(false)
{
}

O65::~O65()
{
    delete code;
    delete data;
    delete zero;
    delete bss;
    delete defs;
}

O65::O65(const O65& b)
    : customheaders(),
      defs(new Defs(*b.defs)),
      code(b.code ? new Segment(*b.code) : NULL),
      data(b.data ? new Segment(*b.data) : NULL),
      zero(b.zero ? new Segment(*b.zero) : NULL),
      bss(b.bss ? new Segment(*b.bss) : NULL),
      error(b.error)
{
}
const O65& O65::operator= (const O65& b)
{
    if(&b == this) return *this;
    delete code; code = b.code ? new Segment(*b.code) : NULL;
    delete data; data = b.data ? new Segment(*b.data) : NULL;
    delete zero; zero = b.zero ? new Segment(*b.zero) : NULL;
    delete bss; bss = b.bss ? new Segment(*b.bss) : NULL;
    delete defs; defs = new Defs(*b.defs);
    return *this;
}

void O65::Load(FILE* fp)
{
    rewind(fp);

    if(this->code) delete this->code;
    if(this->data) delete this->data;
    if(this->zero) delete this->zero;
    if(this->bss) delete this->bss;
    this->code = new Segment;
    this->data = new Segment;
    this->zero = new Segment;
    this->bss = new Segment;

    // Read header
    switch(LoadDWord(fp))
    {
        case 0x616E7A55: // cc65 object file
        {
            LoadWord(fp); // version
            unsigned flags = LoadWord(fp);
            unsigned OptionOffs   = LoadDWord(fp);
            /*unsigned OptionSize=*/LoadDWord(fp);
            unsigned FileOffs     = LoadDWord(fp);
            /*unsigned FileSize=*/  LoadDWord(fp);
            unsigned SegOffs      = LoadDWord(fp);
            /*unsigned SegSize=*/   LoadDWord(fp);
            unsigned ImportOffs   = LoadDWord(fp);
            /*unsigned ImportSize=*/LoadDWord(fp);
            unsigned ExportOffs   = LoadDWord(fp);
            /*unsigned ExportSize=*/LoadDWord(fp);
            unsigned DbgSymOffs   = LoadDWord(fp);
            /*unsigned DbgSymSize=*/LoadDWord(fp);
            unsigned LineInfoOffs = LoadDWord(fp);
            /*unsigned LineInfoSize=*/LoadDWord(fp);
            unsigned StrPoolOffs  = LoadDWord(fp);
            /*unsigned StrPoolSize=*/LoadDWord(fp);
            unsigned AssertOffs   = LoadDWord(fp);
            /*unsigned AssertSize=*/LoadDWord(fp);
            unsigned ScopeOffs    = LoadDWord(fp);
            /*unsigned ScopeSize=*/ LoadDWord(fp);
            unsigned SpanOffs     = LoadDWord(fp);
            /*unsigned SpanSize=*/  LoadDWord(fp);

            enum exprtype : unsigned char {
               EXPR_NULL=0, EXPR_LITERAL=0x81, EXPR_SYMBOL=0x82, EXPR_SECTION=0x83, EXPR_BANK=0x87,
               /* Binary operations, left and right hand sides are valid */
               EXPR_PLUS=0x01,EXPR_MINUS=0x02,EXPR_MUL=0x03,EXPR_DIV=0x04,EXPR_MOD=0x05,
               EXPR_OR=0x06,EXPR_XOR=0x07,EXPR_AND=0x08,EXPR_SHL=0x09,EXPR_SHR=0x0A,EXPR_EQ=0x0B,
               EXPR_NE=0x0C,EXPR_LT=0x0D,EXPR_GT=0x0E,EXPR_LE=0x0F,EXPR_GE=0x10,EXPR_BOOLAND=0x11,
               EXPR_BOOLOR=0x12,EXPR_BOOLXOR=0x13,EXPR_MAX=0x14,EXPR_MIN=0x15,
               /* Unary operations, right hand side is empty */
               EXPR_UNARY_MINUS=0x41,EXPR_NOT=0x42,EXPR_SWAP=0x43,EXPR_BOOLNOT=0x44,
               EXPR_BYTE0=0x48,EXPR_BYTE1=0x49,EXPR_BYTE2=0x4A,EXPR_BYTE3=0x4B,EXPR_WORD0=0x4C,
               EXPR_WORD1=0x4D,EXPR_FARADDR=0x4E,EXPR_DWORD=0x4F
            };
            struct ExprNode
            {
                exprtype op=EXPR_NULL;
                long ival=0;       // literal signed value
                unsigned index=0;  // Reference to imports[] or reference to sections[]
                std::unique_ptr<ExprNode> left, right;
                void Load(FILE* fp)
                {
                    op = (exprtype) std::fgetc(fp); if(op == EXPR_NULL) return; // null node
                    if(op == EXPR_LITERAL)     ival = LoadSDWord(fp);
                    else if(op == EXPR_SYMBOL) index  = LoadVar(fp); // References imports[]
                    else if(op == EXPR_SECTION || op == EXPR_BANK) { index = LoadVar(fp); } // References sections[]
                    else if(!(op & 0x80)) { left = std::make_unique<ExprNode>(); left->Load(fp);
                                            right = std::make_unique<ExprNode>(); right->Load(fp); }
                }
                std::string Dump() const
                {
                    switch(op)
                    {
                        case EXPR_NULL: return "null";
                        case EXPR_LITERAL: { char Buf[64]; std::sprintf(Buf, "%ld",ival); return Buf; }
                        case EXPR_SYMBOL: { char Buf[64]; std::sprintf(Buf, "sym%d", index); return Buf; }
                        case EXPR_SECTION:  { char Buf[64]; std::sprintf(Buf, "sect%d", index); return Buf; }
                        case EXPR_BANK:  { char Buf[64]; std::sprintf(Buf, "bank%d", index); return Buf; }
                        case EXPR_PLUS: return "(" + left->Dump() + "+" + right->Dump() + ")";
                        case EXPR_MINUS: return "(" + left->Dump() + "-" + right->Dump() + ")";
                        case EXPR_MUL: return "(" + left->Dump() + "*" + right->Dump() + ")";
                        case EXPR_DIV: return "(" + left->Dump() + "/" + right->Dump() + ")";
                        case EXPR_MOD: return "(" + left->Dump() + "%" + right->Dump() + ")";
                        case EXPR_OR: return "(" + left->Dump() + "|" + right->Dump() + ")";
                        case EXPR_XOR: return "(" + left->Dump() + "^" + right->Dump() + ")";
                        case EXPR_AND: return "(" + left->Dump() + "&" + right->Dump() + ")";
                        case EXPR_SHL: return "(" + left->Dump() + "<<" + right->Dump() + ")";
                        case EXPR_SHR: return "(" + left->Dump() + ">>" + right->Dump() + ")";
                        case EXPR_EQ: return "(" + left->Dump() + "=" + right->Dump() + ")";
                        case EXPR_NE: return "(" + left->Dump() + "!=" + right->Dump() + ")";
                        case EXPR_LT: return "(" + left->Dump() + "<" + right->Dump() + ")";
                        case EXPR_GT: return "(" + left->Dump() + ">" + right->Dump() + ")";
                        case EXPR_LE: return "(" + left->Dump() + "<=" + right->Dump() + ")";
                        case EXPR_GE: return "(" + left->Dump() + ">=" + right->Dump() + ")";
                        case EXPR_BOOLAND: return "(" + left->Dump() + "&&" + right->Dump() + ")";
                        case EXPR_BOOLOR: return "(" + left->Dump() + "||" + right->Dump() + ")";
                        case EXPR_BOOLXOR: return "(" + left->Dump() + "^^" + right->Dump() + ")";
                        case EXPR_MAX: return "max(" + left->Dump() + "," + right->Dump() + ")";
                        case EXPR_MIN: return "min(" + left->Dump() + "," + right->Dump() + ")";
                        case EXPR_UNARY_MINUS: return "-(" + left->Dump() + ")";
                        case EXPR_NOT: return "!(" + left->Dump() + ")";
                        case EXPR_SWAP: return "SWAP(" + left->Dump() + ")";
                        case EXPR_BOOLNOT: return "not(" + left->Dump() + ")";
                        case EXPR_BYTE0: return "<(" + left->Dump() + ")";
                        case EXPR_BYTE1: return ">(" + left->Dump() + ")";
                        case EXPR_BYTE2: return "^(" + left->Dump() + ")";
                        case EXPR_BYTE3: return "byte3(" + left->Dump() + ")";
                        case EXPR_WORD0: return "!(" + left->Dump() + ")";
                        case EXPR_WORD1: return "word1(" + left->Dump() + ")";
                        case EXPR_FARADDR: return "far(" + left->Dump() + ")";
                        case EXPR_DWORD:   return "dword(" + left->Dump() + ")";
                        default: { char Buf[64]; std::sprintf(Buf, "??op%02X(", op); return Buf + left->Dump() + "," + right->Dump() + ")"; }
                    }
                }
            };
            struct Context
            {
                struct Section
                {
                    Segment*         seg;
                    SegmentSelection segsel;
                    unsigned         offset;
                    struct Expr
                    {
                        ExprNode expr;
                        unsigned pc;
                        unsigned bytes;
                    };
                    std::vector<Expr> expressions;
                };
                struct Public
                {
                    bool          debug    = false;
                    unsigned      sym_type = 0;
                    unsigned char AddrSize = 0;
                    unsigned long size = ~0u;
                    unsigned ImportId = ~0u;
                    unsigned ExportId = ~0u;
                    std::string   name;
                    ExprNode      expr;
                };
                std::vector<Section>     sections;
                std::vector<std::string> imports;
                std::vector<Public>      publics;
            } context;
            struct ParsedExpr
            {
                long val=0;
                bool toocomplex=false;
                unsigned secref=~0u;
                unsigned extref=~0u;
                void Parse(const ExprNode& e, const Context& context, bool forgive=false, int sign=1)
                {
                    switch(e.op)
                    {
                        case EXPR_WORD0: case EXPR_WORD1:
                        case EXPR_BYTE0: case EXPR_BYTE1: case EXPR_BYTE2: case EXPR_BYTE3:
                        case EXPR_DWORD:
                        case EXPR_FARADDR:
                            if(forgive) Parse(*e.left, context, false, sign);
                            toocomplex = true;
                            return;
                        case EXPR_LITERAL: val += sign * e.ival; break;
                        case EXPR_SYMBOL:
                        {
                            extref = e.index;
                            break;
                        }
                        case EXPR_SECTION:
                        case EXPR_BANK:
                        {
                            const auto& section = context.sections[e.index];
                            val += sign * (section.seg->base + section.offset);
                            secref = e.index;
                            break;
                        }
                        case EXPR_PLUS:  Parse(*e.left, context,  sign); Parse(*e.right, context,  sign); break;
                        case EXPR_MINUS: Parse(*e.left, context,  sign); Parse(*e.right, context,  -sign); break;
                        case EXPR_UNARY_MINUS: Parse(*e.left, context,  -sign); break;
                        default: toocomplex = true;
                    }
                }
            };

            // Load string pool
            std::vector<std::string> str;
            std::fseek(fp, StrPoolOffs, SEEK_SET);
            for(unsigned count=LoadVar(fp); count--; str.emplace_back(LoadRaw(fp,LoadVar(fp)))) {}
            // Load externs (imports)
            std::fseek(fp, ImportOffs, SEEK_SET);
            for(unsigned count=LoadVar(fp); count--; )
            {
                /*unsigned char AddrSize =*/ std::fgetc(fp); // unused. 1 means zp, 2 absolute
                const auto&   name     = str[LoadVar(fp)];
                for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 1
                for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 2
                context.imports.push_back(name);
                defs->AddUndefined(name);
            }
            // Load debug symbols
            //if(flags & 1) // OBJ_FLAGS_DBGINFO
            {
                std::fseek(fp, DbgSymOffs, SEEK_SET);
                for(unsigned count=LoadVar(fp); count--; )
                {
                    Context::Public pub;
                    pub.debug = true;
                    pub.sym_type           = LoadVar(fp);
                    pub.AddrSize           = std::fgetc(fp);
                    /*unsigned long owner =*/ LoadVar(fp);
                    pub.name               = str[LoadVar(fp)];
                    if(pub.sym_type & 0x10)
                    {
                        pub.expr.Load(fp);
                    }
                    else
                    {
                        pub.expr.op   = EXPR_LITERAL;
                        pub.expr.ival = LoadDWord(fp);
                    }
                    if(pub.sym_type & 0x08) pub.size = LoadVar(fp); // Load size if available
                    if(pub.sym_type & 0x100) pub.ImportId = LoadVar(fp);
                    if(pub.sym_type &  0x80) pub.ExportId = LoadVar(fp);
                    for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 1
                    for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 2
                    context.publics.push_back(std::move(pub));
                }
            }
            // Load segments
            std::fseek(fp, SegOffs, SEEK_SET);
            for(unsigned count=LoadVar(fp); count--; )
            {
                unsigned long DataSize = LoadDWord(fp);
                unsigned long NextSeg  = std::ftell(fp) + DataSize;
                std::string SegmentName = str[LoadVar(fp)];
                unsigned      Flags    = LoadVar(fp);
                /*unsigned long Size =*/ LoadVar(fp);
                /*unsigned long Align =*/LoadVar(fp);
                unsigned char AddrSize = std::fgetc(fp);

                SegmentSelection segsel = DATA;
                if(Flags)
                {
                    if(Flags & 0x0001) segsel = CODE;      // Read-only segment
                    else if(Flags & 0x0004) segsel = ZERO; // Zero-page segment (these also have &2 set)
                    else if(Flags & 0x0002) segsel = BSS;  // BSS-style segment
                    else segsel = DATA;                    // Data segment...
                }
                else
                {
                    if(AddrSize == 1) segsel = ZERO;
                    else if(SegmentName == "BSS") segsel = BSS;
                    else if(SegmentName == "DATA") segsel = DATA;
                    else segsel = CODE;
                }
                Segment* seg = *GetSegRef(segsel);
                unsigned seg_start = seg->space.size();
                Context::Section section;
                section.seg    = seg;
                section.segsel = segsel;
                section.offset = seg_start;

                /*fprintf(stderr, "Section %u size %lu align %lu addrsize %u: Name(%s) Flags %04X chosen=%u Begins at %u\n",
                    unsigned(context.sections.size()), Size,Align,AddrSize,
                    SegmentName.c_str(),Flags,unsigned(segsel),seg_start);*/
                unsigned frag_start = seg_start;
                for(unsigned NumFrags = LoadVar(fp); NumFrags--; )
                {
                    //std::fprintf(stderr, "Frag at filepos %X\n", std::ftell(fp));
                    unsigned char FragType = std::fgetc(fp);
                    unsigned char Bytes    = FragType & 7;
                    switch(FragType & 0x38)
                    {
                        case 0x00: // FRAG_LITERAL - Literal data
                        case 0x20: // FRAG_FILL  - Fill bytes
                        {
                            unsigned Size = LoadVar(fp);
                            //fprintf(stderr, "Fragment type %02X bytes=%d at %04X\n", FragType,Size, frag_start);

                            seg->space.resize(frag_start + Size);
                            if((FragType & 0x38) == 0x00) // Don't read if FRAG_FILL
                            {
                                LoadRawTo(fp, Size, &seg->space[frag_start]);
                            }
                            frag_start += Size;
                            break;
                        }
                        case 0x08: // FRAG_EXPR  - Unsigned expression
                        case 0x10: // FRAG_SEXPR - Signed expression
                        {
                            Context::Section::Expr ex;
                            ex.expr.Load(fp);
                            ex.pc    = frag_start;
                            ex.bytes = Bytes;
                            //fprintf(stderr, "Fragment type %02X bytes=%d at %04X expr=%s\n", FragType,Bytes, frag_start, ex.expr.Dump().c_str());

                            section.expressions.emplace_back(std::move(ex));
                            seg->space.resize(frag_start + Bytes);
                            frag_start += Bytes;
                            break;
                        }
                        default:
                            fprintf(stderr, "Unknown fragment type %02X\n", FragType&0x38);
                    }
                    for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list
                }
                context.sections.emplace_back(std::move(section));
                std::fseek(fp, NextSeg, SEEK_SET);
            }
            // Load publics (exports)
            std::fseek(fp, ExportOffs, SEEK_SET);
            for(unsigned count=LoadVar(fp); count--; )
            {
                Context::Public pub;
                pub.sym_type           = LoadVar(fp);
                pub.AddrSize           = std::fgetc(fp);
                /*std::string Condes =*/ LoadRaw(fp, pub.sym_type&7);
                pub.name               = str[LoadVar(fp)];

                if(pub.sym_type & 0x10) // expression
                {
                    pub.expr.Load(fp);
                }
                else // const
                {
                    pub.expr.op   = EXPR_LITERAL;
                    pub.expr.ival = LoadDWord(fp);
                }
                if(pub.sym_type & 0x08) pub.size = LoadVar(fp); // Load size if available

                for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 1
                for(unsigned c=LoadVar(fp); c--; LoadVar(fp)); // Skip line info list 2
                context.publics.push_back(std::move(pub));
            }

            // Convert the publics
            for(auto& pub: context.publics)
            {
                SegmentSelection seg = CODE; // TODO which seg?
                if(pub.AddrSize == 1) seg = ZERO;
                else if(!(pub.sym_type & 0x20)) seg = DATA; // If it's not a label, but an equate

                ParsedExpr pars;
                pars.Parse(pub.expr, context);
                if(pars.secref != ~0u)
                {
                    seg = context.sections[pars.secref].segsel;
                }
                // Constant value
                DeclareGlobal(seg, pub.name, pars.val);
            }
            // Convert the expressions in sections into relocations
            for(auto& section: context.sections)
            {
                for(auto& e: section.expressions)
                {
                    Segment& seg = *section.seg;
                    ParsedExpr pars;
                    pars.Parse(e.expr, context, true);

                    unsigned long binval = pars.val;
                    switch(e.expr.op)
                    {
                        default:
                        case EXPR_DWORD:
                        case EXPR_FARADDR:
                        case EXPR_WORD0:
                        case EXPR_BYTE0: break;
                        case EXPR_BYTE1: binval >>= 8; break;
                        case EXPR_WORD1:
                        case EXPR_BYTE2: binval >>= 16; break;
                        case EXPR_BYTE3: binval >>= 24; break;
                    }
                    for(unsigned c = 0; c < e.bytes; ++c)
                        seg.space[e.pc + c] = (binval >> (c*8)) & 0xFF;

                    /*fprintf(stderr, "Converting complex=%d binval=%lu(%ld) extref=%d secref=%d\n",
                        pars.toocomplex,
                        binval,pars.val,pars.extref,pars.secref);*/
                    if(pars.extref != ~0u) // refers to import?
                    {
                        unsigned symno = defs->GetSymno(context.imports[pars.extref]);
                        if(e.expr.op == EXPR_BYTE0)      { seg.R.R16lo.AddReloc(e.pc, symno); }      // Use R16lo
                        else if(e.expr.op == EXPR_BYTE1) { seg.R.R16hi.AddReloc({e.pc, pars.val & 0xFF}, symno); } // Use R16hi
                        else if(e.expr.op == EXPR_BYTE2) { seg.R.R24seg.AddReloc({e.pc, pars.val & 0xFFFF}, symno); } // Use R24seg
                        else if(e.bytes == 1)            { seg.R.R16lo.AddReloc(e.pc, symno); } // Use R16lo
                        else if(e.bytes == 2)            { seg.R.R16.AddReloc(e.pc, symno); } // Use R16
                        else if(e.bytes == 3)            { seg.R.R24.AddReloc(e.pc, symno); } // Use R24
                    }
                    else if(pars.secref != ~0u) // refers to section?
                    {
                        auto segid = context.sections[pars.secref].segsel;
                        if(e.expr.op == EXPR_BYTE0)      { seg.R.R16lo.AddFixup(segid, e.pc); }      // Use R16lo
                        else if(e.expr.op == EXPR_BYTE1) { seg.R.R16hi.AddFixup(segid, {e.pc, pars.val & 0xFF}); } // Use R16hi
                        else if(e.expr.op == EXPR_BYTE2) { seg.R.R24seg.AddFixup(segid, {e.pc, pars.val & 0xFFFF}); } // Use R24seg
                        else if(e.bytes == 1)            { seg.R.R16lo.AddFixup(segid, e.pc); } // Use R16lo
                        else if(e.bytes == 2)            { seg.R.R16.AddFixup(segid, e.pc); } // Use R16
                        else if(e.bytes == 3)            { seg.R.R24.AddFixup(segid, e.pc); } // Use R24
                    }
                } //expr
            } //sect
            break;
        }
        case 0x366F0001: // o65 file
        {
            // Skip rest of header
            LoadWord(fp) /*== 0x3500*/;
            // Skip mode
            unsigned mode = LoadWord(fp);

            /* FIXME: No validity checks here */

            bool use32 = mode & 0x2000;

            this->code->base = LoadSWord(fp, use32);
            this->code->space.resize(LoadSWord(fp, use32));

            this->data->base = LoadSWord(fp, use32);
            this->data->space.resize(LoadSWord(fp, use32));

            this->bss->base = LoadSWord(fp, use32);
            this->bss->space.resize(LoadSWord(fp, use32));

            this->zero->base = LoadSWord(fp, use32);
            this->zero->space.resize(LoadSWord(fp, use32));

            LoadSWord(fp, use32); // Skip stack_len

            //fprintf(stderr, "@%X: stack len\n", ftell(fp));

            // Skip some headers
            for(;;)
            {
                unsigned len = fgetc(fp);
                if(!len || len == (unsigned)EOF)break;

                len &= 0xFF;

                unsigned char type = fgetc(fp);
                if(len >= 2) len -= 2; else len = 0;

                std::string data = LoadRaw(fp, len);
                //fprintf(stderr, "Custom header %u: '%.*s'\n", type, len, data.data());
                customheaders.push_back(make_pair(type, data));
            }

            LoadRawTo(fp, this->code->space.size(), &this->code->space[0]);
            LoadRawTo(fp, this->data->space.size(), &this->data->space[0]);
            // zero and bss Segments don't exist in o65 format.

            // load external symbols

            unsigned num_und = LoadSWord(fp, use32);

            //fprintf(stderr, "@%X: %u externs..\n", ftell(fp), num_und);

            for(unsigned a=0; a<num_und; ++a)
                defs->AddUndefined( LoadZString(fp) );

            //fprintf(stderr, "@%X: code relocs..\n", ftell(fp));

            code->LoadRelocations(fp);

            //fprintf(stderr, "@%X: data relocs..\n", ftell(fp));

            data->LoadRelocations(fp);
            // relocations don't exist for zero/bss in o65 format.

            unsigned num_global = LoadSWord(fp, use32);

            //fprintf(stderr, "@%X: %u globals\n", ftell(fp), num_global);

            for(unsigned a=0; a<num_global; ++a)
            {
                std::string varname = LoadZString(fp);

                SegmentSelection seg = (SegmentSelection)fgetc(fp);

                unsigned value = LoadSWord(fp, use32);

                DeclareGlobal(seg, varname, value);
            }

            break;
        }
        default:
            // TODO: Report invalid format
            break;
    }
}

void O65::DeclareGlobal(SegmentSelection seg, const std::string& name, unsigned address)
{
    if(Segment**s = GetSegRef(seg))
    {
        (*s)->publics[name] = address;
    }
}

void O65::Locate(SegmentSelection seg, unsigned newaddress)
{
    Segment**s = GetSegRef(seg);
    if(!s) return;

    unsigned diff = newaddress - (*s)->base;

    if(code) code->Locate(seg, diff, seg==CODE);
    if(data) data->Locate(seg, diff, seg==DATA);
    if(zero) zero->Locate(seg, diff, seg==ZERO);
    if(bss)   bss->Locate(seg, diff, seg==BSS);
}

unsigned O65::GetBase(SegmentSelection seg) const
{
    const Segment*const *s = GetSegRef(seg);
    if(!s) return 0;

    return (*s)->base;
}

void O65::LinkSym(const std::string& name, unsigned value)
{
    unsigned symno = defs->GetSymno(name);
    if(symno == ~0U)
    {
        fprintf(stderr, "O65: Attempt to define unknown symbol '%s' as %X\n",
            name.c_str(), value);
        return;
    }
    if(defs->IsDefined(symno))
    {
        unsigned oldvalue = defs->GetValue(symno);

        fprintf(stderr, "O65: Attempt to redefine symbol '%s' as %X, old value %X\n",
            name.c_str(),
            value,
            oldvalue
               );
    }
    if(code) code->LocateSym(symno, value);
    if(data) data->LocateSym(symno, value);
    if(zero) zero->LocateSym(symno, value);
    if(bss) bss->LocateSym(symno, value);

    defs->Define(symno, value);
}

void O65::Segment::Locate(SegmentSelection seg, unsigned diff, bool is_me)
{
    if(is_me)
    {
        /* Relocate publics */
        map<std::string, unsigned>::iterator i;
        for(i = publics.begin(); i != publics.end(); ++i)
        {
            i->second += diff;
        }
    }

    /* Fix all references to symbols in the given seg */
    for(unsigned a=0; a<R.R16.Fixups.size(); ++a)
    {
        if(R.R16.Fixups[a].first != seg) continue;
        unsigned addr = R.R16.Fixups[a].second - base;
        unsigned oldvalue = space[addr] | (space[addr+1] << 8);
        unsigned newvalue = oldvalue + diff;

#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %04X with %04X at %06X\n", oldvalue,newvalue&65535, addr);
#endif
        space[addr] = newvalue&255;
        space[addr+1] = (newvalue>>8) & 255;
    }
    for(unsigned a=0; a<R.R16lo.Fixups.size(); ++a)
    {
        if(R.R16lo.Fixups[a].first != seg) continue;
        unsigned addr = R.R16lo.Fixups[a].second - base;
        unsigned oldvalue = space[addr];
        unsigned newvalue = oldvalue + diff;
#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X at %06X\n", oldvalue,newvalue&255, addr);
#endif
        space[addr] = newvalue & 255;
    }
    for(unsigned a=0; a<R.R16hi.Fixups.size(); ++a)
    {
        if(R.R16hi.Fixups[a].first != seg) continue;
        unsigned addr = R.R16hi.Fixups[a].second.first - base;
        unsigned oldvalue = (space[addr] << 8) | R.R16hi.Fixups[a].second.second;
        unsigned newvalue = oldvalue + diff;
#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X at %06X\n", space[addr],(newvalue>>8)&255, addr);
#endif
        space[addr] = (newvalue>>8) & 255;
    }
    for(unsigned a=0; a<R.R24.Fixups.size(); ++a)
    {
        if(R.R24.Fixups[a].first != seg) continue;
        unsigned addr = R.R24.Fixups[a].second - base;
        unsigned oldvalue = space[addr] | (space[addr+1] << 8) | (space[addr+2] << 16);
        unsigned newvalue = oldvalue + diff;

#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %06X with %06X at %06X\n", oldvalue,newvalue, addr);
#endif
        space[addr] = newvalue&255;
        space[addr+1] = (newvalue>>8) & 255;
        space[addr+2] = (newvalue>>16) & 255;
    }
    for(unsigned a=0; a<R.R24seg.Fixups.size(); ++a)
    {
        if(R.R24seg.Fixups[a].first != seg) continue;
        unsigned addr = R.R24seg.Fixups[a].second.first - base;
        unsigned oldvalue = (space[addr] << 16) | R.R24seg.Fixups[a].second.second;
        unsigned newvalue = oldvalue + diff;

#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X at %06X\n", space[addr],newvalue>>16, addr);
#endif
        space[addr] = (newvalue>>16) & 255;
    }

    // Update the relocs and fixups so that they are still usable
    // after this operation. Enables also subsequent Locate() calls.

    // This updates the position of each reloc & fixup.

    if(is_me)
    {
        for(unsigned a=0; a<R.R16.Relocs.size(); ++a)       R.R16.Relocs[a].first += diff;
        for(unsigned a=0; a<R.R24.Relocs.size(); ++a)       R.R24.Relocs[a].first += diff;
        for(unsigned a=0; a<R.R16lo.Relocs.size(); ++a)   R.R16lo.Relocs[a].first += diff;
        for(unsigned a=0; a<R.R16hi.Relocs.size(); ++a)   R.R16hi.Relocs[a].first.first += diff;
        for(unsigned a=0; a<R.R24seg.Relocs.size(); ++a) R.R24seg.Relocs[a].first.first += diff;

        for(unsigned a=0; a<R.R16.Fixups.size(); ++a)       R.R16.Fixups[a].second += diff;
        for(unsigned a=0; a<R.R24.Fixups.size(); ++a)       R.R24.Fixups[a].second += diff;
        for(unsigned a=0; a<R.R16lo.Fixups.size(); ++a)   R.R16lo.Fixups[a].second += diff;
        for(unsigned a=0; a<R.R16hi.Fixups.size(); ++a)   R.R16hi.Fixups[a].second.first += diff;
        for(unsigned a=0; a<R.R24seg.Fixups.size(); ++a) R.R24seg.Fixups[a].second.first += diff;

        base += diff;
    }
}

void O65::Segment::LocateSym(unsigned symno, unsigned value)
{
    /* Locate an external symbol */

    /* Fix all references to it */
    for(unsigned a=0; a<R.R16.Relocs.size(); ++a)
    {
        if(R.R16.Relocs[a].second != symno) continue;
        unsigned addr = R.R16.Relocs[a].first - base;
        unsigned oldvalue = space[addr] | (space[addr+1] << 8);
        unsigned newvalue = oldvalue + value;
#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %04X with %04X for sym %u\n", oldvalue,newvalue&65535, symno);
#endif
        space[addr] = newvalue&255;
        space[addr+1] = (newvalue>>8) & 255;
    }
    for(unsigned a=0; a<R.R16lo.Relocs.size(); ++a)
    {
        if(R.R16lo.Relocs[a].second != symno) continue;
        unsigned addr = R.R16lo.Relocs[a].first - base;
        unsigned oldvalue = space[addr];
        unsigned newvalue = oldvalue + value;
#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X for sym %u\n", oldvalue,newvalue&255, symno);
#endif
        space[addr] = newvalue & 255;
    }
    for(unsigned a=0; a<R.R16hi.Relocs.size(); ++a)
    {
        if(R.R16hi.Relocs[a].second != symno) continue;
        unsigned addr = R.R16hi.Relocs[a].first.first - base;
        unsigned oldvalue = (space[addr] << 8) | R.R16hi.Relocs[a].first.second;
        unsigned newvalue = oldvalue + value;
#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X for sym %u\n",
            space[addr],(newvalue>>8)&255, symno);
#endif
        space[addr] = (newvalue>>8) & 255;
    }
    for(unsigned a=0; a<R.R24.Relocs.size(); ++a)
    {
        if(R.R24.Relocs[a].second != symno) continue;
        unsigned addr = R.R24.Relocs[a].first - base;
        unsigned oldvalue = space[addr] | (space[addr+1] << 8) | (space[addr+2] << 16);
        unsigned newvalue = oldvalue + value;

#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %06X with %06X for sym %u\n", oldvalue,newvalue, symno);
#endif
        space[addr] = newvalue&255;
        space[addr+1] = (newvalue>>8) & 255;
        space[addr+2] = (newvalue>>16) & 255;
    }
    for(unsigned a=0; a<R.R24seg.Relocs.size(); ++a)
    {
        if(R.R24seg.Relocs[a].second != symno) continue;
        unsigned addr = R.R24seg.Relocs[a].first.first - base;
        unsigned oldvalue = (space[addr] << 16) | R.R24seg.Relocs[a].first.second;
        unsigned newvalue = oldvalue + value;

#if DEBUG_FIXUPS
        fprintf(stderr, "Replaced %02X with %02X for sym %u\n", space[addr],newvalue>>16, symno);
#endif
        space[addr] = (newvalue>>16) & 255;
    }
}

const vector<unsigned char>& O65::GetSeg(SegmentSelection seg) const
{
    if(const Segment*const *s = GetSegRef(seg))
    {
        return (*s)->space;
    }
    return code->space;
}

unsigned O65::GetSegSize(SegmentSelection seg) const
{
    if(const Segment*const *s = GetSegRef(seg))
    {
        return (*s)->space.size();
    }
    return 0;
}

unsigned O65::GetSymAddress(SegmentSelection seg, const std::string& name) const
{
    if(const Segment*const *s = GetSegRef(seg))
    {
        Segment::publicmap_t::const_iterator i = (*s)->publics.find(name);
        if(i == code->publics.end())
        {
            fprintf(stderr, "Attempt to find symbol %s which not exists.\n", name.c_str());
            return 0;
        }
        return i->second;
    }
    return 0;
}

bool O65::HasSym(SegmentSelection seg, const std::string& name) const
{
    if(const Segment*const *s = GetSegRef(seg))
    {
        const O65::Segment::publicmap_t& pubs = (*s)->publics;
        return pubs.find(name) != pubs.end();
    }
    return false;
}

const vector<std::string> O65::GetSymbolList(SegmentSelection seg) const
{
    vector<std::string> result;
    if(const Segment*const *s = GetSegRef(seg))
    {
        const O65::Segment::publicmap_t& pubs = (*s)->publics;
        result.reserve(pubs.size());
        for(O65::Segment::publicmap_t::const_iterator
            i = pubs.begin();
            i != pubs.end();
            ++i)
        {
            result.push_back(i->first);
        }
    }
    return result;
}

const vector<std::string> O65::GetExternList() const
{
    return defs->GetExternList();
}

void O65::Verify() const
{
    defs->DumpUndefines();
}

O65::Segment** O65::GetSegRef(SegmentSelection seg)
{
    Segment** result = NULL;
    switch(seg)
    {
        case CODE: result = &code; break;
        case DATA: result = &data; break;
        case ZERO: result = &zero; break;
        case BSS: result = &bss; break;
        default: return NULL;
    }
    if(!*result) *result = new Segment;
    return result;
}
const O65::Segment*const * O65::GetSegRef(SegmentSelection seg) const
{
    switch(seg)
    {
        case CODE: return code ? &code : NULL;
        case DATA: return data ? &data : NULL;
        case ZERO: return zero ? &zero : NULL;
        case BSS: return bss ? &bss : NULL;
    }
    return NULL;
}

void O65::Resize(SegmentSelection seg, unsigned newsize)
{
    if(Segment**s = GetSegRef(seg))
    {
        (*s)->space.resize(newsize);
    }
}

void O65::Write(SegmentSelection seg, unsigned addr, unsigned char value)
{
    if(Segment**s = GetSegRef(seg))
    {
        (*s)->space[addr] = value;
    }
}

void O65::LoadSegFrom(SegmentSelection seg, const vector<unsigned char>& buf)
{
    if(Segment**s = GetSegRef(seg))
    {
        (*s)->space = buf;
    }
}

bool O65::Error() const
{
    return error;
}

void O65::SetError()
{
    error = true;
}

void O65::DeclareByteRelocation(SegmentSelection seg, const std::string& name, unsigned addr)
{
    Segment**s = GetSegRef(seg); if(!s) return;

    unsigned symno = defs->GetSymno(name);

    if(symno == ~0U)
        symno = defs->AddUndefined(name);
    else if(defs->IsDefined(symno))
    {
        unsigned value = defs->GetValue(symno);
        addr -= (*s)->base;
        (*s)->space[addr] = value & 0xFF;
        return;
    }
    (*s)->R.R16lo.AddReloc(addr, symno);
}

void O65::DeclareWordRelocation(SegmentSelection seg, const std::string& name, unsigned addr)
{
    Segment**s = GetSegRef(seg); if(!s) return;

    unsigned symno = defs->GetSymno(name);

    if(symno == ~0U)
        symno = defs->AddUndefined(name);
    else if(defs->IsDefined(symno))
    {
        unsigned value = defs->GetValue(symno);
        addr -= (*s)->base;
        (*s)->space[addr    ] =  value       & 0xFF;
        (*s)->space[addr + 1] = (value >> 8) & 0xFF;
        return;
    }
    (*s)->R.R16.AddReloc(addr, symno);
}

/*
void O65::DeclareHiByteRelocation(SegmentSelection seg, const std::string& name, unsigned addr)
{
    Segment**s = GetSegRef(seg); if(!s) return;

    unsigned symno = defs->GetSymno(name);

    if(symno == ~0U)
        symno = defs->AddUndefined(name);
    else if(defs->IsDefined(symno))
    {
        unsigned value = defs->GetValue(symno);
        addr -= (*s)->base;
        (*s)->space[addr] = (value >> 8) & 0xFF;
        return;
    }
    (*s)->R.R16hi.AddReloc(addr, symno);
}
*/

void O65::DeclareLongRelocation(SegmentSelection seg, const std::string& name, unsigned addr)
{
    Segment**s = GetSegRef(seg); if(!s) return;

    unsigned symno = defs->GetSymno(name);

    if(symno == ~0U)
        symno = defs->AddUndefined(name);
    else if(defs->IsDefined(symno))
    {
        unsigned value = defs->GetValue(symno);
        addr -= (*s)->base;
        (*s)->space[addr    ] =  value       & 0xFF;
        (*s)->space[addr + 1] = (value >> 8) & 0xFF;
        (*s)->space[addr + 2] = (value >>16) & 0xFF;
        return;
    }
    (*s)->R.R24.AddReloc(addr, symno);
}

void O65::Segment::LoadRelocations(FILE* fp)
{
    int addr = -1;
    for(;;)
    {
        int c = fgetc(fp);
        if(!c || c == EOF)break;
        if(c == 255) { addr += 254; continue; }
        addr += c;
        c = fgetc(fp);
        unsigned type = c & 0xE0;
        unsigned area = c & 0x07;

        switch(area)
        {
            case 0: // external
            {
                unsigned symno = LoadWord(fp);
                switch(type)
                {
                    case 0x20:
                    {
                        R.R16lo.AddReloc(addr, symno);
                        break;
                    }
                    case 0x40:
                    {
                        RT::R16hi_t::Type tmp(addr, fgetc(fp));
                        R.R16hi.AddReloc(tmp, symno);
                        break;
                    }
                    case 0x80:
                    {
                        R.R16.AddReloc(addr, symno);
                        break;
                    }
                    case 0xA0:
                    {
                        RT::R24seg_t::Type tmp(addr, LoadWord(fp));
                        R.R24seg.AddReloc(tmp, symno);
                        break;
                    }
                    case 0xC0:
                    {
                        R.R24.AddReloc(addr, symno);
                        break;
                    }
                    default:
                    {
                        fprintf(stderr,
                            "Error: External reloc type %02X not supported yet\n",
                            type);
                    }
                }
                break;
            }
            case CODE: // code =2
            case DATA: // data =3
            case ZERO: // zero =5
            case BSS:  // bss  =4
            {
                SegmentSelection seg = (SegmentSelection)area;
                //fprintf(stderr, "Fixup into %u (%X) -- type=%02X\n", seg, addr, type);
                switch(type)
                {
                    case 0x20:
                    {
                        R.R16lo.AddFixup(seg, addr);
                        break;
                    }
                    case 0x40:
                    {
                        RT::R16hi_t::Type tmp(addr, fgetc(fp));
                        R.R16hi.AddFixup(seg, tmp);
                        break;
                    }
                    case 0x80:
                    {
                        R.R16.AddFixup(seg, addr);
                        break;
                    }
                    case 0xA0:
                    {
                        RT::R24seg_t::Type tmp(addr, LoadWord(fp));
                        R.R24seg.AddFixup(seg, tmp);
                        break;
                    }
                    case 0xC0:
                    {
                        R.R24.AddFixup(seg, addr);
                        break;
                    }
                    default:
                    {
                        fprintf(stderr,
                            "Error: Fixup type %02X not supported yet\n",
                            type);
                    }
                }
                break;
            }
            default:
            {
                fprintf(stderr,
                    "Error: Reloc area type %02X not supported yet\n",
                        area);
            }
        }
    }
}

const vector<pair<unsigned char, std::string> >& O65::GetCustomHeaders() const
{
    return customheaders;
}

/* Get relocation data of the given segment */
const Relocdata<unsigned> O65::GetRelocData(SegmentSelection seg) const
{
    const Segment*const *s = GetSegRef(seg);
    if(!s) return Relocdata<unsigned> ();
    return (*s)->R;
}

const std::string GetSegmentName(const SegmentSelection seg)
{
    switch(seg)
    {
        case CODE: return "code";
        case DATA: return "data";
        case ZERO: return "zero";
        case BSS: return "bss";
    }
    return "?";
}
