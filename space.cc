#include <cstdio>

#include "space.hh"
#include "binpacker.hh"
#include "romaddr.hh"

using namespace std;

freespacemap::freespacemap() : quiet(false)
{
}

unsigned freespacemap::Find(unsigned page, unsigned length)
{
    iterator mapi = find(page);
    if(mapi == end())
    {
        if(!quiet)
        {
            fprintf(stderr,
                "Page %02X is FULL! No %u bytes of free space there!\n",
                page, length);
        }
        return NOWHERE;
    }
    freespaceset &spaceset = mapi->second;

    unsigned bestscore = 0;
    unsigned bestpos   = NOWHERE;
    
    for(freespaceset::const_iterator reci = spaceset.begin();
        reci != spaceset.end(); ++reci)
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

void freespacemap::Report() const
{
    fprintf(stderr, "Free space:");
    const_iterator i;
    unsigned total=0;
    for(i=begin(); i!=end(); ++i)
    {
        //DumpPageMap(i->first);
        unsigned thisfree = 0, hunkcount = 0;
        for(freespaceset::const_iterator j=i->second.begin();
            j!=i->second.end(); ++j)
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
        for(freespaceset::const_iterator j=i->second.begin();
            j!=i->second.end(); ++j)
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
        for(freespaceset::const_iterator
            j=i->second.begin(); j!=i->second.end(); ++j)
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
    //fprintf(stderr, "Adding %u bytes of free space at %02X:%04X\n", length, page, begin);
    operator[] (page).set(begin, begin+length);
}
void freespacemap::Add(unsigned longaddr, unsigned length)
{
    Add(longaddr >> 16, longaddr & 0xFFFF, length);
}

void freespacemap::Del(unsigned page, unsigned begin, unsigned length)
{
    iterator i = find(page);
    if(i == end()) return;
    
    freespaceset &spaceset = i->second;
    
    spaceset.erase(begin, begin+length);
}
void freespacemap::Del(unsigned longaddr, unsigned length)
{
    Del(longaddr >> 16, longaddr & 0xFFFF, length);
}

void freespacemap::Compact()
{
/*
    for(iterator i = begin(); i != end(); ++i)
        i->second.compact();
*/
}

bool freespacemap::Organize(vector<freespacerec> &blocks, unsigned pagenum)
{
    const_iterator i = find(pagenum);
    if(i == end())
    {
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Page %02X is totally empty.\n", pagenum);
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
        }
    return Errors;
}

bool freespacemap::OrganizeToAnyPage(vector<freespacerec> &blocks)
{
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
        }
    }
    
    vector<unsigned> organization = PackBins(holes, items);
    
    unsigned ErrorSize=0;
    unsigned ErrorCount=0;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        unsigned itemsize = blocks[a].len;
        unsigned holeid   = organization[a];
        
        unsigned spaceptr = NOWHERE;
        if(holes[holeid] >= itemsize)
        {
            unsigned pagenum = holepages[holeid];
            spaceptr = holeaddrs[holeid] | (pagenum << 16);
            holeaddrs[holeid] += itemsize;
            holes[holeid]     -= itemsize;
            Del(spaceptr, itemsize);
        }
        else
        {
            ++ErrorCount;
            ErrorSize += itemsize;
        }
        blocks[a].pos = spaceptr;
    }
    if(ErrorCount != 0)
        if(!quiet)
        {
            fprintf(stderr, "ERROR: Organization failed (%u errors, %u bytes)\n", ErrorCount, ErrorSize);
        }
    return ErrorCount != 0;
}

bool freespacemap::OrganizeToAnySamePage(vector<freespacerec> &blocks, unsigned &page)
{
    // To do:
    //   1. Pick a page where they all fit the best
    //   2. Organize there.
    
    freespacemap saved_this = *this;
    quiet = true;
    
    unsigned bestpagenum = 0x3F; /* Guess */
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
            for(freespaceset::const_iterator j=pagemap.begin();
                j!=pagemap.end(); ++j)
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
    unsigned leastfree=0, bestpage=0x3F; /* guess */
    bool first=true;
    for(const_iterator i=begin(); i!=end(); ++i)
    {
        const freespaceset &pagemap = i->second;
        
        for(freespaceset::const_iterator j=pagemap.begin();
            j!=pagemap.end(); ++j)
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
        return NOWHERE;
    }
    return Find(bestpage, length) | (bestpage << 16);
}

#include "o65linker.hh"

void freespacemap::OrganizeO65linker(O65linker& objects)
{
    Compact();
    
    vector<unsigned> sizes = objects.GetSizeList();
    vector<unsigned> addrs = objects.GetAddrList();
    
    vector<LinkageWish> linkages = objects.GetLinkageList();
    
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
                /* no linking, just convert the address */
                addrs[a] = ROM2NESaddr(addrs[a]);
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
        
        //fprintf(stderr, "Linking %u items to page $%02X:\n", items.size(), page);
        
        vector<freespacerec> Organization(items.size());
        
        for(unsigned c=0; c<items.size(); ++c)
            Organization[c].len = sizes[items[c]];
        
        Organize(Organization, page);
        
        for(unsigned c=0; c<items.size(); ++c)
        {
            unsigned addr = Organization[c].pos;
            if(addr != NOWHERE)
            {
                addr |= (page << 16);
                addr = ROM2NESaddr(addr);
            }
            
            unsigned objno = items[c];
            
            //fprintf(stderr, "%5u: %06X (%s)\n", c, addr, objects.GetName(objno).c_str());
            
            addrs[objno] = addr;
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
        
        //fprintf(stderr, "Linking %u items to page $%02X (group %u)\n", items.size(), page, i->first);
        
        for(unsigned c=0; c<items.size(); ++c)
        {
            unsigned addr = Organization[c].pos;
            if(addr != NOWHERE)
            {
                addr |= (page << 16);
                addr = ROM2NESaddr(addr);
            }
            unsigned objno = items[c];
            
            //fprintf(stderr, "%5u: %06X (%s)\n", c, addr, objects.GetName(objno).c_str());
            
            addrs[objno] = addr;
        }
    }
    
    /* LAST link those which go anywhere */

    //fprintf(stderr, "Linking %u items to various addresses\n", items.size());
    
    vector<freespacerec> Organization(items.size());

    for(unsigned c=0; c<items.size(); ++c)
        Organization[c].len = sizes[items[c]];

    OrganizeToAnyPage(Organization);
    
    for(unsigned c=0; c<items.size(); ++c)
    {
        unsigned addr = Organization[c].pos;
        if(addr != NOWHERE)
        {
            addr = ROM2NESaddr(addr);
        }
        unsigned objno = items[c];
        
        //fprintf(stderr, "%5u: %06X (%s)\n", c, addr, objects.GetName(objno).c_str());
        
        addrs[objno] = addr;
    }
    
    /* Everything done. */

    objects.PutAddrList(addrs);
}
