#ifndef bqtctSpaceHH
#define bqtctSpaceHH

#include <map>
#include <set>
#include <vector>

#include "rangeset.hh"
#include "o65.hh" /* For SegmentSelection */

#define NOWHERE 0x10000

struct freespacerec
{
    unsigned pos;
    unsigned len;

    freespacerec() : pos(NOWHERE), len(0) {}
    freespacerec(unsigned l) : pos(NOWHERE), len(l) {}
    freespacerec(unsigned p,unsigned l) : pos(p), len(l) {}

    bool operator< (const freespacerec &b) const
    {
        if(pos != b.pos) return pos < b.pos;
        return len < b.len;
    }
};

typedef rangeset<unsigned> freespaceset;

/* (rom)page -> list */
class freespacemap
{
    bool quiet;
    std::map<unsigned/*bank*/, freespaceset> data;
    struct alias
    {
        unsigned length, realpage,realbegin;
    };
    std::map<unsigned/*bank*/, std::map<unsigned/*begin*/, alias>> aliases;
public:
    /*

    Note: 16-bit here denotes addresses that do not need a page number.
          The actual bitness is deduced from the GetPageSize() routine
          in romaddr.hh.
          24-bit denotes an address that has the page number built in.
    */

    /*
      In the NES version, freespacemap uses ROM-addresses!
    */

    freespacemap();

    void Report() const;
    void DumpPageMap(unsigned pagenum) const;

    void VerboseDump() const;

    // Uses segment-relative addresses (16-bit)
    void Add(unsigned page, unsigned begin, unsigned length);
    void Del(unsigned page, unsigned begin, unsigned length);

    // Uses absolute addresses (24-bit) (no segment wrapping!!)
    void Add(unsigned longaddr, unsigned length);
    void Del(unsigned longaddr, unsigned length);

    // Adds mirroring
    void AddAlias(unsigned aliaspage, unsigned aliasbegin, unsigned aliaslength,
                  unsigned realpage, unsigned realbegin);

    void OrganizeO65linker(class O65linker& objects, const SegmentSelection seg = CODE);

    const std::set<unsigned> GetPageList() const;
    const freespaceset& GetList(unsigned pagenum) const;

    // Returns segment-relative address (16-bit)
    unsigned Find(unsigned page, unsigned length);
    // Returns abbsolute address (24-bit)
    unsigned FindFromAnyPage(unsigned length);

    /* These don't need to be private, but they are
     * now to ensure Chronotools doesn't use them.
     */
private:

    unsigned Size() const;
    unsigned Size(unsigned page) const;
    unsigned GetFragmentation(unsigned page) const;

    // Uses segment-relative addresses (16-bit)
    bool Organize(std::vector<freespacerec> &blocks, unsigned pagenum);
    // Return value: errors-flag

    // Uses absolute addresses (24-bit)
    bool OrganizeToAnyPage(std::vector<freespacerec> &blocks);
    // Return value: errors-flag

    // Uses segment-relative addresses (16-bit), sets page
    bool OrganizeToAnySamePage(std::vector<freespacerec> &blocks, unsigned &page);
    // Return value: errors-flag

    void Compact();

    freespaceset CalculateMapOf(unsigned page) const;
};

#endif
