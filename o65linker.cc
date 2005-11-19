#include "o65linker.hh"
#include "msginsert.hh"

#include <list>
#include <utility>
#include <map>

using std::make_pair;
using std::list;
using std::map;

#include "hash.hh"

class O65linker::Object
{
public:
    O65 object;
private:
    string name;
public:
    vector<string> extlist;
    
    LinkageWish linkage;
    
public:
    Object(const O65& obj, const string& what, LinkageWish link)
    : object(obj),
      name(what),
      extlist(obj.GetExternList()),
      linkage(link)
    {
    }
    
    Object()
    : object(),
      name(),
      extlist(),
      linkage()
    {
    }
    
    const string& GetName() const { return name; }
    
    void Release()
    {
        //*this = Object();
    }
    
    bool operator< (const Object& b) const
    {
        return linkage.GetAddress() < b.linkage.GetAddress();
    }
};

class O65linker::SymCache
{
    typedef hash_map<string, unsigned> cachetype;
    
    cachetype sym_cache;
public:
    void Update(const Object& o, unsigned objnum)
    {
        const vector<string> symlist = o.object.GetSymbolList();
        for(unsigned a=0; a<symlist.size(); ++a)
            sym_cache[symlist[a]] = objnum;
    }
    
    const pair<unsigned, bool> Find(const string& sym) const
    {
        cachetype::const_iterator i = sym_cache.find(sym);
        if(i == sym_cache.end()) return make_pair(0, false);
        return make_pair(i->second, true);
    }
};

void O65linker::AddObject(const O65& object, const string& what, LinkageWish linkage)
{
    if(linked && linkage.type != LinkageWish::LinkHere)
    {
        fprintf(stderr,
            "O65 linker: Attempt to add object \"%s\""
            " after linking already done\n", what.c_str());
        return;
    }
    Object *newobj = new Object(object, what, linkage);
    
    const vector<string> symlist = newobj->object.GetSymbolList();
    
    unsigned collisions = 0;
    for(unsigned a=0; a<symlist.size(); ++a)
    {
        const pair<unsigned, bool> tmp = symcache->Find(symlist[a]);
        if(tmp.second)
        {
            fprintf(stderr,
                "O65 linker: ERROR:"
                " Symbol \"%s\" defined by object \"%s\""
                " is already present in object \"%s\"\n",
                symlist[a].c_str(),
                what.c_str(),
                objects[tmp.first]->GetName().c_str());
            ++collisions;
        }
    }
    if(collisions)
    {
        delete newobj;
        return;
    }
    objects.push_back(newobj);

    symcache->Update(*newobj, objects.size()-1);
}

void O65linker::AddObject(const O65& object, const string& what, unsigned address)
{
    LinkageWish wish;
    wish.SetAddress(address);
    AddObject(object, what, wish);
}

const vector<unsigned> O65linker::GetSizeList() const
{
    vector<unsigned> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->object.GetSegSize(CODE));
    return result;
}

const vector<unsigned> O65linker::GetAddrList() const
{
    vector<unsigned> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->linkage.GetAddress());
    return result;
}

const vector<LinkageWish> O65linker::GetLinkageList() const
{
    vector<LinkageWish> result;
    unsigned n = objects.size();
    result.reserve(n);
    for(unsigned a=0; a<n; ++a)
        result.push_back(objects[a]->linkage);
    return result;
}

void O65linker::PutAddrList(const vector<unsigned>& addrs)
{
    unsigned limit = addrs.size();
    if(objects.size() < limit) limit = objects.size();
    for(unsigned a=0; a<limit; ++a)
    {
        objects[a]->linkage.SetAddress(addrs[a]);
        objects[a]->object.Locate(CODE, addrs[a]);
    }
}

const vector<unsigned char>& O65linker::GetCode(unsigned objno) const
{
    return objects[objno]->object.GetSeg(CODE);
}

const string& O65linker::GetName(unsigned objno) const
{
    return objects[objno]->GetName();
}

void O65linker::Release(unsigned objno)
{
    objects[objno]->Release();
}

void O65linker::DefineSymbol(const string& name, unsigned value)
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
    defines.push_back(std::make_pair(name, std::make_pair(value, false)));
}

void O65linker::AddReference(const string& name, const ReferMethod& reference)
{
    const pair<unsigned, bool> tmp = symcache->Find(name);
    if(tmp.second)
    {
        const Object& o = *objects[tmp.first];
        if(o.linkage.type == LinkageWish::LinkHere)
        {
            unsigned value = o.object.GetSymAddress(name);
            // resolved referer
            FinishReference(reference, value, name);
            return;
        }
    }

    if(linked)
    {
        fprintf(stderr, "O65 linker: Attempt to add references after linking\n");
    }
    referers.push_back(make_pair(reference, name));
}

void O65linker::LinkSymbol(const string& name, unsigned value)
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

void O65linker::FinishReference(const ReferMethod& reference, unsigned target, const string& what)
{
    unsigned pos = reference.GetAddr();
    unsigned value = reference.Evaluate(target);
    
    char Buf[513];
    sprintf(Buf, "%016X", value);

    std::string title = "ref " + what + ": $" + (Buf + 16-reference.GetSize()*2);

    vector<unsigned char> bytes;
    for(unsigned n=0; n<reference.GetSize(); ++n)
    {
        bytes.push_back(value & 255);
        value >>= 8;
    }
    
    AddLump(bytes, pos, title);
}

void O65linker::AddLump(const vector<unsigned char>& source,
                        unsigned address,
                        const string& what,
                        const string& name)
{
    O65 tmp;
    tmp.LoadSegFrom(CODE, source);
    tmp.Locate(CODE, address);
    if(!name.empty()) tmp.DeclareGlobal(CODE, name, address);
    
    LinkageWish wish;
    wish.SetAddress(address);
    
    AddObject(tmp, what, wish);
}

void O65linker::AddLump(const vector<unsigned char>& source,
                        const string& what,
                        const string& name)
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
        if(o.linkage.type != LinkageWish::LinkHere)
        {
            MessageModuleWithoutAddress(o.GetName());
            continue;
        }
        
        MessageLoadingItem(o.GetName());
        
        for(unsigned b=0; b<o.extlist.size(); ++b)
        {
            const string& ext = o.extlist[b];
            
            unsigned found=0, addr=0, defcount=0;
            
            const pair<unsigned, bool> tmp = symcache->Find(ext);
            if(tmp.second)
            {
                addr = objects[tmp.first]->object.GetSymAddress(ext);
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
        const string& name = referers[c].second;
        const pair<unsigned, bool> tmp = symcache->Find(name);
        if(tmp.second)
        {
            const Object& o = *objects[tmp.first];
            if(o.linkage.type != LinkageWish::LinkHere) continue;
            
            unsigned value = o.object.GetSymAddress(name);
             
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
        string   name;
     public:
        IPS_global(): IPS_item(), name() { }
    };
    struct IPS_extern: public IPS_item
    {
        string   name;
        unsigned size;
     public:
        IPS_extern(): IPS_item(), name(), size() { }
    };
    struct IPS_lump: public IPS_item
    {
        vector<unsigned char> data;
     public:
        IPS_lump(): IPS_item(), data() { }
    };
}

void O65linker::LoadIPSfile(FILE* fp, const string& what)
{
    rewind(fp);
    
    /* FIXME: No validity checks here */
    
    for(int a=0; a<5; ++a) fgetc(fp); // Skip header which should be "PATCH"
    
    list<IPS_global> globals;
    list<IPS_extern> externs;
    list<IPS_lump> lumps;
    
    for(;;)
    {
        unsigned addr = LoadIPSlong(fp);
        if(feof(fp) || addr == IPS_EOF_MARKER) break;
        
        unsigned length = LoadIPSword(fp);
        
        vector<unsigned char> Buf2(length);
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

                IPS_extern tmp;
                tmp.name = name;
                tmp.addr = addr;
                tmp.size = size;
                
                externs.push_back(tmp);
                
                break;
            }
            default:
            {
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
    
    for(list<IPS_lump>::const_iterator next_lump,
        i = lumps.begin(); i != lumps.end(); i=next_lump)
    {
        next_lump = i; ++next_lump;
        const IPS_lump& lump = *i;
        
        O65 tmp;
        
        tmp.LoadSegFrom(CODE, lump.data);
        tmp.Locate(CODE, lump.addr);
        
        bool last = next_lump == lumps.end();

        for(list<IPS_global>::iterator next_global,
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

        for(list<IPS_extern>::iterator next_extern,
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
        sprintf(Buf, "block $%06X of ", lump.addr); /* | 0xC00000 removed here */
        
        LinkageWish wish;
        wish.SetAddress(lump.addr);
        
        AddObject(tmp, Buf + what, wish);
    }
}

void O65linker::SortByAddress()
{
    std::sort(objects.begin(), objects.end());
}
