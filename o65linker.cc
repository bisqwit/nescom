#include "o65linker.hh"
#include "msginsert.hh"

#include <list>
#include <utility>
#include <map>

#include "hash.hh"

class O65linker::Object
{
public:
    O65 object;
private:
    std::string name;

public:
    std::vector<std::string> extlist;

private:
    LinkageWish linkageCODE;
    LinkageWish linkageDATA;
    LinkageWish linkageZERO;
    LinkageWish linkageBSS;

public:
    Object(const O65& obj, const std::string& what,
        LinkageWish linkCODE,
        LinkageWish linkDATA,
        LinkageWish linkZERO,
        LinkageWish linkBSS
      )
    : object(obj),
      name(what),
      extlist(obj.GetExternList()),
      linkageCODE(linkCODE),
      linkageDATA(linkDATA),
      linkageZERO(linkZERO),
      linkageBSS(linkBSS)
    {
    }

    Object()
    : object(),
      name(),
      extlist(),
      linkageCODE(),
      linkageDATA(),
      linkageZERO(),
      linkageBSS()
    {
    }

    const std::string& GetName() const { return name; }

    void Release()
    {
        //*this = Object();
    }

    bool operator< (const Object& b) const
    {
        if(linkageCODE != b.linkageCODE) return linkageCODE < b.linkageCODE;
        if(linkageDATA != b.linkageDATA) return linkageDATA < b.linkageDATA;
        if(linkageZERO != b.linkageZERO) return linkageZERO < b.linkageZERO;
        if(linkageBSS  != b.linkageBSS ) return linkageBSS  < b.linkageBSS ;
        return name < b.name;
    }

    const LinkageWish& GetLinkage(const SegmentSelection seg) const
    {
        switch(seg)
        {
            case CODE: return linkageCODE;
            case DATA: return linkageDATA;
            case ZERO: return linkageZERO;
            case BSS : return linkageBSS ;
        }
        return linkageCODE;
    }
    LinkageWish& GetLinkage(const SegmentSelection seg)
    {
        return
            const_cast<LinkageWish&> (
            (const_cast<const Object&>(*this)).GetLinkage(seg)
                                     );
    }
};

struct ResolvedSymbol
{
    unsigned objnum;
    SegmentSelection seg;
};
struct ClashItem
{
    std::string symbol;
    SegmentSelection seg;
    ResolvedSymbol found;
};
typedef std::list<ClashItem> clashlist_t;

class O65linker::SymCache
{
    typedef hash_map<std::string, ResolvedSymbol> cachetype;

    cachetype sym_cache;
public:
    void Update(const Object& o, unsigned objnum,
                clashlist_t& clashlist)
    {
        Update(o, objnum, CODE, clashlist);
        Update(o, objnum, DATA, clashlist);
        Update(o, objnum, ZERO, clashlist);
        Update(o, objnum, BSS,  clashlist);
    }
    void Update(const Object& o, unsigned objnum, SegmentSelection seg,
                clashlist_t& clashlist)
    {
        ResolvedSymbol res;
        res.objnum = objnum;
        res.seg    = seg;

        const std::vector<std::string> symlist = o.object.GetSymbolList(seg);
        for(unsigned a=0; a<symlist.size(); ++a)
        {
            cachetype::const_iterator i = sym_cache.find(symlist[a]);
            if(i != sym_cache.end())
            {
                ClashItem clash;
                clash.symbol = symlist[a];
                clash.seg    = seg;
                clash.found  = i->second;
                clashlist.push_back(clash);
                continue;
            }
            sym_cache[symlist[a]] = res;
        }
    }

    const std::pair<ResolvedSymbol, bool> Find(const std::string& sym) const
    {
        cachetype::const_iterator i = sym_cache.find(sym);
        if(i == sym_cache.end())
        {
            return std::make_pair(ResolvedSymbol(), false);
        }
        return std::make_pair(i->second, true);
    }
};

void O65linker::AddObject(const O65& object, const std::string& what, const std::map<SegmentSelection, LinkageWish>& linkages)
{
    LinkageWish linkageCODE;
    LinkageWish linkageDATA;
    LinkageWish linkageZERO;
    LinkageWish linkageBSS;
    for(const auto& l: linkages)
        switch(l.first)
        {
            case CODE: linkageCODE = std::move(l.second); break;
            case DATA: linkageDATA = std::move(l.second); break;
            case ZERO: linkageZERO = std::move(l.second); break;
            case BSS:  linkageBSS  = std::move(l.second); break;
        }

    if(linked && linkageCODE.type != LinkageWish::LinkHere)
    {
        fprintf(stderr,
            "O65 linker: Attempt to add object \"%s\""
            " after linking already done\n", what.c_str());
        return;
    }

    SymCache backup = *symcache;
    Object *newobj = new Object(object, what,
        linkageCODE, linkageDATA, linkageZERO, linkageBSS);

    clashlist_t clashes;
    symcache->Update(*newobj, objects.size(), clashes);
    if(!clashes.empty())
    {
        for(clashlist_t::const_iterator i = clashes.begin(); i != clashes.end(); ++i)
        {
            const ClashItem& clash = *i;

            fprintf(stderr,
                "O65 linker: ERROR:"
                " Symbol \"%s\", defined by object \"%s\" in %s,"
                " is already present in object \"%s\"'s %s\n",
                clash.symbol.c_str(),
                what.c_str(), GetSegmentName(clash.seg).c_str(),

                objects[clash.found.objnum]->GetName().c_str(),
                GetSegmentName(clash.found.seg).c_str()
            );
        }
        *symcache = backup;
        delete newobj;
        return;
    }
    objects.push_back(newobj);
}

/*
void O65linker::AddObject(const O65& object, const std::string& what, unsigned address)
{
    LinkageWish wish;
    wish.SetAddress(address);
    AddObject(object, what, wish);
}
*/

const std::vector<unsigned> O65linker::GetSizeList(const SegmentSelection seg) const
{
    std::vector<unsigned> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->object.GetSegSize(seg));
    return result;
}

const std::vector<unsigned> O65linker::GetAddrList(const SegmentSelection seg) const
{
    std::vector<unsigned> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->GetLinkage(seg).GetAddress());
    return result;
}

const std::vector<LinkageWish> O65linker::GetLinkageList(const SegmentSelection seg) const
{
    std::vector<LinkageWish> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->GetLinkage(seg));
    return result;
}

void O65linker::PutAddrList(const std::vector<unsigned>& addrs, const SegmentSelection seg)
{
    unsigned limit = addrs.size();
    if(objects.size() < limit) limit = objects.size();
    for(unsigned a=0; a<limit; ++a)
    {
        unsigned addr = addrs[a];
        /*
        if(addr >= 0xC08000 && addr <= 0xC0FFFF)
            addr -= 0x400000; // Put them in 0x808000
        */
        objects[a]->GetLinkage(seg).SetAddress(addr);
        objects[a]->object.Locate(seg, addrs[a]);
    }
}

const std::vector<unsigned char>& O65linker::GetSeg(const SegmentSelection seg, unsigned objno) const
{
    return objects[objno]->object.GetSeg(seg);
}

const std::string& O65linker::GetName(unsigned objno) const
{
    return objects[objno]->GetName();
}

void O65linker::Release(unsigned objno)
{
    objects[objno]->Release();
}

void O65linker::DefineSymbol(const std::string& name, unsigned value)
{
    if(linked)
    {
        fprintf(stderr, "O65 linker: Attempt to add symbols after linking\n");
    }
    for(unsigned c=0; c<defines.size(); ++c)
    {
        if(defines[c].first == name)
        {
            if(defines[c].second.first != value)
            {
                fprintf(stderr,
                    "O65 linker: Error: %s previously defined as %X,"
                    " can not redefine as %X\n",
                        name.c_str(), defines[c].second.first, value);
            }
            return;
        }
    }

    defines.emplace_back(name, std::make_pair(value, false));
}

void O65linker::AddReference(const std::string& name, const ReferMethod& reference)
{
    const std::pair<ResolvedSymbol, bool> tmp = symcache->Find(name);
    if(tmp.second)
    {
        const Object& o = *objects[tmp.first.objnum];

        if(o.GetLinkage(tmp.first.seg).type == LinkageWish::LinkHere)
        {
            unsigned value = o.object.GetSymAddress(tmp.first.seg, name);
            // resolved referer
            FinishReference(reference, value, name);
            return;
        }
    }

    if(linked)
    {
        fprintf(stderr, "O65 linker: Attempt to add references after linking\n");
    }
    referers.emplace_back(reference, name);
}

void O65linker::LinkSymbol(const std::string& name, unsigned value)
{
    for(unsigned a=0; a<objects.size(); ++a)
    {
        Object& o = *objects[a];
        for(unsigned b=0; b<o.extlist.size(); ++b)
        {
            /* If this module is referring to this symbol */
            if(o.extlist[b] == name)
            {
                o.extlist.erase(o.extlist.begin() + b);
                o.object.LinkSym(name, value);
                --b;
            }
        }
    }
    for(unsigned a=0; a<referers.size(); ++a)
    {
        if(name == referers[a].second)
        {
            // resolved referer
            FinishReference(referers[a].first, value, name);
            referers.erase(referers.begin() + a);
            --a;
        }
    }
}

void O65linker::FinishReference(const ReferMethod& reference, unsigned target, const std::string& what)
{
    unsigned pos = reference.GetAddr();
    unsigned value = reference.Evaluate(target);

    char Buf[513];
    sprintf(Buf, "%016X", value);

    std::string title = "ref " + what + ": $" + (Buf + 16-reference.GetSize()*2);

    std::vector<unsigned char> bytes;
    for(unsigned n=0; n<reference.GetSize(); ++n)
    {
        bytes.push_back(value & 255);
        value >>= 8;
    }

    AddLump(bytes, pos, title);
}

void O65linker::AddLump(const std::vector<unsigned char>& source,
                        unsigned address,
                        const std::string& what,
                        const std::string& name)
{
    O65 tmp;
    tmp.LoadSegFrom(CODE, source);
    tmp.Locate(CODE, address);
    if(!name.empty()) tmp.DeclareGlobal(CODE, name, address);

    LinkageWish wish;
    wish.SetAddress(address);
    AddObject(tmp, what, {{CODE,wish}} );
}

void O65linker::AddLump(const std::vector<unsigned char>& source,
                        const std::string& what,
                        const std::string& name)
{
    O65 tmp;
    tmp.LoadSegFrom(CODE, source);
    if(!name.empty()) tmp.DeclareGlobal(CODE, name, 0);
    AddObject(tmp, what);
}

void O65linker::Link()
{
    if(linked)
    {
        fprintf(stderr, "O65 linker: Attempt to link twice\n");
        return;
    }
    linked = true;

    MessageLinkingModules(objects.size());

    // For each module, satisfy each of their externs one by one.
    for(unsigned a=0; a<objects.size(); ++a)
    {
        Object& o = *objects[a];

        bool LinkageIncomplete = false;

        if(o.GetLinkage(CODE).type != LinkageWish::LinkHere)
        {
            MessageModuleWithoutAddress(o.GetName(), CODE);
            LinkageIncomplete = true;
        }

        if(o.GetLinkage(DATA).type != LinkageWish::LinkHere)
        {
            MessageModuleWithoutAddress(o.GetName(), DATA);
            LinkageIncomplete = true;
        }

        if(o.GetLinkage(ZERO).type != LinkageWish::LinkHere)
        {
            MessageModuleWithoutAddress(o.GetName(), ZERO);
            LinkageIncomplete = true;
        }

        if(o.GetLinkage(BSS).type != LinkageWish::LinkHere)
        {
            MessageModuleWithoutAddress(o.GetName(), BSS);
            LinkageIncomplete = true;
        }

        if(LinkageIncomplete) continue;




        MessageLoadingItem(o.GetName());

        for(unsigned b=0; b<o.extlist.size(); ++b)
        {
            const std::string& ext = o.extlist[b];

            unsigned found=0, addr=0, defcount=0;

            const std::pair<ResolvedSymbol, bool> tmp = symcache->Find(ext);
            if(tmp.second)
            {
                addr = objects[tmp.first.objnum]->object.GetSymAddress(tmp.first.seg, ext);
                ++found;
            }

            // Or if it was an external definition.
            for(unsigned c=0; c<defines.size(); ++c)
            {
                if(defines[c].first == ext)
                {
                    addr = defines[c].second.first;
                    defines[c].second.second = true;
                    ++defcount;
                }
            }

            if(found == 0 && !defcount)
            {
                MessageUndefinedSymbol(ext);
                // FIXME: where?
            }
            else if((found+defcount) != 1)
            {
                MessageDuplicateDefinition(ext, found, defcount);
            }

/*
            if(found > 0)
                fprintf(stderr, "Extern %u(%s) was resolved with linking\n", b, ext.c_str());
            if(defcount > 0)
                fprintf(stderr, "Extern %u(%s) was resolved with a define\n", b, ext.c_str());
*/

            if(found > 0 || defcount > 0)
            {
                o.object.LinkSym(ext, addr);

                o.extlist.erase(o.extlist.begin() + b);
                --b;
            }
        }
        if(!o.extlist.empty())
        {
            MessageUndefinedSymbols(o.extlist.size());
            // FIXME: where?
        }
    }

    for(unsigned c=0; c<referers.size(); ++c)
    {
        const std::string& name = referers[c].second;
        const std::pair<ResolvedSymbol, bool> tmp = symcache->Find(name);
        if(tmp.second)
        {
            const Object& o = *objects[tmp.first.objnum];
            if(o.GetLinkage(tmp.first.seg).type != LinkageWish::LinkHere) continue;

            unsigned value = o.object.GetSymAddress(tmp.first.seg, name);

            // resolved referer
            FinishReference(referers[c].first, value, name);
            referers.erase(referers.begin() + c);
            --c;
        }
    }

    MessageDone();

    for(unsigned a=0; a<objects.size(); ++a)
        objects[a]->object.Verify();

    if(!referers.empty())
    {
        //fprintf(stderr,
        //    "O65 linker: Leftover references found.\n");
        for(unsigned a=0; a<referers.size(); ++a)
            fprintf(stderr,
                "O65 linker: Unresolved reference: %s\n",
                    referers[a].second.c_str());
    }

    for(unsigned c=0; c<defines.size(); ++c)
        if(!defines[c].second.second)
        {
            fprintf(stderr,
                "O65 linker: Warning: Symbol \"%s\" was defined but never used.\n",
                defines[c].first.c_str());
        }
}

O65linker::O65linker()
   : symcache(new SymCache),
     objects(),
     defines(),
     referers(),
     num_groups_used(0),
     linked(false)
{
}

O65linker::~O65linker()
{
    delete symcache;
}

namespace
{
    unsigned LoadIPSword(FILE *fp)
    {
        unsigned char Buf[2];
        fread(Buf, 2, 1, fp);
        return (Buf[0] << 8) | Buf[1];
    }
    unsigned LoadIPSlong(FILE *fp)
    {
        unsigned char Buf[3];
        fread(Buf, 3, 1, fp);
        return (Buf[0] << 16) | (Buf[1] << 8) | Buf[2];
    }

    struct IPS_item
    {
        unsigned addr;

        bool operator< (const IPS_item& b) const { return addr < b.addr; }
     public:
        IPS_item(): addr(0) { }
    };

    struct IPS_global: public IPS_item
    {
        std::string   name;
     public:
        IPS_global(): IPS_item(), name() { }
    };
    struct IPS_extern: public IPS_item
    {
        std::string   name;
        unsigned size;
     public:
        IPS_extern(): IPS_item(), name(), size() { }
    };
    struct IPS_lump: public IPS_item
    {
        std::vector<unsigned char> data;
     public:
        IPS_lump(): IPS_item(), data() { }
    };
}

void O65linker::LoadIPSfile(FILE* fp, const std::string& what,
                            unsigned long (*AddressTransformer)(unsigned long))
{
    rewind(fp);

    /* FIXME: No validity checks here */

    for(int a=0; a<5; ++a) fgetc(fp); // Skip header which should be "PATCH"

    std::list<IPS_global> globals;
    std::list<IPS_extern> externs;
    std::list<IPS_lump> lumps;

    for(;;)
    {
        unsigned addr = LoadIPSlong(fp);
        if(feof(fp) || addr == IPS_EOF_MARKER) break;

        unsigned length = LoadIPSword(fp);

        std::vector<unsigned char> Buf2(length);
        int c = fread(&Buf2[0], 1, length, fp);
        if(c < 0 || c != (int)length) break;

        switch(addr)
        {
            case IPS_ADDRESS_GLOBAL:
            {
                std::string name((const char *)&Buf2[0], Buf2.size());
                name = name.c_str();
                unsigned addr = Buf2[name.size()+1]
                             | (Buf2[name.size()+2] << 8)
                             | (Buf2[name.size()+3] << 16);

                if(AddressTransformer) addr = AddressTransformer(addr);

                IPS_global tmp;
                tmp.name = name;
                tmp.addr = addr;

                globals.push_back(tmp);

                break;
            }
            case IPS_ADDRESS_EXTERN:
            {
                std::string name((const char *)&Buf2[0], Buf2.size());
                name = name.c_str();
                unsigned addr = Buf2[name.size()+1]
                             | (Buf2[name.size()+2] << 8)
                             | (Buf2[name.size()+3] << 16);
                unsigned size = Buf2[name.size()+4];

                if(AddressTransformer) addr = AddressTransformer(addr);

                IPS_extern tmp;
                tmp.name = name;
                tmp.addr = addr;
                tmp.size = size;

                externs.push_back(tmp);

                break;
            }
            default:
            {
                if(AddressTransformer) addr = AddressTransformer(addr);

                IPS_lump tmp;
                tmp.data = Buf2;
                tmp.addr = addr;

                lumps.push_back(tmp);

                break;
            }
        }
    }

    globals.sort();
    externs.sort();
    lumps.sort();

    for(std::list<IPS_lump>::const_iterator next_lump,
        i = lumps.begin(); i != lumps.end(); i=next_lump)
    {
        next_lump = i; ++next_lump;
        const IPS_lump& lump = *i;

        O65 tmp;

        tmp.LoadSegFrom(CODE, lump.data);
        tmp.Locate(CODE, lump.addr);

        bool last = next_lump == lumps.end();

        for(std::list<IPS_global>::iterator next_global,
            j = globals.begin(); j != globals.end(); j = next_global)
        {
            next_global = j; ++next_global;

            if(last
            || (j->addr >= lump.addr && j->addr < lump.addr + lump.data.size())
              )
            {
                tmp.DeclareGlobal(CODE, j->name, j->addr);
                globals.erase(j);
            }
        }

        for(std::list<IPS_extern>::iterator next_extern,
            j = externs.begin(); j != externs.end(); j = next_extern)
        {
            next_extern = j; ++next_extern;

            if(last
            || (j->addr >= lump.addr && j->addr < lump.addr + lump.data.size())
              )
            {
                switch(j->size)
                {
                    case 1:
                        tmp.DeclareByteRelocation(CODE, j->name, j->addr);
                        break;
                    case 2:
                        tmp.DeclareWordRelocation(CODE, j->name, j->addr);
                        break;
                    case 3:
                        tmp.DeclareLongRelocation(CODE, j->name, j->addr);
                        break;
                }
                externs.erase(j);
            }
        }

        char Buf[64];
        sprintf(Buf, "block $%06X of ", lump.addr);

        LinkageWish wish;
        wish.SetAddress(lump.addr);

        //fprintf(stderr, "%s\n", Buf);

        AddObject(tmp, Buf + what, {{CODE,wish}});
    }
}

void O65linker::SortByAddress()
{
    std::sort(objects.begin(), objects.end());
}
