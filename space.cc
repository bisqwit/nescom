#include <cstdio>

#include "space.hh"
#include "logfiles.hh"
#include "binpacker.hh"
#include "romaddr.hh"

freespacemap::freespacemap() : quiet(false)
{
}

unsigned freespacemap::Find(unsigned page, unsigned length)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    auto mapi = data.find(page);
    if(mapi == data.end())
    {
        if(!quiet)
        {
            std::fprintf(stderr,
                "Page %02X is FULL! No %u bytes of free space there!\n",
                page, length);
            if(log)
            std::fprintf(log,
                "Page %02X is FULL! No %u bytes of free space there!\n",
                page, length);
        }
        return NOWHERE;
    }
    freespaceset &spaceset = mapi->second;

    unsigned bestscore = 0;
    unsigned bestpos   = NOWHERE;

    for(auto reci = spaceset.begin(); reci != spaceset.end(); ++reci)
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
            std::fprintf(stderr,
                "Can't find %u bytes of free space in page %02X! (Total left: %u)\n",
                length, page, size);
            if(log)
            std::fprintf(log,
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
    auto mapi = data.find(pagenum);
    if(mapi == data.end())
    {
        std::fprintf(stderr, "(Page %02X full)\n", pagenum);
        return;
    }

    const freespaceset &spaceset = mapi->second;

    std::fprintf(stderr, "Map of page $%02X:\n", pagenum);
    for(auto reci = spaceset.begin(); reci != spaceset.end(); ++reci)
    {
        unsigned recpos = reci->lower;
        unsigned reclen = reci->upper - recpos;
        std::fprintf(stderr, "  $%X: %u\n", recpos, reclen);
    }
}

void freespacemap::VerboseDump() const
{
    /**/
    FILE *log = GetLogFile("mem", "log_addrs");
    if(!log) return; //log = stdout;

    for(auto i = data.begin(); i != data.end(); ++i)
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
    /**/
}

void freespacemap::Report() const
{
    std::fprintf(stderr, "Free space:");
    unsigned total=0;
    for(auto i=data.begin(); i!=data.end(); ++i)
    {
        unsigned thisfree = 0, hunkcount = 0;
        for(auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            thisfree += j->length();
            ++hunkcount;
        }
        total += thisfree;
        if(thisfree)
        {
            std::fprintf(stderr, " %02X:%u/%u", i->first, thisfree, hunkcount);
        }
    }
    std::fprintf(stderr, " - total: %u bytes\n", total);
}

unsigned freespacemap::Size() const
{
    unsigned total=0;
    for(auto i = data.begin(); i != data.end(); ++i)
    {
        unsigned thisfree = 0;
        for(auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            thisfree += j->length();
        }
        total += thisfree;
    }
    return total;
}

unsigned freespacemap::Size(unsigned page) const
{
    unsigned total=0;
    if(auto i = data.find(page); i != data.end())
    {
        for(auto j = i->second.begin(); j != i->second.end(); ++j)
        {
            total += j->length();
        }
    }
    return total;
}

unsigned freespacemap::GetFragmentation(unsigned page) const
{
    if(auto i = data.find(page); i != data.end()) return i->second.size();
    return 0;
}

const std::set<unsigned> freespacemap::GetPageList() const
{
    std::set<unsigned> result;
    for(auto i = data.begin(); i != data.end(); ++i) result.insert(i->first);
    return result;
}
const freespaceset &freespacemap::GetList(unsigned pagenum) const
{
    return data.find(pagenum)->second;
}

void freespacemap::Add(unsigned page, unsigned begin, unsigned length)
{
    //std::fprintf(stderr, "Adding %u bytes of free space at %02X:%04X\n", length, page, begin);
    if(begin + length > GetPageSize())
    {
        std::fprintf(stderr,
            "freespacemap::Add: Error in Add($%02X,$%X, %u): Page is greater than %u bytes!\n",
            page,begin,length, GetPageSize());
    }

    data[page].set(begin, begin+length);
    //data[page].compact();
}
void freespacemap::Add(unsigned longaddr, unsigned length)
{
    Add(longaddr / GetPageSize(), longaddr % GetPageSize(), length);
}

void freespacemap::Del(unsigned page, unsigned begin, unsigned length)
{
    //std::fprintf(stderr, "Deleting %u bytes of free space at %02X:%04X\n", length, page, begin);
    if(begin + length > GetPageSize())
    {
        std::fprintf(stderr,
            "freespacemap::Del: Error in Del($%02X,$%X, %u): Page is greater than %u bytes!\n",
            page,begin,length, GetPageSize());
    }

    unsigned end = begin+length;
    if(auto i = data.find(page); i == data.end())
        return;
    else
    {
        freespaceset &spaceset = i->second;
        spaceset.erase(begin, end);
        //spaceset.compact();
    }

    /* Run through aliases */
    if(auto i = aliases.find(page); i != aliases.end())
        for(const auto& j: i->second)
        {
            unsigned alias_begin = j.first;
            unsigned alias_end   = alias_begin + j.second.length;
            if(alias_begin < end && alias_end > begin)
            {
                unsigned real_bank  = j.second.realpage;
                unsigned real_begin = j.second.realbegin;
                if(auto k = data.find(real_bank); k != data.end())
                {
                    freespaceset &spaceset = k->second;

                    unsigned delete_begin  = std::max(begin, alias_begin);
                    unsigned delete_end    = std::min(end,   alias_end);
                    unsigned delete_amount = delete_end - delete_begin;
                    unsigned skip_begin    = delete_begin - alias_begin;
                    spaceset.erase(real_begin + skip_begin, real_begin + skip_begin + delete_amount);
                }
            }
        }
}
void freespacemap::Del(unsigned longaddr, unsigned length)
{
    Del(longaddr / GetPageSize(), longaddr % GetPageSize(), length);
}

void freespacemap::AddAlias(unsigned aliaspage, unsigned aliasbegin, unsigned aliaslength,
                            unsigned realpage, unsigned realbegin)
{
    aliases[aliaspage][aliasbegin] = alias{aliaslength,realpage,realbegin};
}

void freespacemap::Compact()
{
    /*for(iterator i = begin(); i != end(); ++i)
        i->second.compact();*/
}

freespaceset freespacemap::CalculateMapOf(unsigned pagenum) const
{
    freespaceset pagemap;
    if(auto i = data.find(pagenum); i != data.end())
    {
        pagemap = i->second;
    }
    if(auto i = aliases.find(pagenum); i != aliases.end())
        for(const auto& j: i->second)
        {
            const auto& a = j.second;
            if(auto k = data.find(a.realpage); k != data.end())
            {
                // Find all space within {realbegin, realbegin+length}
                // and insert that into pagemap.
                freespaceset submap = k->second;
                submap.erase_before( a.realbegin );
                submap.erase_after( a.realbegin + a.length );
                for(auto l = submap.begin(); l != submap.end(); ++l)
                {
                    unsigned length = l->upper - l->lower;
                    unsigned base   = l->lower - a.realbegin + j.first;
                    pagemap.set(base, base+length);
                }
            }
        }
    return pagemap;
}

bool freespacemap::Organize(std::vector<freespacerec> &blocks, unsigned pagenum)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    freespaceset pagemap = CalculateMapOf(pagenum);

    if(pagemap.empty())
    {
        if(!quiet)
        {
            std::fprintf(stderr, "ERROR: Page %02X is totally empty.\n", pagenum);
            if(log)
            std::fprintf(log, "ERROR: Page %02X is totally empty.\n", pagenum);
        }
        return true;
    }

    std::vector<unsigned> items;
    std::vector<unsigned> holes;
    std::vector<unsigned> holeaddrs;

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
            std::fprintf(stderr, "ERROR: Page %02X doesn't have %u bytes of space (only %u there)!\n",
                pagenum, totalsize, totalspace);
            if(log)
            std::fprintf(log, "ERROR: Page %02X doesn't have %u bytes of space (only %u there)!\n",
                pagenum, totalsize, totalspace);
        }
    }

    std::vector<unsigned> organization = PackBins(holes, items);

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
            std::fprintf(stderr, "ERROR: Organization to page %02X failed\n", pagenum);
            if(log)
            std::fprintf(log, "ERROR: Organization to page %02X failed\n", pagenum);
        }
    return Errors;
}

bool freespacemap::OrganizeToAnyPage(std::vector<freespacerec> &blocks)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    std::vector<unsigned> items;
    std::vector<unsigned> holes;
    std::vector<unsigned> holepages;
    std::vector<unsigned> holeaddrs;

    items.reserve(blocks.size());

    unsigned totalsize = 0;
    for(unsigned a=0; a<blocks.size(); ++a)
    {
        totalsize += blocks[a].len;
        items.push_back(blocks[a].len);
    }

    // Try twice. First without aliases, then with aliases
    std::set<unsigned> pages;
    for(const auto& d: data) pages.insert(d.first);

    bool Errors = false;
    for(unsigned try_number = 1; try_number <= 2; ++try_number)
    {
        unsigned totalspace = 0;
        if(try_number == 2)
        {
            for(const auto& d: aliases) pages.insert(d.first);
        }
        for(unsigned pagenum: pages)
        {
            freespaceset pagemap = CalculateMapOf(pagenum);
            for(auto j = pagemap.begin(); j != pagemap.end(); ++j)
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
                std::fprintf(stderr, "ERROR: No %u bytes of space available (only %u there)!\n",
                    totalsize, totalspace);
                if(log)
                std::fprintf(log, "ERROR: No %u bytes of space available (only %u there)!\n",
                    totalsize, totalspace);
            }
            if(try_number == 1)
                continue;
        }

        std::vector<unsigned> organization = PackBins(holes, items);

        Errors = false;
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
        if(!Errors) break;
    }
    if(Errors)
        if(!quiet)
        {
            std::fprintf(stderr, "ERROR: Organization failed\n");
            if(log)
            std::fprintf(log, "ERROR: Organization failed\n");
        }
    return Errors;
}

bool freespacemap::OrganizeToAnySamePage(std::vector<freespacerec> &blocks, unsigned &page)
{
    // To do:
    //   1. Pick a page where they all fit the best
    //   2. Organize there.

    freespacemap saved_this = *this;
    quiet = true;

    unsigned bestpagenum = 0xFF; /* Guess */
    unsigned bestpagesize = 0;
    bool first = true;
    bool candidates = false;
    for(auto i = data.begin(); i != data.end(); ++i)
    {
        unsigned pagenum = i->first;

        std::vector<freespacerec> tmpblocks = blocks;

        if(!Organize(tmpblocks, pagenum))
        {
            // candidate!

            // Can't use previous reference because things have changed
            // FIXME: Is this really the case?
            const freespaceset &pagemap = data.find(pagenum)->second;

            unsigned freesize = 0;
            for(auto j = pagemap.begin(); j != pagemap.end(); ++j)
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
        std::fprintf(stderr, "Warning: All pages seem to be too small\n");
    }

    return Organize(blocks, bestpagenum);
}

unsigned freespacemap::FindFromAnyPage(unsigned length)
{
    FILE *log = GetLogFile("mem", "log_addrs");

    unsigned leastfree=0, bestpage=0x00; /* guess */
    bool first=true;
    for(auto i = data.begin(); i != data.end(); ++i)
    {
        const freespaceset &pagemap = i->second;

        for(auto j = pagemap.begin(); j != pagemap.end(); ++j)
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
        std::fprintf(stderr, "No %u-byte free space block available!\n", length);
        if(log)
            std::fprintf(log, "No %u-byte free space block available!\n", length);
        return NOWHERE;
    }
    return Find(bestpage, length) + (bestpage * GetPageSize());
}

#include "o65linker.hh"

void freespacemap::OrganizeO65linker
    (O65linker& objects, const SegmentSelection seg)
{
    Compact();

    std::vector<unsigned> sizes = objects.GetSizeList(seg);
    std::vector<unsigned> addrs = objects.GetAddrList(seg);

    std::vector<LinkageWish> linkages = objects.GetLinkageList(seg);

    std::map<unsigned, std::vector<unsigned> > destinies;
    std::map<unsigned, std::vector<unsigned> > groups;
    std::vector<unsigned> items;

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
    for(auto i = destinies.begin(); i != destinies.end(); ++i)
    {
        const unsigned page = i->first;
        const std::vector<unsigned>& items = i->second;

        // Only try relocating non-empty blobs
        std::vector<freespacerec> Organization;
        for(unsigned c=0; c<items.size(); ++c)
            if(sizes[items[c]])
                Organization.push_back(freespacerec{0u, sizes[items[c]]});

        Organize(Organization, page);

        for(unsigned d=0,c=0; c<items.size(); ++c)
        {
            if(sizes[items[c]] == 0) continue;
            unsigned addr = Organization[d++].pos;
            if(addr != NOWHERE)
            {
                addr += page * GetPageSize();
            }
            addrs[items[c]] = addr;
        }
    }

    /* SECOND link those which require same pages */

    /* Link each group. */
    for(auto i = groups.begin(); i != groups.end(); ++i)
    {
        //const unsigned groupnum = i->first; /* unused */
        const std::vector<unsigned>& items = i->second;

        // Only try relocating non-empty blobs
        std::vector<freespacerec> Organization;
        for(unsigned c=0; c<items.size(); ++c)
            if(sizes[items[c]])
                Organization.push_back(freespacerec{0u, sizes[items[c]]});

        unsigned page = NOWHERE;
        OrganizeToAnySamePage(Organization, page);

        for(unsigned d=0, c=0; c<items.size(); ++c)
        {
            if(sizes[items[c]] == 0) continue;
            unsigned addr = Organization[d++].pos;
            if(addr != NOWHERE)
            {
                addr += page * GetPageSize();
            }
            addrs[items[c]] = addr;
        }
    }

    /* LAST link those which go anywhere */

    // Only try relocating non-empty blobs
    std::vector<freespacerec> Organization;
    for(unsigned c=0; c<items.size(); ++c)
        if(sizes[items[c]])
            Organization.push_back(freespacerec{0u, sizes[items[c]]});

    OrganizeToAnyPage(Organization);

    for(unsigned d=0, c=0; c<items.size(); ++c)
    {
        if(sizes[items[c]] == 0) continue;

        unsigned addr = Organization[d++].pos;
        if(addr != NOWHERE)
        {
            //addr = MakeNESaddr(addr / GetPageSize(), addr % GetPageSize());
        }
        addrs[items[c]] = addr;
    }

    /* Everything done. */
    for(unsigned n=0; n<sizes.size(); ++n)
        std::fprintf(stderr, "Size %04X address = %04X\n", sizes[n], addrs[n]);

    objects.PutAddrList(addrs, seg);
}
