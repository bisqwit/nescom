#ifndef bqtO65linkerHH
#define bqtO65linkerHH

#include <vector>
#include <utility>
#include <cstdio>
#include <string>

using std::pair;
using std::FILE;

#include "o65.hh"
#include "refer.hh"

#define IPS_ADDRESS_EXTERN 0x01
#define IPS_ADDRESS_GLOBAL 0x02
#define IPS_EOF_MARKER     0x454F46

struct LinkageWish
{
    enum type
    {
        LinkAnywhere,
        LinkHere,
        LinkInGroup,
        LinkThisPage
    } type;
    unsigned param;
public:
    LinkageWish(): type(LinkAnywhere), param(0) {}
    
    unsigned GetAddress() const
    {
        if(type != LinkHere) return 0;
        return param; /* | 0xC00000 removed here */
    }
    unsigned GetPage() const { return param; }
    unsigned GetGroup() const { return param; }
    
    void SetAddress(unsigned addr) { param=addr; type=LinkHere; }
    void SetLinkageGroup(unsigned num) { param=num; type=LinkInGroup; }
    void SetLinkagePage(unsigned page) { param=page; type=LinkThisPage; }
    
    bool operator< (const LinkageWish& b) const { return type==b.type ? param<b.param : type<b.type; }
    bool operator==(const LinkageWish& b) const { return type==b.type && param==b.param; }
    inline bool operator!=(const LinkageWish& b) const { return !operator==(b); }
};

class O65linker
{
public:
    O65linker();
    ~O65linker();
    
    void LoadIPSfile(FILE* fp, const std::string& what,
                     unsigned long (*AddressTransformer)(unsigned long) = 0);
    
    void AddObject(const O65& object, const std::string& what,
        LinkageWish linkageCODE = LinkageWish());
    
    void AddObject(const O65& object, const std::string& what, unsigned address);
    void AddLump(const std::vector<unsigned char>&,
                 unsigned address,
                 const std::string& what, const std::string& name="");
    void AddLump(const std::vector<unsigned char>&,
                 const std::string& what, const std::string& name="");
    
    unsigned CreateLinkageGroup() { return ++num_groups_used; }
    
    const std::vector<unsigned> GetSizeList(const SegmentSelection seg=CODE) const;
    const std::vector<unsigned> GetAddrList(const SegmentSelection seg=CODE) const;
    const std::vector<LinkageWish> GetLinkageList(const SegmentSelection seg=CODE) const;
    void PutAddrList(const std::vector<unsigned>& addrs, const SegmentSelection seg=CODE);
    
    const std::vector<unsigned char>& GetSeg(const SegmentSelection seg, unsigned objno) const;

    inline const std::vector<unsigned char>& GetCode(unsigned objno) const
        { return GetSeg(CODE, objno); }
    
    const std::string& GetName(unsigned objno) const;
    
    void DefineSymbol(const std::string& name, unsigned value);
    
    void AddReference(const std::string& name, const ReferMethod& reference);

    void Link();
    
    void SortByAddress();
    
    // Release the memory allocated by given obj
    void Release(unsigned objno); // no range checks

private:
    // Copying prohibited
    O65linker(const O65linker& );
    const O65linker& operator= (const O65linker& );

private:
    void LinkSymbol(const std::string& name, unsigned value);
    
    void FinishReference(const ReferMethod& reference, unsigned target,
                         const std::string& what);

    class SymCache;
    class Object;
    
    SymCache *symcache;
    
    std::vector<Object* > objects;
    std::vector<pair<std::string, pair<unsigned, bool> > > defines;
    std::vector<pair<ReferMethod, std::string> > referers;
    unsigned num_groups_used;
    bool linked;
};

#endif
