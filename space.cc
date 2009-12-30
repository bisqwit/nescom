#include <cstdio>

#include "space.hh"
#include "logfiles.hh"
#include "binpacker.hh"
#include "romaddr.hh"

using namespace std;

freespacemap::freespacemap() : quiet(false)
{
}

unsigned freespacemap::Find(unsigned page, unsigned length)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    iterator mapi = find(page);
    if(mapi == end())
    {
        if(!quiet)
        {
            fprintf(stderr,
                "Page %02X is FULL! No %u bytes of free space there!\n",
                page, length);
            if(log)
            fprintf(log,
                "Page %02X is FULL! No %u bytes of free space there!\n",
                page, length);
        }
        return NOWHERE;
    }
    freespaceset &spaceset = mapi->second;
    

    unsigned bestscore = 0;
    unsigned bestpos   = NOWHERE;
    
    for(freespaceset::const_iterator
        reci = spaceset.begin(); reci != spaceset.end(); ++reci)
    {
        const unsigned recpos = reci->lower;
        const unsigned reclen = reci->upper - recpos;
        
        if(reclen == length)
        {
            bestpos = recpos;
            // Found exact match!
            break;
        }
        if(reclen < length)
        {
            // Too small, not good.
            continue;
        }
        
        // The smaller, the better.
        unsigned score = 0x7FFFFFF - reclen;
        
        if(score > bestscore)
        {
            bestscore = score;
            bestpos   = recpos;
            //break;
        }
    }
    if(!bestscore)
    {
        if(!quiet)
        {
            unsigned size = Size(page);
            fprintf(stderr,
                "Can't find %u bytes of free space in page %02X! (Total left: %u)\n",
                length, page, size);
            if(log)
            fprintf(log,
                "Can't find %u bytes of free space in page %02X! (Total left: %u)\n",
                length, page, size);
        }
        return NOWHERE;
    }
    
    spaceset.erase(bestpos, bestpos+length);
    
    return bestpos;
}

void freespacemap::DumpPageMap(unsigned pagenum) const
{
    const_iterator mapi = find(pagenum);
    if(mapi == end())
    {
        fprintf(stderr, "(Page %02X full)\n", pagenum);
        return;
    }

    const freespaceset &spaceset = mapi->second;
    
    fprintf(stderr, "Map of page %02X:\n", pagenum);
    for(freespaceset::const_iterator
        reci = spaceset.begin(); reci != spaceset.end(); ++reci)
    {
        unsigned recpos = reci->lower;
        unsigned reclen = reci->upper - recpos;
        fprintf(stderr, "  %X: %u\n", recpos, reclen);
    }
}

void freespacemap::VerboseDump() const
{
    FILE *log = GetLogFile("mem", "log_addrs");
    if(!log) return; //log = stdout;

    for(const_iterator i = begin(); i != end(); ++i)
    {
        unsigned page = i->first;
        const freespaceset &spaceset = i->second;
        for(freespaceset::const_iterator
            reci = spaceset.begin(); reci != spaceset.end(); ++reci)
        {
            unsigned recpos = reci->lower;
            unsigned reclen = reci->upper - recpos;
            
            unsigned pos = page * GetPageSize() + recpos;
            
            //MarkFree(pos, reclen, "free");
        }
    }
}

void freespacemap::Report() const
{
    fprintf(stderr, "Free space:");
    const_iterator i;
    unsigned total=0;
    for(i=begin(); i!=end(); ++i)
    {
        unsigned thisfree = 0, hunkcount = 0;
        for(freespaceset::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            thisfree += j->length();
            ++hunkcount;
        }
        total += thisfree;
        if(thisfree)
        {
            fprintf(stderr, " %02X:%u/%u", i->first, thisfree, hunkcount);
        }
    }
    fprintf(stderr, " - total: %u bytes\n", total);
}

unsigned freespacemap::Size() const
{
    const_iterator i;
    unsigned total=0;
    for(i=begin(); i!=end(); ++i)
    {
        unsigned thisfree = 0;
        for(freespaceset::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            thisfree += j->length();
        }
        total += thisfree;
    }
    return total;
}

unsigned freespacemap::Size(unsigned page) const
{
    const_iterator i = find(page);
    unsigned total=0;
    if(i != end())
    {
        for(freespaceset::const_iterator j=i->second.begin(); j!=i->second.end(); ++j)
        {
            total += j->length();
        }
    }
    return total;
}

unsigned freespacemap::GetFragmentation(unsigned page) const
{
    const_iterator i = find(page);
    if(i != end()) return i->second.size();
    return 0;
}

const set<unsigned> freespacemap::GetPageList() const
{
    set<unsigned> result;
    for(const_iterator i = begin(); i != end(); ++i)
        result.insert(i->first);
    return result;
}
const freespaceset &freespacemap::GetList(unsigned pagenum) const
{
    return find(pagenum)->second;
}

void freespacemap::Add(unsigned page, unsigned begin, unsigned length)
{
    fprintf(stderr, "Adding %u bytes of free space at %02X:%04X\n", length, page, begin);
    if(begin + length > GetPageSize())
    {
        fprintf(stderr,
            "freespacemap::Add: Error in Add($%02X,$%X, %u): Page is greater than %u bytes!\n",
            page,begin,length, GetPageSize());
    }
    
    operator[] (page).set(begin, begin+length);
    //operator[] (page).compact();
}
void freespacemap::Add(unsigned longaddr, unsigned length)
{
    Add(longaddr / GetPageSize(), longaddr % GetPageSize(), length);
}

void freespacemap::Del(unsigned page, unsigned begin, unsigned length)
{
    //fprintf(stderr, "Deleting %u bytes of free space at %02X:%04X\n", length, page, begin);
    if(begin + length > GetPageSize())
    {
        fprintf(stderr,
            "freespacemap::Del: Error in Del($%02X,$%X, %u): Page is greater than %u bytes!\n",
            page,begin,length, GetPageSize());
    }
    
    iterator i = find(page);
    if(i == end()) return;
    
    freespaceset &spaceset = i->second;
    
    spaceset.erase(begin, begin+length);
    //spaceset.compact();
}
void freespacemap::Del(unsigned longaddr, unsigned length)
{
    Del(longaddr / GetPageSize(), longaddr % GetPageSize(), length);
}

void freespacemap::Compact()
{
    /*for(iterator i = begin(); i != end(); ++i)
        i->second.compact();*/
}

bool freespacemap::Organize(vector<freespacerec> &blocks, unsigned pagenum)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    const_iterator i = find(pagenum);
    if(i == end())
    {
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Page %02X is totally empty.\n", pagenum);
            if(log)
            fprintf(log, "ERROR: Page %02X is totally empty.\n", pagenum);
        }
        return true;
    }
    
    const freespaceset &pagemap = i->second;
    
    vector<unsigned> items;
    vector<unsigned> holes;
    vector<unsigned> holeaddrs;
    
    items.reserve(blocks.size());
    
    unsigned totalsize = 0;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        totalsize += blocks[a].len;
        items.push_back(blocks[a].len);
    }
    
    unsigned totalspace = 0;
    holes.reserve(pagemap.size());
    holeaddrs.reserve(pagemap.size());
    for(freespaceset::const_iterator i = pagemap.begin(); i != pagemap.end(); ++i)
    {
        const unsigned recpos = i->lower;
        const unsigned reclen = i->upper - recpos;
        totalspace += reclen;
        holes.push_back(reclen);
        holeaddrs.push_back(recpos);
    }
    
    if(totalspace < totalsize)
    {
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Page %02X doesn't have %u bytes of space (only %u there)!\n",
                pagenum, totalsize, totalspace);
            if(log)
            fprintf(log, "ERROR: Page %02X doesn't have %u bytes of space (only %u there)!\n",
                pagenum, totalsize, totalspace);
        }
    }
    
    vector<unsigned> organization = PackBins(holes, items);
    
    bool Errors = false;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        unsigned itemsize = blocks[a].len;
        unsigned holeid   = organization[a];
        
        unsigned spaceptr = NOWHERE;
        if(holeid < holes.size()
        && holes[holeid] >= itemsize)
        {
            spaceptr = holeaddrs[holeid];
            holeaddrs[holeid] += itemsize;
            holes[holeid]     -= itemsize;
            Del(pagenum, spaceptr, itemsize);
        }
        else
        {
            Errors = true;
        }
        blocks[a].pos = spaceptr;
    }
    if(Errors)
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Organization to page %02X failed\n", pagenum);
            if(log)
            fprintf(log, "ERROR: Organization to page %02X failed\n", pagenum);
        }
    return Errors;
}

bool freespacemap::OrganizeToAnyPage(vector<freespacerec> &blocks)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    vector<unsigned> items;
    vector<unsigned> holes;
    vector<unsigned> holepages;
    vector<unsigned> holeaddrs;
    
    items.reserve(blocks.size());
    
    unsigned totalsize = 0;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        totalsize += blocks[a].len;
        items.push_back(blocks[a].len);
    }

    unsigned totalspace = 0;
    for(const_iterator i=begin(); i!=end(); ++i)
    {
        unsigned pagenum = i->first;
        const freespaceset &pagemap = i->second;
        for(freespaceset::const_iterator j=pagemap.begin(); j!=pagemap.end(); ++j)
        {
            const unsigned recpos = j->lower;
            const unsigned reclen = j->upper - recpos;
            totalspace += reclen;
            holes.push_back(reclen);
            holeaddrs.push_back(recpos);
            holepages.push_back(pagenum);
        }
    }
    
    if(totalspace < totalsize)
    {
        if(!quiet)
        {
            fprintf(stderr, "ERROR: No %u bytes of space available (only %u there)!\n",
                totalsize, totalspace);
            if(log)
            fprintf(log, "ERROR: No %u bytes of space available (only %u there)!\n",
                totalsize, totalspace);
        }
    }
    
    vector<unsigned> organization = PackBins(holes, items);
    
    bool Errors = false;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        unsigned itemsize = blocks[a].len;
        unsigned holeid   = organization[a];
        
        unsigned spaceptr = NOWHERE;
        if(holes[holeid] >= itemsize)
        {
            unsigned pagenum = holepages[holeid];
            spaceptr = holeaddrs[holeid] + (pagenum * GetPageSize());
            holeaddrs[holeid] += itemsize;
            holes[holeid]     -= itemsize;
            Del(spaceptr, itemsize);
        }
        else
        {
            Errors = true;
        }
        blocks[a].pos = spaceptr;
    }
    if(Errors)
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Organization failed\n");
            if(log)
            fprintf(log, "ERROR: Organization failed\n");
        }
    return Errors;
}

bool freespacemap::OrganizeToAnySamePage(vector<freespacerec> &blocks, unsigned &page)
{
    // To do:
    //   1. Pick a page where they all fit the best
    //   2. Organize there.
    
    freespacemap saved_this = *this;
    quiet = true;
    
    unsigned bestpagenum = 0x00; /* Guess */
    unsigned bestpagesize = 0;
    bool first = true;
    bool candidates = false;
    for(const_iterator i=begin(); i!=end(); ++i)
    {
        unsigned pagenum = i->first;
        
        vector<freespacerec> tmpblocks = blocks;
        
        if(!Organize(tmpblocks, pagenum))
        {
            // candidate!
            
            // Can't use previous reference because things have changed
            // FIXME: Is this really the case?
            const freespaceset &pagemap = find(pagenum)->second;
            
            unsigned freesize = 0;
            for(freespaceset::const_iterator j=pagemap.begin(); j!=pagemap.end(); ++j)
            {
                unsigned reclen = j->length();
                freesize += reclen;
            }

            if(first || freesize < bestpagesize)
            {
                bestpagenum  = pagenum;
                bestpagesize = freesize;
                first = false;
            }
            
            candidates = true;
        }
    }
    
    *this = saved_this;

    page = bestpagenum;
    
    if(!candidates)
    {
        fprintf(stderr, "Warning: All pages seem to be too small\n");
    }

    return Organize(blocks, bestpagenum);
}

unsigned freespacemap::FindFromAnyPage(unsigned length)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    unsigned leastfree=0, bestpage=0x00; /* guess */
    bool first=true;
    for(const_iterator i=begin(); i!=end(); ++i)
    {
        const freespaceset &pagemap = i->second;
        
        for(freespaceset::const_iterator j=pagemap.begin(); j!=pagemap.end(); ++j)
        {
            unsigned reclen = j->length();
            if(reclen < length) continue;
            if(first || reclen < leastfree)
            {
                bestpage  = i->first;
                leastfree = reclen;
                first = false;
            }
        }
    }
    if(first)
    {
        fprintf(stderr, "No %u-byte free space block available!\n", length);
        if(log)
            fprintf(log, "No %u-byte free space block available!\n", length);
        return NOWHERE;
    }
    return Find(bestpage, length) + (bestpage * GetPageSize());
}

#include "o65linker.hh"

void freespacemap::OrganizeO65linker
    (O65linker& objects, const SegmentSelection seg)
{
    Compact();
    
    vector<unsigned> sizes = objects.GetSizeList(seg);
    vector<unsigned> addrs = objects.GetAddrList(seg);
    
    vector<LinkageWish> linkages = objects.GetLinkageList(seg);
    
    map<unsigned, vector<unsigned> > destinies;
    map<unsigned, vector<unsigned> > groups;
    vector<unsigned> items;

    /* All structures are filled at the same time so
     * that we can have this switch() here and see if
     * we missed something.
     */
    for(unsigned a=0; a<linkages.size(); ++a)
        switch(linkages[a].type)
        {
            case LinkageWish::LinkThisPage:
                destinies[linkages[a].GetPage()].push_back(a);
                break;
            case LinkageWish::LinkInGroup:
                groups[linkages[a].GetGroup()].push_back(a);
                break;
            case LinkageWish::LinkAnywhere:
                items.push_back(a);
                break;
            case LinkageWish::LinkHere:
            {
                /* No linking, just ensure we won't overwrite it */
                Del(addrs[a], sizes[a]);
                break;
            }
        }
        
    /* FIRST link those which require specific pages */

    /* Link each page. */
    for(map<unsigned, vector<unsigned> >::const_iterator
        i = destinies.begin(); i != destinies.end(); ++i)
    {
        const unsigned page = i->first;
        const vector<unsigned>& items = i->second;
        
        vector<freespacerec> Organization(items.size());
        
        for(unsigned c=0; c<items.size(); ++c)
            Organization[c].len = sizes[items[c]];
        
        Organize(Organization, page);
        
        for(unsigned c=0; c<items.size(); ++c)
        {
            unsigned addr = Organization[c].pos;
            if(addr != NOWHERE)
            {
                if(seg == CODE) addr = MakeNESaddr(page, addr);
                else addr += page * GetPageSize();
            }
            addrs[items[c]] = addr;
        }
    }
    
    /* SECOND link those which require same pages */

    /* Link each group. */
    for(map<unsigned, vector<unsigned> >::const_iterator
        i = groups.begin(); i != groups.end(); ++i)
    {
        //const unsigned groupnum = i->first; /* unused */
        const vector<unsigned>& items = i->second;
        
        vector<freespacerec> Organization(items.size());

        for(unsigned c=0; c<items.size(); ++c)
            Organization[c].len = sizes[items[c]];
        
        unsigned page = NOWHERE;
        OrganizeToAnySamePage(Organization, page);
        
        for(unsigned c=0; c<items.size(); ++c)
        {
            unsigned addr = Organization[c].pos;
            if(addr != NOWHERE)
            {
                if(seg == CODE) addr = MakeNESaddr(page, addr);
                else addr += page * GetPageSize();
            }
            addrs[items[c]] = addr;
        }
    }
    
    /* LAST link those which go anywhere */

    vector<freespacerec> Organization(items.size());

    for(unsigned c=0; c<items.size(); ++c)
        Organization[c].len = sizes[items[c]];

    OrganizeToAnyPage(Organization);
    
    for(unsigned c=0; c<items.size(); ++c)
    {
        unsigned addr = Organization[c].pos;
        if(addr != NOWHERE)
        {
            if(seg == CODE)
                addr = MakeNESaddr(addr / GetPageSize(), addr % GetPageSize());
        }
        addrs[items[c]] = addr;
    }
    
    /* Everything done. */

    objects.PutAddrList(addrs, seg);
}
