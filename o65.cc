/* xa65 object file loader and linker for C++
 * For loading and linking 65816 object files
 * Copyright (C) 1992,2005 Bisqwit (http://iki.fi/bisqwit/)
 *
 * Version 1.9.0 - Aug 18 2003, Sep 4 2003, Jan 23 2004,
 *                 Jan 31 2004
 */

#define DEBUG_FIXUPS 0

#include <map>
#include <set>

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
    unsigned LoadSWord(FILE *fp, bool use32)
    {
        return use32 ? LoadDWord(fp) : LoadWord(fp);
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
    
    /* FIXME: No validity checks here */
    // Skip header
    LoadWord(fp);LoadWord(fp);LoadWord(fp);
    
    /* FIXME: No validity checks here */
    // Skip mode
    unsigned mode = LoadWord(fp);
    
    bool use32 = mode & 0x2000;
    
    if(this->code) delete this->code;
    if(this->data) delete this->data;
    if(this->zero) delete this->zero;
    if(this->bss) delete this->bss;

    this->code = new Segment;
    this->data = new Segment;
    this->zero = new Segment;
    this->bss = new Segment;
    
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
        
        std::string data(len, '\0');
        
        for(unsigned a=0; a<len; ++a) data[a] = fgetc(fp);
        
        //fprintf(stderr, "Custom header %u: '%.*s'\n", type, len, data.data());
        
        customheaders.push_back(make_pair(type, data));
    }
    
    fread(&this->code->space[0], this->code->space.size(), 1, fp);
    fread(&this->data->space[0], this->data->space.size(), 1, fp);
    // zero and bss Segments don't exist in o65 format.
    
    // load external symbols
    
    unsigned num_und = LoadSWord(fp, use32);

    //fprintf(stderr, "@%X: %u externs..\n", ftell(fp), num_und);

    for(unsigned a=0; a<num_und; ++a)
    {
        std::string varname;
        while(int c = fgetc(fp)) { if(c==EOF)break; varname += (char) c; }
        
        defs->AddUndefined(varname);
    }
    
    //fprintf(stderr, "@%X: code relocs..\n", ftell(fp));
    
    code->LoadRelocations(fp);

    //fprintf(stderr, "@%X: data relocs..\n", ftell(fp));

    data->LoadRelocations(fp);
    // relocations don't exist for zero/bss in o65 format.
    
    unsigned num_global = LoadSWord(fp, use32);
    
    //fprintf(stderr, "@%X: %u globals\n", ftell(fp), num_global);
    
    for(unsigned a=0; a<num_global; ++a)
    {
        std::string varname;
        while(int c = fgetc(fp)) { if(c==EOF)break; varname += (char) c; }
        
        SegmentSelection seg = (SegmentSelection)fgetc(fp);
        
        unsigned value = LoadSWord(fp, use32);
        
        DeclareGlobal(seg, varname, value);
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
